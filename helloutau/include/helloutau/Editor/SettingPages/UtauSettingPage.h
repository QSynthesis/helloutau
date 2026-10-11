#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_UTAUSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_UTAUSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace hello::daw {

    class AppSettings;

    /// The UTAU folder, and how a relative voice bank path is resolved. The pages of UTAU
    /// compatibility, such as that of the ClassicPluginHost plugin, are its child pages.
    class HELLOUTAU_EDITOR_EXPORT UtauSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit UtauSettingPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QLineEdit *utauDirectoryEdit() const;
        QCheckBox *relativeVoiceDirInUtauBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        void showVoiceFolders();

        AppSettings &m_settings;
        QPointer<QLineEdit> m_utauDirectory;
        QPointer<QCheckBox> m_relativeVoiceDirInUtau;
        QPointer<QLabel> m_voiceFolders;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_UTAUSETTINGPAGE_H
