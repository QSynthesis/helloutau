#include "ClassicPluginSettingPage.h"

#include <algorithm>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/TextCodec.h>

#include <helloutau/Editor/Dialogs/DiagnosticBox.h>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginPreviewInstallation.h>

#include "ClassicPluginContribution.h"

namespace hello::daw {

    namespace {

        using Installation = ClassicPluginPreviewInstallation;

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

    }

    ClassicPluginSettingPage::ClassicPluginSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Classic Plugins"));
        setDescription(tr("The folders in which the plugins of UTAU are discovered, and the "
                          "preview program of the plugins."));
        setKeywords({QStringLiteral("Classic Plugins"), QStringLiteral("UTAU"),
                     QStringLiteral("Plugins"), QStringLiteral("playinplugin")});
    }

    QWidget *ClassicPluginSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto group = new QGroupBox(tr("Plugin folders"), widget);
        auto groupLayout = new QVBoxLayout(group);
        auto note = new QLabel(tr("The plugins are discovered in these folders, in this order. A "
                                  "folder that does not exist is skipped. The second folder is "
                                  "the plugins folder of the UTAU folder on the UTAU page."),
                               group);
        note->setWordWrap(true);
        groupLayout->addWidget(note);
        m_folders = new QWidget(group);
        auto foldersLayout = new QVBoxLayout(m_folders);
        foldersLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->addWidget(m_folders);
        fillFolders();
        layout->addWidget(group);

        auto previews = new QGroupBox(tr("Preview program"), widget);
        auto previewsLayout = new QVBoxLayout(previews);
        auto previewNote = new QLabel(
            tr("Some plugins play their notes through the program %1 in their folder. The preview "
               "program of HelloUtau replaces it, so that the notes are rendered and played in "
               "HelloUtau. The original program is kept, and still runs if UTAU runs the plugin.")
                .arg(Installation::programName),
            previews);
        previewNote->setWordWrap(true);
        previewsLayout->addWidget(previewNote);
        m_previews = new QWidget(previews);
        auto rowsLayout = new QVBoxLayout(m_previews);
        rowsLayout->setContentsMargins(0, 0, 0, 0);
        previewsLayout->addWidget(m_previews);
        fillPreviews();
        layout->addWidget(previews);

        layout->addStretch();
        return widget;
    }

    void ClassicPluginSettingPage::settingsApplied() {
        fillFolders();
        fillPreviews();
    }

    void ClassicPluginSettingPage::fillFolders() {
        if (!m_folders) {
            return;
        }
        qDeleteAll(m_folders->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
        for (const auto &folder : ClassicPluginContribution::pluginFolders(m_settings)) {
            auto row = new QWidget(m_folders);
            auto rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto path = new QLineEdit(
                QDir::toNativeSeparators(QString::fromStdU16String(folder.u16string())), row);
            path->setReadOnly(true);
            auto open = new QPushButton(tr("Open"), row);
            connect(open, &QPushButton::clicked, row, [this, open, folder] {
                ClassicPluginContribution::openFolder(open, m_settings, folder);
            });
            rowLayout->addWidget(path, 1);
            rowLayout->addWidget(open);
            m_folders->layout()->addWidget(row);
        }
    }

    namespace {

        // Shows a dialog in which the user chooses the encoding of the plugin.txt in folder,
        // which is transcribed into a plugin.json, in the form of the dialog in which the user
        // chooses the encoding of a UST (UstDecodingDialog): the candidates, gray if the file has
        // invalid byte sequences in them, and the file decoded in the one selected. Returns
        // std::nullopt if the user cancels.
        std::optional<QString> promptForCharset(QWidget *parent,
                                                const std::filesystem::path &folder) {
            QList<QByteArray> lines;
            QFile file(textOf(folder / u"plugin.txt"));
            if (file.open(QIODevice::ReadOnly)) {
                for (const auto &line : file.readAll().split('\n')) {
                    if (const auto trimmed = line.trimmed(); !trimmed.isEmpty()) {
                        lines.push_back(trimmed);
                    }
                }
            }

            QDialog dialog(parent);
            dialog.setWindowTitle(
                ClassicPluginSettingPage::tr("Choose Encoding - %1")
                    .arg(QString::fromStdU16String(folder.filename().u16string())));
            const auto charsets = new QListWidget(&dialog);
            charsets->addItems(kit::TextCodec::ustCandidates());
            const auto preview = new QPlainTextEdit(&dialog);
            preview->setReadOnly(true);
            const auto gray = dialog.palette().brush(QPalette::Disabled, QPalette::Text);
            for (int i = 0; i < charsets->count(); ++i) {
                const auto item = charsets->item(i);
                const kit::TextCodec codec(item->text());
                const bool readable =
                    std::all_of(lines.cbegin(), lines.cend(), [&](const QByteArray &line) {
                        return codec.decode(line).has_value();
                    });
                item->setForeground(readable ? QBrush() : gray);
            }
            const auto showText = [&] {
                const auto item = charsets->currentItem();
                if (!item) {
                    preview->clear();
                    return;
                }
                const kit::TextCodec codec(item->text());
                QStringList text;
                for (const auto &line : lines) {
                    const auto decoded = codec.decode(line);
                    text.push_back(
                        decoded ? *decoded
                                : ClassicPluginSettingPage::tr("(not valid in this encoding)"));
                }
                preview->setPlainText(text.join(QLatin1Char('\n')));
            };
            QObject::connect(charsets, &QListWidget::currentRowChanged, &dialog, showText);
            const auto best =
                kit::TextCodec::ranked(lines, kit::TextCodec::ustCandidates()).value(0);
            if (const auto items = charsets->findItems(best, Qt::MatchExactly); !items.isEmpty()) {
                charsets->setCurrentItem(items.first());
            } else {
                charsets->setCurrentRow(0);
            }
            showText();

            const auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
            buttons->addButton(ClassicPluginSettingPage::tr("Install"),
                               QDialogButtonBox::AcceptRole);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            const auto content = new QHBoxLayout();
            content->addWidget(charsets, 1);
            content->addWidget(preview, 3);
            const auto layout = new QVBoxLayout(&dialog);
            const auto note = new QLabel(
                ClassicPluginSettingPage::tr(
                    "The plugin.txt of this plugin does not state its encoding. Choose the "
                    "encoding in which its text is displayed correctly. For the "
                    "installation, the file is transcribed into a plugin.json in UTF-8."),
                &dialog);
            note->setWordWrap(true);
            layout->addWidget(note);
            layout->addLayout(content);
            layout->addWidget(buttons);
            dialog.resize(640, 400);
            if (dialog.exec() != QDialog::Accepted || !charsets->currentItem()) {
                return std::nullopt;
            }
            return charsets->currentItem()->text();
        }

    }

    void ClassicPluginSettingPage::fillPreviews() {
        if (!m_previews) {
            return;
        }
        qDeleteAll(m_previews->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
        const auto program = Installation::bundledProgram();
        if (program.empty()) {
            auto missing =
                new QLabel(tr("The preview program %1 is not in the folder of HelloUtau.")
                               .arg(Installation::bundledName),
                           m_previews);
            missing->setWordWrap(true);
            m_previews->layout()->addWidget(missing);
            return;
        }
        kit::DiagnosticList diagnostics;
        const auto plugins = ClassicPlugin::discover(
            ClassicPluginContribution::pluginFolders(m_settings), diagnostics);
        // The folders that cannot be read, such as one in the Documents folder that macOS keeps
        // from the application, above the plugins of the other folders
        for (const auto &diagnostic : std::as_const(diagnostics)) {
            auto label = new QLabel(diagnostic.message, m_previews);
            label->setWordWrap(true);
            label->setTextFormat(Qt::PlainText);
            m_previews->layout()->addWidget(label);
        }
        if (plugins.isEmpty()) {
            m_previews->layout()->addWidget(new QLabel(tr("No plugin is found."), m_previews));
            return;
        }

        // The plugins of the buttons for all plugins. Install All installs or updates the preview
        // program in the recommended plugins (Installation::isRecommended()), and Remove All
        // removes every installation.
        QList<ClassicPlugin> installable;
        QList<ClassicPlugin> outdated;
        QList<ClassicPlugin> installed;
        for (const auto &plugin : plugins) {
            const auto state = Installation::stateOf(plugin.folder, program);
            if (Installation::isRecommended(plugin.folder, program)) {
                (state == Installation::Outdated ? outdated : installable).push_back(plugin);
            }
            if (state != Installation::Absent) {
                installed.push_back(plugin);
            }
        }
        auto all = new QWidget(m_previews);
        auto allLayout = new QHBoxLayout(all);
        allLayout->setContentsMargins(0, 0, 0, 0);
        auto installAll = new QPushButton(tr("Install All"), all);
        installAll->setToolTip(tr("Installs the preview program in the plugins whose folder has a "
                                  "%1, and updates the older versions.")
                                   .arg(Installation::programName));
        installAll->setEnabled(!installable.isEmpty() || !outdated.isEmpty());
        auto removeAll = new QPushButton(tr("Remove All"), all);
        removeAll->setToolTip(tr("Removes the preview program from every plugin."));
        removeAll->setEnabled(!installed.isEmpty());
        allLayout->addWidget(installAll);
        allLayout->addWidget(removeAll);
        allLayout->addStretch();
        m_previews->layout()->addWidget(all);
        const auto recommended = int(installable.size() + outdated.size());
        auto recommendation = new QLabel(
            recommended > 0
                ? tr("%n plugin(s) below come with a %1, which they start for a preview, and are "
                     "recommended. Install All installs the preview program in exactly these "
                     "plugins, or updates its older version.",
                     nullptr, recommended)
                      .arg(Installation::programName)
                : tr("No plugin is recommended. A plugin that comes with a %1, which it starts for "
                     "a preview, is recommended.")
                      .arg(Installation::programName),
            m_previews);
        recommendation->setWordWrap(true);
        m_previews->layout()->addWidget(recommendation);

        // Adds the failures of plugin to failures, each with the name of the plugin.
        const auto collect = [](kit::DiagnosticList &failures, const ClassicPlugin &plugin,
                                const kit::DiagnosticList &diagnostics) {
            for (auto diagnostic : diagnostics) {
                diagnostic.message = tr("%1: %2").arg(plugin.name, diagnostic.message);
                failures.push_back(diagnostic);
            }
        };
        connect(installAll, &QPushButton::clicked, all, [=] {
            kit::DiagnosticList failures;
            for (const auto &plugin : installable) {
                QString charset;
                if (Installation::transcribes(plugin.folder)) {
                    // A plugin whose encoding the user does not confirm is skipped.
                    const auto chosen = promptForCharset(all, plugin.folder);
                    if (!chosen) {
                        continue;
                    }
                    charset = *chosen;
                }
                kit::DiagnosticList diagnostics;
                if (!Installation::install(plugin.folder, program, charset, diagnostics)) {
                    collect(failures, plugin, diagnostics);
                }
            }
            for (const auto &plugin : outdated) {
                kit::DiagnosticList diagnostics;
                if (!Installation::update(plugin.folder, program, diagnostics)) {
                    collect(failures, plugin, diagnostics);
                }
            }
            if (!failures.isEmpty()) {
                DiagnosticBox::report(all, tr("Install All"), failures);
            }
            QMetaObject::invokeMethod(this, [this] { fillPreviews(); }, Qt::QueuedConnection);
        });
        connect(removeAll, &QPushButton::clicked, all, [=] {
            if (QMessageBox::question(all, tr("Remove All"),
                                      tr("Remove the preview program of HelloUtau from every "
                                         "plugin, and restore their original programs?")) !=
                QMessageBox::Yes) {
                return;
            }
            kit::DiagnosticList failures;
            for (const auto &plugin : installed) {
                kit::DiagnosticList diagnostics;
                if (!Installation::remove(plugin.folder, diagnostics)) {
                    collect(failures, plugin, diagnostics);
                }
            }
            if (!failures.isEmpty()) {
                DiagnosticBox::report(all, tr("Remove All"), failures);
            }
            QMetaObject::invokeMethod(this, [this] { fillPreviews(); }, Qt::QueuedConnection);
        });

        for (const auto &plugin : plugins) {
            auto row = new QWidget(m_previews);
            auto rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto name = new QLabel(plugin.name, row);
            name->setToolTip(textOf(plugin.folder));
            const auto state = Installation::stateOf(plugin.folder, program);
            const bool recommended = Installation::isRecommended(plugin.folder, program);
            auto status = new QLabel(row);
            switch (state) {
                case Installation::Absent:
                    status->setText(recommended ? tr("Not installed, recommended")
                                                : tr("Not installed"));
                    if (recommended) {
                        status->setToolTip(tr("The folder of the plugin has a %1, which the "
                                              "plugin starts for a preview.")
                                               .arg(Installation::programName));
                    }
                    break;
                case Installation::Installed:
                    status->setText(tr("Installed"));
                    break;
                case Installation::Outdated:
                    status->setText(recommended ? tr("Installed, older version, update recommended")
                                                : tr("Installed, older version"));
                    break;
            }
            if (recommended) {
                auto font = status->font();
                font.setBold(true);
                status->setFont(font);
            }
            rowLayout->addWidget(name, 1);
            rowLayout->addWidget(status);

            const auto folder = plugin.folder;
            const auto title = plugin.name;
            // Runs action, reports its failure, and shows the new states afterwards.
            const auto perform = [this, row, title](auto action) {
                kit::DiagnosticList failures;
                if (!action(failures)) {
                    DiagnosticBox::report(row, title, failures);
                }
                QMetaObject::invokeMethod(this, [this] { fillPreviews(); }, Qt::QueuedConnection);
            };
            if (state == Installation::Absent) {
                auto install = new QPushButton(tr("Install"), row);
                connect(install, &QPushButton::clicked, row, [=] {
                    QString charset;
                    if (Installation::transcribes(folder)) {
                        const auto chosen = promptForCharset(row, folder);
                        if (!chosen) {
                            return;
                        }
                        charset = *chosen;
                    } else if (QMessageBox::question(
                                   row, title,
                                   tr("Install the preview program of HelloUtau in the folder of "
                                      "the plugin?")) != QMessageBox::Yes) {
                        return;
                    }
                    perform([&](kit::DiagnosticList &failures) {
                        return Installation::install(folder, program, charset, failures);
                    });
                });
                rowLayout->addWidget(install);
            }
            if (state == Installation::Outdated) {
                auto update = new QPushButton(tr("Update"), row);
                connect(update, &QPushButton::clicked, row, [=] {
                    perform([&](kit::DiagnosticList &failures) {
                        return Installation::update(folder, program, failures);
                    });
                });
                rowLayout->addWidget(update);
            }
            if (state != Installation::Absent) {
                auto remove = new QPushButton(tr("Remove"), row);
                connect(remove, &QPushButton::clicked, row, [=] {
                    if (QMessageBox::question(row, title,
                                              tr("Remove the preview program of HelloUtau and "
                                                 "restore the original program of the plugin?")) !=
                        QMessageBox::Yes) {
                        return;
                    }
                    perform([&](kit::DiagnosticList &failures) {
                        return Installation::remove(folder, failures);
                    });
                });
                rowLayout->addWidget(remove);
            }
            m_previews->layout()->addWidget(row);
        }
    }

}
