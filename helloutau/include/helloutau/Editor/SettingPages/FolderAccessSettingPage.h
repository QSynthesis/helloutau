#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_FOLDERACCESSSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_FOLDERACCESSSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QTreeWidget;

namespace hello::daw {

    class AppSettings;

    /// The recorded access of the folders that the application reads, under System Settings,
    /// with the check again, which also checks the writing of the folders that the application
    /// writes. The page has no settings to apply. See docs/FolderAccess.md.
    class HELLOUTAU_EDITOR_EXPORT FolderAccessSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.FolderAccess";

        explicit FolderAccessSettingPage(AppSettings &settings, QObject *parent = nullptr);

        void settingsApplied() override;

        /// Checks the read folders and the written folders of the settings again, and shows the
        /// record.
        void checkAgain();

        QTreeWidget *folderList() const;

    protected:
        QWidget *createWidget() override;

    private:
        void showRecord();

        AppSettings &m_settings;
        QPointer<QTreeWidget> m_folders;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_FOLDERACCESSSETTINGPAGE_H
