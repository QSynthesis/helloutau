#ifndef HELLOUTAU_EDITOR_DIALOGS_TRANSPOSEDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_TRANSPOSEDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QSpinBox;

namespace hello::daw {

    /// A dialog in which the user enters the number of semitones by which the selected notes are
    /// transposed, initially 0, negative downwards. The box accepts only the numbers that keep
    /// every note within the keyboard range, kit::lowestNoteNum to kit::highestNoteNum, and the
    /// dialog shows that range and the keys of the notes after the transposition.
    class HELLOUTAU_EDITOR_EXPORT TransposeDialog : public QDialog {
        Q_OBJECT
    public:
        /// Constructs the dialog for notes whose keys span \a lowest to \a highest.
        TransposeDialog(int lowest, int highest, QWidget *parent = nullptr);
        ~TransposeDialog();

        /// The number of semitones, negative downwards
        int semitones() const;

        QSpinBox *semitoneBox() const;

        /// The label with the range of the box and the keys after the transposition
        QLabel *summaryLabel() const;

    private:
        void showSummary();

        int m_lowest;
        int m_highest;
        QSpinBox *m_semitones;
        QLabel *m_summary;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_TRANSPOSEDIALOG_H
