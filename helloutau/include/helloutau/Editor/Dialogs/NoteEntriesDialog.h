#ifndef HELLOUTAU_EDITOR_DIALOGS_NOTEENTRIESDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_NOTEENTRIESDIALOG_H

#include <optional>

#include <QtCore/QList>
#include <QtGui/QColor>
#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>
#include <hellokit/Support/Diagnostic.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QPlainTextEdit;

namespace hello::daw {

    /// The dialog that the Others button of the note properties opens, as that of UTAU: the
    /// entries of the selected notes that the note properties do not edit, as text. The built-in
    /// entries, those of kit::UstNoteEntries, are written as a UST writes them and are checked
    /// when the dialog is accepted. The user data are lines of the form key=value, with a warning
    /// for each entry that UTAU drops or changes when it saves the file.
    ///
    /// As in UTAU, the entries can be edited only if every selected note has the same entries.
    /// Otherwise the entries of the first note are shown, read only.
    class HELLOUTAU_EDITOR_EXPORT NoteEntriesDialog : public QDialog {
        Q_OBJECT
        Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor)
    public:
        explicit NoteEntriesDialog(const QList<kit::Note> &notes, QWidget *parent = nullptr);
        ~NoteEntriesDialog();

        /// Returns whether the entries can be edited.
        bool isEditable() const;

        /// Returns the first note with the entries as the edits give them, or \c std::nullopt
        /// with the errors of kit::UstNoteEntries if a line is refused.
        std::optional<kit::Note> editedNote(kit::DiagnosticList &diagnostics) const;

        /// Returns the note with the entries as accepted, for kit::NotePropertyChanges::others,
        /// or \c std::nullopt if the dialog was not accepted, the entries are read only, or the
        /// entries did not change.
        std::optional<kit::Note> others() const;

        QPlainTextEdit *builtInEdit() const;
        QPlainTextEdit *userDataEdit() const;

        /// Returns the label of the warnings of the user data, empty if none applies.
        QLabel *warningLabel() const;

        /// The color of the warnings, which are bold, by default the dark red of the warning of
        /// ProjectPropertiesDialog. It is set in a theme with \c qproperty-warningColor.
        QColor warningColor() const;
        void setWarningColor(const QColor &color);

    public Q_SLOTS:
        /// Accepts the entries, or reports the lines that are refused and keeps the dialog open.
        void accept() override;

    private:
        kit::Note m_note;
        bool m_editable;
        QPlainTextEdit *m_builtIn;
        QPlainTextEdit *m_userData;
        QLabel *m_warnings;
        QColor m_warningColor = QColor(0xb0, 0x00, 0x20);
        std::optional<kit::Note> m_others;

        void updateWarnings();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_NOTEENTRIESDIALOG_H
