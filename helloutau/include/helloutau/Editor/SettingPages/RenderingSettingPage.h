#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_RENDERINGSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_RENDERINGSETTINGPAGE_H

#include <optional>

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLineEdit;

namespace hello::daw {

    class AppSettings;

    /// The synth tools used for audio rendering, the playback mode, and the number of rendering
    /// threads.
    class HELLOUTAU_EDITOR_EXPORT RenderingSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit RenderingSettingPage(AppSettings &settings, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QLineEdit *resamplerEdit() const;
        QLineEdit *wavtoolEdit() const;
        QComboBox *playbackModeBox() const;
        QComboBox *threadCountBox() const;

        /// Returns the number of rendering threads in the box, zero for Automatic, or
        /// \c std::nullopt if the text typed in is not a positive count. There is no upper
        /// limit, as there is none for the jobs of ninja and make.
        std::optional<int> threadCount() const;
        std::optional<int> renderLogLimit() const;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        QPointer<QLineEdit> m_resampler;
        QPointer<QLineEdit> m_wavtool;
        QPointer<QComboBox> m_playbackMode;
        QPointer<QComboBox> m_threads;
        QPointer<QComboBox> m_renderLogMode;
        QPointer<QComboBox> m_renderLogLimit;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_RENDERINGSETTINGPAGE_H
