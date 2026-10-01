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

    /// The ids of the pages of the editor, in the order of the settings of JetBrains IDEs. See the
    /// settings dialog in docs/Widgets.md.
    struct EditorSettingPageIds {
        static constexpr char appearanceAndBehavior[] = "editor.AppearanceAndBehavior";
        static constexpr char menusAndToolbars[] = "editor.MenusAndToolbars";
        static constexpr char systemSettings[] = "editor.SystemSettings";
        static constexpr char keymap[] = "editor.Keymap";
        static constexpr char editor[] = "editor.Editor";
        static constexpr char rendering[] = "editor.Rendering";
    };

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

    /// Adds the pages of the editor to \a catalog: Appearance & Behavior with Menus and Toolbars
    /// and System Settings in it, Keymap, Editor, and Rendering. A plugin places its pages among
    /// them, as Core places Plugins before Rendering. The keymap edits the shortcuts of
    /// \a registry and writes them to \a keymapFile. Menus and Toolbars edits the layouts of
    /// \a registry and writes them to \a layoutsFile.
    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings,
                               QAK::ActionRegistry *registry, const QString &keymapFile,
                               const QString &layoutsFile);

}

#endif // HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H
