#ifndef HELLOUTAU_EDITOR_FREQUENCYMAPWAVEFORM_H
#define HELLOUTAU_EDITOR_FREQUENCYMAPWAVEFORM_H

#include <memory>
#include <optional>

#include <QtGui/QColor>
#include <QtWidgets/QFrame>

#include <hellokit/AudioFile/WaveAudio.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class FrequencyMapView;

    /// The waveform below the editing area of frequency maps, with a line at the time of the
    /// playback. See the editing in docs/FrequencyMaps.md.
    ///
    /// The waveform follows the time axis of the view: a time is drawn at the same x on the
    /// screen as in the viewport of the view, and the part below the column of labels of the view
    /// is left blank. The waveform of each channel is drawn in a lane of its own. A wheel step is
    /// passed to the viewport of the view, so that it scrolls and zooms the view.
    ///
    /// The colors are properties that a style sheet can set. The background and the lines take
    /// the colors of the rows of the view.
    class HELLOUTAU_EDITOR_EXPORT FrequencyMapWaveform : public QFrame {
        Q_OBJECT
        Q_PROPERTY(QColor waveColor READ waveColor WRITE setWaveColor)
        Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor)
    public:
        /// Creates the waveform that follows \a view, which outlives it.
        explicit FrequencyMapWaveform(FrequencyMapView *view, QWidget *parent = nullptr);
        ~FrequencyMapWaveform();

        /// Returns the audio shown, or null.
        std::shared_ptr<const kit::WaveAudio> audio() const;
        void setAudio(std::shared_ptr<const kit::WaveAudio> audio);

        /// Returns the time of the playback line in milliseconds, or \c std::nullopt.
        std::optional<double> playhead() const;
        void setPlayhead(std::optional<double> time);

        /// Returns the x of the widget at which \a time in milliseconds is drawn.
        double xOf(double time) const;

        QColor waveColor() const;
        void setWaveColor(const QColor &color);
        QColor playheadColor() const;
        void setPlayheadColor(const QColor &color);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_FREQUENCYMAPWAVEFORM_H
