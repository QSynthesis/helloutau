#include "ClassicPluginRun.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginExchange.h>
#include <ClassicPluginHost/ClassicPluginRunner.h>

namespace hello::daw {

    namespace {

        // The translation context of the messages of a run
        class ClassicPluginRun {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginRun)
        };

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The ID of this plugin, the key of its values in the plugin settings
        const char pluginId[] = "org.helloutau.classicpluginhost";

        // The programs approved by the user. Each entry records the plugin folder, the path of
        // the program relative to the folder for human readers, and the fingerprint of the
        // program. Only the fingerprint is compared.
        //
        //     "approved": [{"folder": "C:\\UTAU\\plugins\\Foo", "relativePath": "foo.exe",
        //                   "sha256": "<SHA-256>"}]
        const char approvedKey[] = "approved";
        const char folderKey[] = "folder";
        const char relativePathKey[] = "relativePath";
        const char sha256Key[] = "sha256";

        // Returns the path of the program relative to the plugin folder. ClassicPlugin::program
        // always lies inside the folder.
        std::filesystem::path programInFolder(const ClassicPlugin &plugin) {
            std::error_code error;
            auto relative = std::filesystem::relative(plugin.program, plugin.folder, error);
            return error || relative.empty() ? plugin.program.filename() : relative;
        }

        // Returns the SHA-256 of the program content, so that a changed program requires
        // approval again.
        QString fingerprintOf(const ClassicPlugin &plugin) {
            QFile file(QString::fromStdU16String(plugin.program.u16string()));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (file.open(QIODevice::ReadOnly)) {
                hash.addData(&file);
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        // Asks the user to approve a program that has not run before or has changed since its
        // last run, and returns whether the program is approved. Without a loader, which stores
        // the answers, the user is asked each time.
        bool approve(QWidget *parent, const ClassicPlugin &plugin) {
            const auto loader = AppLoader::instance();
            auto approved =
                loader ? loader->pluginValue(QLatin1String(pluginId), QLatin1String(approvedKey))
                             .toArray()
                       : QJsonArray();
            const auto folder = textOf(plugin.folder);
            const auto fingerprint = fingerprintOf(plugin);
            qsizetype index = -1;
            for (qsizetype i = 0; i < approved.size(); ++i) {
                if (approved[i].toObject().value(QLatin1String(folderKey)).toString() == folder) {
                    index = i;
                    break;
                }
            }
            if (index >= 0 &&
                approved[index].toObject().value(QLatin1String(sha256Key)).toString() ==
                    fingerprint) {
                return true;
            }
            const auto question =
                index >= 0
                    ? ClassicPluginRun::tr("The program of the plugin \"%1\" has changed since its "
                                           "last run. The plugin runs the following program:")
                    : ClassicPluginRun::tr("The plugin \"%1\" has not run before. The plugin runs "
                                           "the following program:");
            const auto answer = QMessageBox::question(
                parent, ClassicPluginRun::tr("Run Plugin"),
                question.arg(plugin.name) + QStringLiteral("\n\n") + textOf(plugin.program) +
                    QStringLiteral("\n\n") +
                    ClassicPluginRun::tr("Run the program only if its source is trusted."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return false;
            }
            const QJsonObject entry{
                {QLatin1String(folderKey),       folder                         },
                {QLatin1String(relativePathKey), textOf(programInFolder(plugin))},
                {QLatin1String(sha256Key),       fingerprint                    }
            };
            if (index >= 0) {
                approved[index] = entry;
            } else {
                approved.push_back(entry);
            }
            if (loader) {
                loader->setPluginValue(QLatin1String(pluginId), QLatin1String(approvedKey),
                                       approved);
            }
            return true;
        }

        QString messagesOf(const kit::DiagnosticList &diagnostics) {
            QStringList messages;
            for (const auto &diagnostic : diagnostics) {
                messages.push_back(diagnostic.message);
            }
            return messages.join(QLatin1Char('\n'));
        }

        // Shows a modal dialog until the run ends, with Cancel, and with Done if the end of the
        // program cannot be observed. Returns whether the run ended without cancellation.
        bool waitFor(QWidget *parent, const ClassicPlugin &plugin, ClassicPluginRunner &runner) {
            QDialog dialog(parent);
            dialog.setWindowTitle(plugin.name);
            const auto layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel(
                runner.isUnobservable()
                    ? ClassicPluginRun::tr("The plugin \"%1\" runs in another program. Click Done "
                                           "when it has finished.")
                          .arg(plugin.name)
                    : ClassicPluginRun::tr("The plugin \"%1\" is running.").arg(plugin.name)));
            const auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
            if (runner.isUnobservable()) {
                buttons->addButton(ClassicPluginRun::tr("Done"), QDialogButtonBox::AcceptRole);
            }
            layout->addWidget(buttons);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &runner,
                             &ClassicPluginRunner::finish);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            QObject::connect(&runner, &ClassicPluginRunner::finished, &dialog, &QDialog::accept);
            dialog.exec();
            if (runner.isRunning()) {
                runner.cancel();
            }
            return !runner.isCancelled();
        }

    }

    void runClassicPlugin(ProjectWindow *window, const ClassicPlugin &plugin) {
        const auto document = window->document();
        const auto project = document->session()->snapshot();
        const int size = int(project.tracks.first().notes.size());

        // A contiguous selection as in UTAU, from the first selected note to the last
        int first = 0;
        int count = size;
        if (!plugin.wholeTrack) {
            const auto selected = window->pianoRoll()->selectedIndices();
            if (selected.isEmpty()) {
                QMessageBox::information(window, plugin.name,
                                         ClassicPluginRun::tr("Select the notes for the plugin."));
                return;
            }
            first = selected.first();
            count = selected.last() - first + 1;
        }

        if (!approve(window, plugin)) {
            return;
        }

        ClassicPluginExchange::Paths paths;
        paths.project = document->filePath();
        paths.voiceDirectory =
            project.tracks.first().voiceDirectory(window->editor()->settings().utauDirectory());
        if (!paths.project.empty()) {
            paths.cacheDirectory = kit::Project::cacheDirectoryOf(paths.project);
        }
        const auto bank = document->voiceBank();
        const auto input =
            ClassicPluginExchange::input(plugin, project, first, count, paths, bank.get());

        ClassicPluginRunner runner;
        QString error;
        if (!runner.start(plugin, input, &error)) {
            QMessageBox::critical(window, plugin.name, error);
            return;
        }
        if (!waitFor(window, plugin, runner) || runner.isUnchanged()) {
            return;
        }

        kit::DiagnosticList diagnostics;
        const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
        const auto outcome =
            ClassicPluginExchange::apply(plugin, notes, first, count, runner.result(), diagnostics);
        if (outcome == ClassicPluginExchange::Failed) {
            QMessageBox::critical(window, plugin.name,
                                  ClassicPluginRun::tr("The result of the plugin could not be "
                                                       "applied.") +
                                      QStringLiteral("\n\n") + messagesOf(diagnostics));
        } else if (!diagnostics.isEmpty()) {
            QMessageBox::warning(window, plugin.name, messagesOf(diagnostics));
        }
    }

}
