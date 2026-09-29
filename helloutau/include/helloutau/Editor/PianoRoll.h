#ifndef HELLOUTAU_EDITOR_PIANOROLL_H
#define HELLOUTAU_EDITOR_PIANOROLL_H

#include <memory>
#include <optional>
#include <utility>

#include <QtCore/QList>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <hellokit/Document/Note.h>
#include <hellokit/Support/Diagnostic.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLineEdit;

namespace hello::kit {
    class ProjectSession;
    class TrackTimeline;
    class VoiceBank;
}

namespace hello::daw {

    class PianoKeyboard;
    class SceneView;
    class TimelineRuler;

    /// The piano roll of the first track of a project: the notes as bars along a timeline, with
    /// their lyrics, a ruler above and a keyboard beside, where the notes are selected and edited.
    ///
    /// The piano roll reads the tree of the session through a \c TrackTimeline and keeps no
    /// copy of the notes: a change only marks it for drawing again. A drag changes only what is
    /// drawn, and writes the tree in one transaction when it ends; Escape abandons it. See the
    /// section on the piano roll and step 4 in docs/Widgets.md.
    ///
    /// The selection is a set of note identifiers, so it follows the notes through edits, undo
    /// and redo.
    ///
    /// The colors are properties that a style sheet can set; unset, they derive from the palette.
    /// So are the distances within which the pitch responds to the pointer, pointGrip() and
    /// curveGrip().
    class HELLOUTAU_EDITOR_EXPORT PianoRoll : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor noteColor READ noteColor WRITE setNoteColor)
        Q_PROPERTY(QColor restColor READ restColor WRITE setRestColor)
        Q_PROPERTY(QColor lyricColor READ lyricColor WRITE setLyricColor)
        Q_PROPERTY(QColor unsampledColor READ unsampledColor WRITE setUnsampledColor)
        Q_PROPERTY(QColor unsampledLyricColor READ unsampledLyricColor WRITE setUnsampledLyricColor)
        Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor)
        Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor)
        Q_PROPERTY(QColor pitchColor READ pitchColor WRITE setPitchColor)
        Q_PROPERTY(QColor vibratoColor READ vibratoColor WRITE setVibratoColor)
        Q_PROPERTY(QColor faintPointColor READ faintPointColor WRITE setFaintPointColor)
        Q_PROPERTY(QColor envelopeColor READ envelopeColor WRITE setEnvelopeColor)
        Q_PROPERTY(QColor parameterColor READ parameterColor WRITE setParameterColor)
        Q_PROPERTY(double pointGrip READ pointGrip WRITE setPointGrip)
        Q_PROPERTY(double curveGrip READ curveGrip WRITE setCurveGrip)
        Q_PROPERTY(QColor whiteRowColor READ whiteRowColor WRITE setWhiteRowColor)
        Q_PROPERTY(QColor blackRowColor READ blackRowColor WRITE setBlackRowColor)
        Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)
        Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    public:
        /// The part that a hit reports, see SceneHit::part.
        enum Part {
            NoteBody,
            /// The right edge of a note, dragged to change its length.
            NoteEnd,
            /// Anywhere not on a note.
            Background,
            /// A Mode2 point, while the pitch is shown; SceneHit::index is its index in the
            /// note.
            PitchPoint,
            /// The handles of a vibrato, while the pitch is shown: the start of its trapezoid,
            /// which sets its length, the ends of its fades, its top edge, which sets its depth,
            /// the right edge of its period box and the inside of the box, which sets its phase.
            VibratoStart,
            VibratoFadeIn,
            VibratoFadeOut,
            VibratoDepth,
            VibratoPeriod,
            VibratoPhase,
            /// An anchor of an envelope in the parameter area; SceneHit::index is its index in
            /// time order.
            EnvelopePoint,
            /// The handle of the value of a note in the parameter area, while it shows the
            /// intensity, the modulation or the velocity.
            ParameterHandle,
        };

        /// What the parameter area shows and edits.
        enum Lane {
            EnvelopeLane,
            IntensityLane,
            ModulationLane,
            /// The consonant velocity
            VelocityLane,
        };

        /// What a press on the background does with the left button. With the right button,
        /// every tool but PitchTool drawing selects the notes in the span of time of a drag,
        /// whatever their keys, and with Ctrl adds them to the selection, as a drag selects in
        /// UTAU.
        enum Tool {
            /// Selects the notes in a rectangle.
            SelectTool,
            /// Draws a note on the background: before the note at the pointer, or after the last
            /// note, from where the note before it ends; with Shift, from the pointer after a
            /// rest, or within a rest in its place. A press on a note selects and moves it, as
            /// with SelectTool.
            PenTool,
            /// Draws the Mode1 pitch, anywhere in the roll, while the pitch is shown and the
            /// project turns Mode2 off; otherwise it acts as SelectTool. See
            /// isPitchVisible().
            PitchTool,
        };

        explicit PianoRoll(kit::ProjectSession *session, QWidget *parent = nullptr);
        ~PianoRoll();

        SceneView *view() const;

        /// The parameter area below the roll, which shares its time axis, and where the
        /// envelopes of the notes are drawn and edited (step 4 in docs/Tuning.md), or one value
        /// of each note (step 7).
        SceneView *parameterView() const;

        /// What the parameter area shows, chosen with the buttons beside it. The envelopes at
        /// first.
        Lane lane() const;
        void setLane(Lane lane);
        TimelineRuler *ruler() const;
        PianoKeyboard *keyboard() const;
        kit::TrackTimeline *timeline() const;

        /// Scrolls to the start of the track and to the middle of the keys its notes use.
        void scrollToNotes();

        /// The voice bank against which notes are looked up, or \c nullptr if none is known.
        std::shared_ptr<const kit::VoiceBank> voiceBank() const;
        void setVoiceBank(std::shared_ptr<const kit::VoiceBank> bank);

        /// Returns whether note \a index of the timeline is to be sung but voiceBank() has no
        /// sample for it. Such a note is drawn as an outline in unsampledColor(). Without a voice
        /// bank no note is reported, since nothing is known of the samples.
        bool lacksSample(int index) const;

        Tool tool() const;
        void setTool(Tool tool);

        /// \name Quantization
        ///
        /// The grid in ticks to which drags snap, or 0 for none. Holding Alt during a drag
        /// suspends it.
        /// @{
        int quantization() const;
        void setQuantization(int ticks);

        /// The choices offered, from a quarter note to a sixty-fourth, and 0.
        static QList<int> quantizations();

        /// The length of a note that a command or a click creates: the quantization, or a
        /// quarter note if there is none.
        int quantizedLength() const;

        QComboBox *quantizationBox() const;
        /// @}

        /// \name Selection
        /// @{

        /// The indices in the timeline of the selected notes, in ascending order.
        QList<int> selectedIndices() const;
        void setSelectedIndices(const QList<int> &indices);
        void selectAll();

        /// The selected Mode2 points, as the index of the note and the index of the point in
        /// it, in ascending order. Points and notes are not selected at the same time: selecting
        /// either clears the other.
        QList<std::pair<int, int>> selectedPoints() const;
        void setSelectedPoints(const QList<std::pair<int, int>> &points);
        /// @}

        /// \name Operations on the selection
        ///
        /// Each is one undo step, and returns whether it was made, with the reason in
        /// \a diagnostics otherwise.
        /// @{

        /// Removes the selected notes, or the selected points. Each note keeps two points at
        /// least: where fewer would remain, its first and last points stay.
        bool removeSelected(kit::DiagnosticList &diagnostics);
        bool transposeSelected(int semitones, kit::DiagnosticList &diagnostics);

        /// Gives each selected sung note without Mode2 points the default two, 15 ms before
        /// and after its start at its own pitch; if every such note has points, removes them.
        bool togglePortamento(kit::DiagnosticList &diagnostics);

        /// Gives each selected sung note without a vibrato the default one of
        /// VibratoDialog::defaultVibrato(); if every such note has one, removes them.
        bool toggleVibrato(kit::DiagnosticList &diagnostics);

        /// The two crossfades of the envelopes over the overlaps of the notes (QSynthesis).
        enum Crossfade {
            /// The attack from p1 at the start to p2 at the end of the overlap, the release
            /// from p3 at the overlap of the next note to p4 at the end, the volumes kept
            CrossfadeP2P3,
            /// p1 at the end of the overlap with the volume of p2, and p2 5 ms after it; p4 at
            /// the overlap of the next note with the volume of p3, and p3 5 ms before it
            CrossfadeP1P4,
        };

        /// Fades the envelope of each selected sung note in over its overlap with the previous
        /// note, if that is sung and the overlap after its correction positive, and out over
        /// the overlap of the next note likewise. An envelope so changed loses its middle
        /// anchor; a note without an envelope starts from the default of UTAU.
        bool crossfadeEnvelopes(Crossfade crossfade, kit::DiagnosticList &diagnostics);

        /// The parameters of a note that are copied, pasted and reset together.
        enum Parameter {
            /// The Mode2 points
            PortamentoParameter = 0x1,
            VibratoParameter = 0x2,
            EnvelopeParameter = 0x4,
            AllParameters = PortamentoParameter | VibratoParameter | EnvelopeParameter,
        };
        Q_DECLARE_FLAGS(Parameters, Parameter)

        /// Puts the selected notes on the clipboard, as \c .usth writes notes, and returns
        /// whether there were any.
        bool copySelected();

        /// The notes on the clipboard, as copySelected() put them there, or none.
        static QList<kit::Note> copiedNotes();

        /// Gives the selected sung notes \a parameters of the copied notes as they are: those of
        /// the one copied note to every selected note, or those of several copied notes to the
        /// selected notes in order, as far as both go. Rests are skipped on either side.
        bool pasteParameters(Parameters parameters, kit::DiagnosticList &diagnostics);

        /// Removes \a parameters from the selected sung notes, or from every note if none is
        /// selected, which leaves the defaults of UTAU: no points, no vibrato, its envelope.
        bool resetParameters(Parameters parameters, kit::DiagnosticList &diagnostics);

        /// Multiplies the heights of the points of the selected sung notes by \a portamento and
        /// the depths of their vibratos by \a vibrato.
        bool scalePitch(double portamento, double vibrato, kit::DiagnosticList &diagnostics);

        /// Inserts a note before the first selected note, with the key of that note and
        /// quantizedLength(), or after the last note if nothing is selected, and selects it.
        bool insertNote(kit::DiagnosticList &diagnostics);

        /// Inserts the copied notes as they are, before the first selected note or after the
        /// last note if nothing is selected, and selects them.
        bool pasteNotes(kit::DiagnosticList &diagnostics);
        /// @}

        /// \name Editing a lyric in place
        /// @{

        /// Shows an editor over note \a index with its lyric. Return commits it; Tab and
        /// Shift+Tab commit it and edit the next and the previous note; Escape abandons it.
        /// Losing the focus or scrolling commits it.
        void editLyric(int index);

        /// The editor, visible while a lyric is edited.
        QLineEdit *lyricEditor() const;
        /// @}

        /// The position of playback in ticks, drawn as a vertical line, or none. The view
        /// scrolls to keep it in sight: once it passes the right edge, or is left of the view,
        /// it continues from near the left edge.
        std::optional<double> playheadPosition() const;
        void setPlayheadPosition(std::optional<double> tick);

        /// The playhead at rest, in ticks, 0 at first: where a preview starts and where the line
        /// returns to when playback stops. The line is drawn there while playheadPosition() is
        /// none. Pressing or dragging on the ruler moves it, snapped to the quantization unless
        /// Alt is held, and reports cursorMoved(). See step 6 in docs/Widgets.md.
        double cursorPosition() const;
        void setCursorPosition(double tick);

        /// Whether the playhead at rest is drawn and moved on the ruler, as in the realtime
        /// playback mode; true at first. Without it, as in the prerender mode, only
        /// playheadPosition() is drawn, and the ruler moves nothing.
        bool isCursorEnabled() const;
        void setCursorEnabled(bool enabled);

        /// Whether the pitch of each note is drawn: its portamento in pitchColor() and, apart
        /// from it, its vibrato around the middle of its row in vibratoColor(), both as the
        /// resampler receives them (kit::PitchCurve). See step 1 in docs/Tuning.md.
        ///
        /// While it is, the Mode2 points are drawn on the portamento and edited there (step 2 in
        /// docs/Tuning.md): a point is dragged, with those selected with it, Shift snapping it to
        /// the time of another point of its note and Ctrl its height to 50 cents; a double click
        /// on the portamento inserts a point; the context menu of a point changes its shape or
        /// removes it. The first point after a sung note starts at the pitch of that note and
        /// only moves in time, as does the last point.
        ///
        /// While the project turns Mode2 off, the pitch is the curve of the Mode1 values
        /// instead, as UTAU synthesizes it (kit::PitchBend::curveAt()), and the points and
        /// vibratos are hidden, though kept (step 5 in docs/Tuning.md). PitchTool draws the
        /// curve: along a stroke, the values of each note from its first reading to its end
        /// take the pitch under the pointer, and a stroke with its right button returns them to 0.
        /// With the other tools, the right button selects a span of time, see Tool. A note without
        /// values starts them at its first reading, as UTAU does.
        bool isPitchVisible() const;
        void setPitchVisible(bool visible);

        /// The distance in pixels within which a Mode2 point is hit, 6 by default.
        double pointGrip() const;
        void setPointGrip(double pixels);

        /// The distance in pixels above and below the portamento within which a double click
        /// inserts a point, 5 by default. Elsewhere on a note a double click edits its lyric,
        /// as F2 does. See the questions in docs/Tuning.md.
        double curveGrip() const;
        void setCurveGrip(double pixels);

        QColor noteColor() const;
        void setNoteColor(const QColor &color);
        QColor restColor() const;
        void setRestColor(const QColor &color);
        QColor lyricColor() const;
        void setLyricColor(const QColor &color);
        QColor unsampledColor() const;
        void setUnsampledColor(const QColor &color);
        QColor unsampledLyricColor() const;
        void setUnsampledLyricColor(const QColor &color);
        QColor selectionColor() const;
        void setSelectionColor(const QColor &color);
        QColor pitchColor() const;
        void setPitchColor(const QColor &color);
        QColor vibratoColor() const;
        void setVibratoColor(const QColor &color);
        /// The color of the points of the notes whose portamento is not under the pointer, and
        /// of the handles of the values that a note leaves to the default.
        QColor faintPointColor() const;
        void setFaintPointColor(const QColor &color);
        QColor envelopeColor() const;
        void setEnvelopeColor(const QColor &color);
        /// The color of the handles of the values in the parameter area
        QColor parameterColor() const;
        void setParameterColor(const QColor &color);
        QColor playheadColor() const;
        void setPlayheadColor(const QColor &color);
        QColor whiteRowColor() const;
        void setWhiteRowColor(const QColor &color);
        QColor blackRowColor() const;
        void setBlackRowColor(const QColor &color);
        QColor lineColor() const;
        void setLineColor(const QColor &color);
        QColor barLineColor() const;
        void setBarLineColor(const QColor &color);

    Q_SIGNALS:
        /// The selection changed, or the notes it refers to did.
        void selectionChanged();

        /// An edit made in the roll itself, by a gesture, a lyric or a context menu, was refused
        /// and left the project as it was; \a message states why. The functions of the roll
        /// report in their diagnostics instead.
        void editRefused(const QString &message);

        /// The user moved the playhead at rest to \a tick on the ruler.
        void cursorMoved(double tick);

    protected:
        void keyPressEvent(QKeyEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(PianoRoll::Parameters)

}

#endif // HELLOUTAU_EDITOR_PIANOROLL_H
