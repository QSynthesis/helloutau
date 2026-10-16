#ifndef HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H

#include <optional>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>
#include <hellokit/Edit/ProjectEdits.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

namespace hello::daw {

    /// Edits the properties of the selected notes, as the note property dialog of UTAU does: the
    /// lyric, the key, the length, the tempo, the intensity, the modulation, the consonant
    /// velocity, the pre-utterance, the overlap, the start point and the flags.
    ///
    /// A field shows the value the notes share, or stays empty with "(various)" if they
    /// differ, and "(default)" if they leave it to the default. Only the fields edited
    /// change, on every note. If emptied, a field that a note may leave to the default returns
    /// it to the default. See the note properties in docs/Widgets.md.
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

        /// The values that a note takes for the properties it leaves to the default: the tempo
        /// in effect before it, and the pre-utterance and the overlap of its sample, or
        /// \c std::nullopt without a sample.
        struct Defaults {
            double tempo = 0;
            std::optional<double> preUtterance;
            std::optional<double> voiceOverlap;
        };

        /// \a defaults has one element for each of \a notes, or is empty if the defaults are
        /// unknown. A default is shown as a number only if every note has it and all are equal.
        explicit NotePropertiesDialog(const QList<kit::Note> &notes,
                                      const QList<Defaults> &defaults = {},
                                      QWidget *parent = nullptr);
        ~NotePropertiesDialog();

        /// The fields edited, for ProjectEdits::setNoteProperties(). A number that does not parse,
        /// or a length that is not positive, is left out.
        kit::NotePropertyChanges changes() const;

        QLineEdit *field(Field field) const;

        /// Returns the box of the key, from C1 to B8 as in UTAU, with no key chosen if the notes
        /// differ.
        QComboBox *keyBox() const;

        /// Returns the button that opens NoteEntriesDialog, whose accepted entries changes()
        /// gives as kit::NotePropertyChanges::others. The dialog opens again with the entries
        /// accepted before.
        QPushButton *othersButton() const;

    private:
        QList<kit::Note> m_notes;
        QList<QLineEdit *> m_fields;
        QList<bool> m_edited;
        QComboBox *m_key;
        bool m_keyEdited = false;
        QPushButton *m_othersButton;
        std::optional<kit::Note> m_others;
    };

    /// A dialog in which the user enters the tempo of a note: a value in BPM, or none to follow
    /// the preceding tempo.
    class HELLOUTAU_EDITOR_EXPORT TempoDialog : public QDialog {
        Q_OBJECT
    public:
        /// \param tempo the tempo that the note sets, if any
        /// \param current the tempo the note plays at, shown while it sets none
        TempoDialog(std::optional<double> tempo, double current, QWidget *parent = nullptr);
        ~TempoDialog();

        /// Returns the tempo that the note is to set, or \c std::nullopt if the note follows the
        /// tempo before. Unless the tempo was edited, the tempo of the note is returned
        /// unchanged, even if the box cannot show it, so that a tempo out of the range of UTAU is
        /// not changed by confirming the dialog.
        std::optional<double> tempo() const;

        QDoubleSpinBox *tempoBox() const;
        QCheckBox *followBox() const;

    private:
        std::optional<double> m_original;
        bool m_tempoEdited = false;
        QDoubleSpinBox *m_tempo;
        QCheckBox *m_follow;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_NOTEPROPERTIESDIALOG_H
