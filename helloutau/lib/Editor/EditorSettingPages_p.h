#ifndef HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H
#define HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace hello::daw {

    class AppSettings;

    /// The UTAU folder and the encoding in which a UST is exported.
    class GeneralSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit GeneralSettingPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QLineEdit *utauDirectoryEdit() const;
        QComboBox *ustExportCharsetBox() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QLineEdit> m_utauDirectory;
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

    /// Adds the pages of the editor to \a catalog.
    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings);

}

#endif // HELLOUTAU_EDITOR_EDITORSETTINGPAGES_P_H
