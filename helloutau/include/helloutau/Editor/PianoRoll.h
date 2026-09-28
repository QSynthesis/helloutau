#ifndef HELLOUTAU_EDITOR_PIANOROLL_H
#define HELLOUTAU_EDITOR_PIANOROLL_H

#include <memory>

#include <QtGui/QColor>
#include <QtWidgets/QWidget>

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
    class HELLOUTAU_EDITOR_EXPORT PianoRoll : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor noteColor READ noteColor WRITE setNoteColor)
        Q_PROPERTY(QColor restColor READ restColor WRITE setRestColor)
        Q_PROPERTY(QColor lyricColor READ lyricColor WRITE setLyricColor)
        Q_PROPERTY(QColor unsampledColor READ unsampledColor WRITE setUnsampledColor)
        Q_PROPERTY(QColor unsampledLyricColor READ unsampledLyricColor WRITE setUnsampledLyricColor)
        Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor)
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
        };

        /// What a press on the background does.
        enum Tool {
            /// Selects the notes in a rectangle.
            SelectTool,
            /// Draws a note after the last one.
            PenTool,
        };

        explicit PianoRoll(kit::ProjectSession *session, QWidget *parent = nullptr);
        ~PianoRoll();

        SceneView *view() const;
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
        /// @}

        /// \name Operations on the selection
        ///
        /// Each is one undo step, and returns whether it was made, with the reason in
        /// \a diagnostics otherwise.
        /// @{
        bool removeSelected(kit::DiagnosticList &diagnostics);
        bool transposeSelected(int semitones, kit::DiagnosticList &diagnostics);

        /// Inserts a note before the first selected note, with the key of that note and
        /// quantizedLength(), or after the last note if nothing is selected, and selects it.
        bool insertNote(kit::DiagnosticList &diagnostics);
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

    protected:
        void keyPressEvent(QKeyEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLL_H
