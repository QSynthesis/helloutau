#ifndef HELLOUTAU_EDITOR_PIANOROLL_H
#define HELLOUTAU_EDITOR_PIANOROLL_H

#include <memory>

#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

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
    /// their lyrics, a ruler above and a keyboard beside.
    ///
    /// The piano roll reads the tree of the session through a \c TrackTimeline and keeps no
    /// copy of the notes: a change only marks it for drawing again. See the section on the piano
    /// roll in docs/Widgets.md.
    ///
    /// The colors are properties that a style sheet can set; unset, they derive from the palette.
    class HELLOUTAU_EDITOR_EXPORT PianoRoll : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor noteColor READ noteColor WRITE setNoteColor)
        Q_PROPERTY(QColor restColor READ restColor WRITE setRestColor)
        Q_PROPERTY(QColor lyricColor READ lyricColor WRITE setLyricColor)
        Q_PROPERTY(QColor unsampledColor READ unsampledColor WRITE setUnsampledColor)
        Q_PROPERTY(QColor unsampledLyricColor READ unsampledLyricColor WRITE setUnsampledLyricColor)
        Q_PROPERTY(QColor whiteRowColor READ whiteRowColor WRITE setWhiteRowColor)
        Q_PROPERTY(QColor blackRowColor READ blackRowColor WRITE setBlackRowColor)
        Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)
        Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    public:
        /// The part of a note that a hit reports, see SceneHit::part.
        enum NotePart {
            NoteBody,
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
        QColor whiteRowColor() const;
        void setWhiteRowColor(const QColor &color);
        QColor blackRowColor() const;
        void setBlackRowColor(const QColor &color);
        QColor lineColor() const;
        void setLineColor(const QColor &color);
        QColor barLineColor() const;
        void setBarLineColor(const QColor &color);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLL_H
