#include "FolderAccessSettingPage.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/PlatformFileSystem.h>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    namespace {

        QString stateText(kit::FolderAccessRecord::State state) {
            switch (state) {
                case kit::FolderAccessRecord::Missing:
                    return FolderAccessSettingPage::tr("Does not exist");
                case kit::FolderAccessRecord::Readable:
                    return FolderAccessSettingPage::tr("Readable");
                case kit::FolderAccessRecord::Writable:
                    return FolderAccessSettingPage::tr("Readable and writable");
                case kit::FolderAccessRecord::Unreadable:
                    return FolderAccessSettingPage::tr("Cannot be read");
                case kit::FolderAccessRecord::Unwritable:
                    return FolderAccessSettingPage::tr("Cannot be written");
            }
            return {};
        }

    }

    FolderAccessSettingPage::FolderAccessSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Folder Access"));
        setDescription(tr("The access of the folders of the voice banks, the plugins and the "
                          "translations."));
        setKeywords({QStringLiteral("Folder Access"), QStringLiteral("Documents"),
                     QStringLiteral("Permissions")});
    }

    QWidget *FolderAccessSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto text = tr("The folders are checked when %1 starts and when the UTAU folder changes. "
                       "Check Again also checks whether the folders into which voice banks and "
                       "plugins are installed can be written.")
                        .arg(QCoreApplication::applicationName());
#if defined(Q_OS_MACOS)
        text += QLatin1Char('\n') + tr("If a folder in the Documents folder cannot be read, allow "
                                       "the access in System Settings, Privacy & Security, Files "
                                       "and Folders.");
#elif defined(Q_OS_WINDOWS)
        text += QLatin1Char('\n') +
                tr("If a folder in the Documents folder cannot be written, allow %1 in Windows "
                   "Security, Virus & threat protection, Ransomware protection, Allow an app "
                   "through Controlled folder access.")
                    .arg(QCoreApplication::applicationName());
#endif
        layout->addWidget(note(text));

        m_folders = new QTreeWidget(widget);
        m_folders->setObjectName(QStringLiteral("folderList"));
        m_folders->setRootIsDecorated(false);
        m_folders->setHeaderLabels({tr("Folder"), tr("Access"), tr("Reason")});
        m_folders->header()->setSectionResizeMode(QHeaderView::Interactive);
        layout->addWidget(m_folders, 1);

        auto checkAgainButton = new QPushButton(tr("Check &Again"), widget);
        checkAgainButton->setObjectName(QStringLiteral("checkAgain"));
        connect(checkAgainButton, &QPushButton::clicked, this,
                &FolderAccessSettingPage::checkAgain);
        auto buttons = new QHBoxLayout();
        buttons->addStretch();
        buttons->addWidget(checkAgainButton);
        layout->addLayout(buttons);

        // The folders of a UTAU folder that changed since the last check are recorded as well.
        if (const auto record = kit::FolderAccessRecord::instance()) {
            record->check(m_settings.readFolders());
        }
        showRecord();
        return widget;
    }

    void FolderAccessSettingPage::settingsApplied() {
        showRecord();
    }

    void FolderAccessSettingPage::checkAgain() {
        if (const auto record = kit::FolderAccessRecord::instance()) {
            record->check(m_settings.readFolders());
            record->check(m_settings.writtenFolders(), true);
        }
        showRecord();
    }

    QTreeWidget *FolderAccessSettingPage::folderList() const {
        return m_folders;
    }

    void FolderAccessSettingPage::showRecord() {
        if (!m_folders) {
            return;
        }
        // Without a record, as in a test, the folders are checked for the list alone.
        std::vector<kit::FolderAccessRecord::Entry> entries;
        if (const auto record = kit::FolderAccessRecord::instance()) {
            entries = record->entries();
        } else {
            for (const auto &folder : m_settings.readFolders()) {
                entries.push_back(kit::FolderAccessRecord::checked(folder));
            }
        }
        m_folders->clear();
        for (const auto &entry : entries) {
            auto item = new QTreeWidgetItem(m_folders);
            item->setText(
                0, QDir::toNativeSeparators(QString::fromStdU16String(entry.folder.u16string())));
            item->setText(1, stateText(entry.state));
            item->setText(2, entry.reason);
            item->setToolTip(2, entry.reason);
        }
        m_folders->resizeColumnToContents(0);
        m_folders->resizeColumnToContents(1);
    }

}
