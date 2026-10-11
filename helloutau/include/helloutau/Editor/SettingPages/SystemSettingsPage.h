#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;

namespace hello::daw {

    class AppSettings;

    /// The language of the interface, under Appearance & Behavior as the system settings of
    /// JetBrains IDEs. A changed language takes effect at the next start, so applying it marks a
    /// restart as required (kit::RestartScheduler::requireRestart()).
    class HELLOUTAU_EDITOR_EXPORT SystemSettingsPage : public SettingPage {
        Q_OBJECT
    public:
        explicit SystemSettingsPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QComboBox *languageBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QComboBox> m_language;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_SYSTEMSETTINGSPAGE_H
