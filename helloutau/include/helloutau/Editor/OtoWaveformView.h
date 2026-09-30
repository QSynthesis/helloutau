#ifndef HELLOUTAU_EDITOR_OTOWAVEFORMVIEW_H
#define HELLOUTAU_EDITOR_OTOWAVEFORMVIEW_H

#include <memory>
#include <optional>
#include <utility>

#include <QtGui/QColor>
#include <QtWidgets/QAbstractScrollArea>

#include <hellokit/Synth/Spectrogram.h>
#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/VoiceBank/FrequencyTable.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The waveform of the audio file of an oto entry, with the five values of the entry drawn
    /// over it and dragged there. See the waveform area in docs/VoiceBankEditor.md.
    ///
    /// Before the offset and after the cutoff the audio is masked, and the consonant is a region
    /// of another color. The pre-utterance and the overlap are lines, and the envelope that the
    /// overlap and the cutoff determine is drawn over the audio. Each of the five boundaries is
    /// dragged by the pointer within grip() pixels of it; a drag changes only the view and emits
    /// entryEdited() once released, and Escape abandons it. See moved() for the rules of a move.
    ///
    /// Times are in milliseconds from the start of the audio file. The wheel scrolls, and the
    /// wheel with Ctrl zooms about the pointer.
    ///
    /// The colors are properties that a style sheet can set; unset, they have fixed defaults
    /// that read on light and dark palettes alike.
    class HELLOUTAU_EDITOR_EXPORT OtoWaveformView : public QAbstractScrollArea {
        Q_OBJECT
        Q_PROPERTY(QColor waveColor READ waveColor WRITE setWaveColor)
        Q_PROPERTY(QColor maskColor READ maskColor WRITE setMaskColor)
        Q_PROPERTY(QColor consonantColor READ consonantColor WRITE setConsonantColor)
        Q_PROPERTY(QColor preUtteranceColor READ preUtteranceColor WRITE setPreUtteranceColor)
        Q_PROPERTY(QColor overlapColor READ overlapColor WRITE setOverlapColor)
        Q_PROPERTY(QColor boundaryColor READ boundaryColor WRITE setBoundaryColor)
        Q_PROPERTY(QColor envelopeColor READ envelopeColor WRITE setEnvelopeColor)
        Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor)
        Q_PROPERTY(QColor frequencyColor READ frequencyColor WRITE setFrequencyColor)
        Q_PROPERTY(QColor spectrumColor READ spectrumColor WRITE setSpectrumColor)
        Q_PROPERTY(double grip READ grip WRITE setGrip)
    public:
        /// The values of an entry, in the order of the keys 1 to 5 that set them at the pointer,
        /// as OpenUtau assigns them.
        enum Value {
            Offset,
            Overlap,
            PreUtterance,
            Consonant,
            Cutoff,
        };
        Q_ENUM(Value)

        explicit OtoWaveformView(QWidget *parent = nullptr);
        ~OtoWaveformView();

        /// The audio shown, or null for none, as for an audio file that does not exist or does
        /// not read. Shows all of it.
        std::shared_ptr<const kit::WaveAudio> audio() const;
        void setAudio(std::shared_ptr<const kit::WaveAudio> audio);

        /// The duration of the audio, or 0 without audio.
        double duration() const;

        /// The entry shown, including a drag in progress, or none. Without audio its values are
        /// neither drawn nor dragged, since a positive cutoff counts from the end of the file.
        std::optional<kit::VoiceOtoEntry> entry() const;

        /// Shows \a entry, abandoning a drag in progress.
        void setEntry(const std::optional<kit::VoiceOtoEntry> &entry);

        /// \name The view
        /// @{

        /// The pixels per millisecond.
        double scale() const;

        /// The time at the left edge of the viewport.
        double viewStart() const;

        /// Zooms by \a factor about the time at \a x of the viewport, which stays there.
        void zoom(double factor, double x);

        /// Shows the whole audio in the viewport.
        void fit();

        /// The time at \a x of the viewport, and the reverse.
        double timeAt(double x) const;
        double xOf(double time) const;
        /// @}

        /// The time under the pointer, or none while it is outside the viewport.
        std::optional<double> pointerTime() const;

        /// The value whose boundary the pointer is on, or that a drag moves.
        std::optional<Value> activeValue() const;

        /// Moves \a value of the entry to \a time, rounded to a millisecond, see moved(), and
        /// emits entryEdited() if the entry changes. Returns whether it changed.
        bool setValueAt(Value value, double time);

        /// The time of the line that marks what plays, or none.
        void setPlayhead(std::optional<double> time);

        /// \name The spectrum and the frequency table
        /// See docs/FrequencyTables.md.
        /// @{

        /// The spectrogram drawn in place of the waveform, or null to draw the waveform. Its
        /// rows follow the pitch axis, see pitchRange().
        std::shared_ptr<const kit::Spectrogram> spectrogram() const;
        void setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram);

        /// The frequency table whose curve is drawn over the audio, or none. The frames of a
        /// frequency of 0 are gaps in the curve.
        std::optional<kit::FrequencyTable> frequencyTable() const;
        void setFrequencyTable(const std::optional<kit::FrequencyTable> &table);

        /// The notes at the bottom and the top of the audio on the pitch axis, on which the
        /// curve and the spectrogram are drawn: those of the voiced frames of the table an
        /// octave apart below and above, within C1 to B7, or C2 to C6 without a table.
        std::pair<double, double> pitchRange() const;

        /// The y of the viewport of \a note on the pitch axis, where 60 is C4 and a fraction is
        /// a part of a semitone.
        double yOfNote(double note) const;
        /// @}

        /// The distance in pixels within which the pointer takes a boundary. 5 by default.
        double grip() const;
        void setGrip(double grip);

        /// Returns the time of \a value of \a entry in an audio file of \a duration: the offset
        /// and its sum with each of the other values but the cutoff. A negative cutoff is the
        /// length from the offset, and a positive one or zero the distance from the end.
        static double positionOf(const kit::VoiceOtoEntry &entry, Value value, double duration);

        /// Returns \a entry with \a value moved to \a time, clamped to the audio:
        /// - Moving the offset keeps the other values at their times, as UTAU and OpenUtau do,
        ///   and pushes the end of the consonant and the cutoff along if it passes them.
        /// - The end of the consonant stays after the offset, and pushes the cutoff along.
        /// - The cutoff stays after the end of the consonant.
        /// - A negative cutoff stays negative and a positive one positive, as the author decided
        ///   after QSynthesis. A cutoff of zero reaches the end of the file; it stays zero unless
        ///   moved, and then becomes negative, as for a new entry. A negative cutoff is at least
        ///   one millisecond long, since zero would reach the end of the file.
        /// Every value written is rounded to a thousandth of a millisecond, so that a sum keeps
        /// the digits of its terms.
        static kit::VoiceOtoEntry moved(const kit::VoiceOtoEntry &entry, Value value, double time,
                                        double duration);

        /// Returns the name of \a value as the entry table heads it.
        static QString nameOf(Value value);

        QColor waveColor() const;
        void setWaveColor(const QColor &color);
        QColor maskColor() const;
        void setMaskColor(const QColor &color);
        QColor consonantColor() const;
        void setConsonantColor(const QColor &color);
        QColor preUtteranceColor() const;
        void setPreUtteranceColor(const QColor &color);
        QColor overlapColor() const;
        void setOverlapColor(const QColor &color);
        QColor boundaryColor() const;
        void setBoundaryColor(const QColor &color);
        QColor envelopeColor() const;
        void setEnvelopeColor(const QColor &color);
        QColor playheadColor() const;
        void setPlayheadColor(const QColor &color);

        /// The curve of the frequency table
        QColor frequencyColor() const;
        void setFrequencyColor(const QColor &color);

        /// The loudest part of the spectrogram, which fades into the background of the view
        /// with the level
        QColor spectrumColor() const;
        void setSpectrumColor(const QColor &color);

    Q_SIGNALS:
        /// A drag ended, or setValueAt() moved a value: \a entry is to be written.
        void entryEdited(const kit::VoiceOtoEntry &entry);

        /// The pointer moved over the viewport, onto a boundary or off it, or left.
        void pointerMoved();

        /// The viewport was double-clicked off any boundary, at \a time.
        void playRequested(double time);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void scrollContentsBy(int dx, int dy) override;
        bool viewportEvent(QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_OTOWAVEFORMVIEW_H
