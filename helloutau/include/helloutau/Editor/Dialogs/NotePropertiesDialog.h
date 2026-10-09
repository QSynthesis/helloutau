#ifndef HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H

#include <optional>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>
#include <hellokit/Edit/ProjectEdits.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

namespace hello::daw {

    /// Edits the properties of the selected notes, as the note property dialog of UTAU does: the
    /// lyric, the length, the tempo, the intensity, the modulation, the consonant velocity, the
    /// pre-utterance, the overlap, the start point and the flags.
    ///
    /// A field shows the value the notes share, or stays empty with "(various)" where they
    /// differ, and "(default)" where they leave it to the default. Only the fields edited
    /// change, on every note; emptied, a field that a note may leave to the default returns it
    /// to the default. See the note properties in docs/Widgets.md.
    class HELLOUTAU_EDITOR_EXPORT NotePropertiesDialog : public QDialog {
        Q_OBJECT
    public:
        enum Field {
            Lyric,
            Length,
            Tempo,
            Intensity,
            Modulation,
            Velocity,
            PreUtterance,
            VoiceOverlap,
            StartPoint,
            Flags,
        };

        struct Defaults {
            QList<double> tempo;
            QList<double> preUtterance;
            QList<double> voiceOverlap;
        };

        explicit NotePropertiesDialog(const QList<kit::Note> &notes, Defaults defaults = {},
                                      QWidget *parent = nullptr);
        ~NotePropertiesDialog();

        /// The fields edited, for ProjectEdits::setNoteProperties(). A number that does not read
        /// as one, or a length that is not positive, is left out.
        kit::NotePropertyChanges changes() const;

        QLineEdit *field(Field field) const;

    private:
        QList<QLineEdit *> m_fields;
        QList<bool> m_edited;
    };

    /// Asks the tempo of a note: a value in BPM, or none to follow the tempo before it.
    class HELLOUTAU_EDITOR_EXPORT TempoDialog : public QDialog {
        Q_OBJECT
    public:
        /// \param tempo the tempo that the note sets, if any
        /// \param current the tempo the note plays at, shown while it sets none
        TempoDialog(std::optional<double> tempo, double current, QWidget *parent = nullptr);
        ~TempoDialog();

        std::optional<double> tempo() const;

        QDoubleSpinBox *tempoBox() const;
        QCheckBox *followBox() const;

    private:
        QDoubleSpinBox *m_tempo;
        QCheckBox *m_follow;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H
