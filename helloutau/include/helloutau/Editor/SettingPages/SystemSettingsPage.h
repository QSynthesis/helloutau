#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;

namespace hello::daw {

    class AppSettings;

    /// The language of the interface and the threads of the general tasks of the editor
    /// (TaskSettings), under Appearance & Behavior as the system settings of JetBrains IDEs. A
    /// changed language takes effect at the next start, so applying it marks a restart as
    /// required (kit::ExitLauncher::requireRestart()).
    class HELLOUTAU_EDITOR_EXPORT SystemSettingsPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.SystemSettings";

        explicit SystemSettingsPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QComboBox *languageBox() const;
        /// The box of TaskSettings::threadCount()
        QComboBox *taskThreadsBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QComboBox> m_language;
        QPointer<QComboBox> m_taskThreads;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H
