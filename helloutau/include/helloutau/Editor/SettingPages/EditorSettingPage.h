#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;

namespace hello::daw {

    class AppSettings;

    /// The encoding in which a UST is exported, and later the other settings of editing.
    class HELLOUTAU_EDITOR_EXPORT EditorSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit EditorSettingPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QComboBox *ustExportCharsetBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QComboBox> m_ustExportCharset;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGE_H
