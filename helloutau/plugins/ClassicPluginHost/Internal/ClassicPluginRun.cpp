#include "ClassicPluginRun.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSettings>
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

        // The programs that the user allowed to run, by the folder of their plugin: a file of
        // its own beside the settings of the editor
        QSettings approvals() {
            return QSettings(QSettings::IniFormat, QSettings::UserScope,
                             QCoreApplication::organizationName(),
                             QStringLiteral("ClassicPluginHost"));
        }

        QString keyOf(const ClassicPlugin &plugin) {
            return QStringLiteral("approved/") +
                   QString::fromLatin1(QCryptographicHash::hash(textOf(plugin.folder).toUtf8(),
                                                                QCryptographicHash::Sha1)
                                           .toHex());
        }

        // The content of the program, so that a changed program is asked for again
        QString fingerprintOf(const ClassicPlugin &plugin) {
            QFile file(QString::fromStdU16String(plugin.program.u16string()));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (file.open(QIODevice::ReadOnly)) {
                hash.addData(&file);
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        // Asks the user to allow a program that has not run before, or that has changed since.
        bool approve(QWidget *parent, const ClassicPlugin &plugin) {
            auto settings = approvals();
            const auto key = keyOf(plugin);
            const auto fingerprint = fingerprintOf(plugin);
            if (settings.value(key).toString() == fingerprint) {
                return true;
            }
            const auto question =
                settings.contains(key)
                    ? ClassicPluginRun::tr("The program of the plugin \"%1\" has changed since it "
                                           "last ran. It runs this program:")
                    : ClassicPluginRun::tr("The plugin \"%1\" has not run before. It runs this "
                                           "program:");
            const auto answer = QMessageBox::question(
                parent, ClassicPluginRun::tr("Run Plugin"),
                question.arg(plugin.name) + QStringLiteral("\n\n") + textOf(plugin.program) +
                    QStringLiteral("\n\n") +
                    ClassicPluginRun::tr("Run it only if you trust where it comes from."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return false;
            }
            settings.setValue(key, fingerprint);
            return true;
        }

        QString messagesOf(const kit::DiagnosticList &diagnostics) {
            QStringList messages;
            for (const auto &diagnostic : diagnostics) {
                messages.push_back(diagnostic.message);
            }
            return messages.join(QLatin1Char('\n'));
        }

        // Shows that the plugin runs until it ends, with Cancel, and with Done where its end
        // cannot be observed. Returns whether it ended rather than being cancelled.
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

        // The selection as UTAU has it, from the first selected note to the last
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
