#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_UPDATESETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_UPDATESETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QSpinBox;
class QLabel;

namespace hello::daw {

    class AppSettings;
    class UpdateChecker;

    /// The check for newer releases and the extraction of an update, under System Settings as the
    /// updates of JetBrains IDEs. The check of Check Now uses the host selected on the page,
    /// whether applied or not.
    class HELLOUTAU_EDITOR_EXPORT UpdateSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.Updates";

        UpdateSettingPage(AppSettings &settings, UpdateChecker &checker, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QComboBox *modeBox() const;
        QComboBox *sourceBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        // Returns the source of the settings, or an empty string for the automatic selection if
        // the update configuration has no source of that name
        QString storedSource() const;
        void showLastCheck();

        AppSettings &m_settings;
        UpdateChecker &m_checker;
        QPointer<QComboBox> m_mode;
        QPointer<QComboBox> m_source;
        QPointer<QSpinBox> m_extractionThreads;
        QPointer<QLabel> m_lastCheck;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_UPDATESETTINGPAGE_H
