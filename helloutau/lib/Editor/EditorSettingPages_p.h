#ifndef HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H
#define HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    class AppSettings;

    /// The UTAU folder, under Appearance & Behavior as the system settings of JetBrains IDEs.
    class SystemSettingsPage : public SettingPage {
        Q_OBJECT
    public:
        explicit SystemSettingsPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QLineEdit *utauDirectoryEdit() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QLineEdit> m_utauDirectory;
    };

    /// The encoding in which a UST is exported, and later the other settings of editing.
    class EditorSettingPage : public SettingPage {
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

    /// The engines that render, the playback mode, and the number of rendering threads.
    class RenderingSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit RenderingSettingPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QLineEdit *resamplerEdit() const;
        QLineEdit *wavtoolEdit() const;
        QComboBox *playbackModeBox() const;
        QSpinBox *threadCountBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QLineEdit> m_resampler;
        QPointer<QLineEdit> m_wavtool;
        QPointer<QComboBox> m_playbackMode;
        QPointer<QSpinBox> m_threads;
    };

    /// Adds the pages of the editor to \a catalog: Appearance & Behavior with System Settings in
    /// it, Editor, and Rendering. A plugin places its pages among them by EditorSettingPageIds,
    /// as Core places Keymap before Editor and Plugins before Rendering.
    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings);

}

#endif // HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H
