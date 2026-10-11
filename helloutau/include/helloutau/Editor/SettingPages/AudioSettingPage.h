#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_AUDIOSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_AUDIOSETTINGPAGE_H

#include <QtCore/QByteArray>
#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;

namespace hello::daw {

    class AppSettings;

    class HELLOUTAU_EDITOR_EXPORT AudioSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.Audio";

        explicit AudioSettingPage(AppSettings &settings, QObject *parent = nullptr);
        bool isModified() const override;
        bool apply(QString *error) override;

    protected:
        QWidget *createWidget() override;

    private:
        // Fills the box with System default and the output devices, and selects \a current, or
        // System default if \a current is absent
        void fillDevices(const QByteArray &current);

        AppSettings &m_settings;
        QPointer<QComboBox> m_output;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_AUDIOSETTINGPAGE_H
