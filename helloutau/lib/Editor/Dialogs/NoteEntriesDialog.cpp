#include "NoteEntriesDialog.h"

#include <QtGui/QFontDatabase>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Document/UstNoteEntries.h>

#include "DiagnosticBox.h"

namespace hello::daw {

    namespace {

        QStringList linesOf(const QPlainTextEdit *edit) {
            return edit->toPlainText().split(QLatin1Char('\n'));
        }

        QPlainTextEdit *entriesEdit(const QStringList &lines, bool editable) {
            auto edit = new QPlainTextEdit(lines.join(QLatin1Char('\n')));
            edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            edit->setLineWrapMode(QPlainTextEdit::NoWrap);
            edit->setReadOnly(!editable);
            return edit;
        }

    }

    NoteEntriesDialog::NoteEntriesDialog(const QList<kit::Note> &notes, QWidget *parent)
        : QDialog(parent), m_note(notes.value(0)) {
        setWindowTitle(tr("Others"));

        using Entries = kit::UstNoteEntries;
        const auto builtIn = Entries::builtInLinesOf(m_note);
        const auto userData = Entries::userDataLinesOf(m_note);
        m_editable = true;
        for (const auto &note : notes) {
            m_editable = m_editable && Entries::builtInLinesOf(note) == builtIn &&
                         Entries::userDataLinesOf(note) == userData;
        }

        m_builtIn = entriesEdit(builtIn, m_editable);
        auto builtInNote =
            new QLabel(tr("The envelope, the vibrato, the pitch, the label, $direct, "
                          "$patch and the regions, one entry a line as a UST "
                          "writes them."));
        builtInNote->setWordWrap(true);
        auto builtInGroup = new QGroupBox(tr("&Built-in Entries"));
        auto builtInLayout = new QVBoxLayout(builtInGroup);
        builtInLayout->addWidget(m_builtIn);
        builtInLayout->addWidget(builtInNote);

        m_userData = entriesEdit(userData, m_editable);
        auto userDataNote = new QLabel(tr("One entry a line, as key=value."));
        userDataNote->setWordWrap(true);
        m_warnings = new QLabel();
        m_warnings->setTextFormat(Qt::RichText);
        m_warnings->setWordWrap(true);
        auto userDataGroup = new QGroupBox(tr("&User Data"));
        auto userDataLayout = new QVBoxLayout(userDataGroup);
        userDataLayout->addWidget(m_userData);
        userDataLayout->addWidget(userDataNote);
        userDataLayout->addWidget(m_warnings);

        auto layout = new QVBoxLayout(this);
        if (!m_editable) {
            auto readOnly = new QLabel(tr("The selected notes have different entries, which "
                                          "cannot be edited together. The entries of the first "
                                          "note are shown."));
            readOnly->setWordWrap(true);
            layout->addWidget(readOnly);
        }
        layout->addWidget(builtInGroup, 1);
        layout->addWidget(userDataGroup, 1);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &NoteEntriesDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
        resize(560, 480);

        connect(m_userData, &QPlainTextEdit::textChanged, this, &NoteEntriesDialog::updateWarnings);
        updateWarnings();
    }

    NoteEntriesDialog::~NoteEntriesDialog() = default;

    bool NoteEntriesDialog::isEditable() const {
        return m_editable;
    }

    std::optional<kit::Note> NoteEntriesDialog::editedNote(kit::DiagnosticList &diagnostics) const {
        using Entries = kit::UstNoteEntries;
        // The lines are numbered in each box, which the messages therefore name.
        kit::DiagnosticList builtInErrors;
        kit::DiagnosticList userDataErrors;
        auto note = Entries::withBuiltInLines(m_note, linesOf(m_builtIn), builtInErrors);
        const auto userData = Entries::userDataOf(linesOf(m_userData), userDataErrors);
        for (auto error : std::as_const(builtInErrors)) {
            error.message = tr("Built-in entries: %1").arg(error.message);
            diagnostics.push_back(error);
        }
        for (auto error : std::as_const(userDataErrors)) {
            error.message = tr("User data: %1").arg(error.message);
            diagnostics.push_back(error);
        }
        if (!note || !userData) {
            return std::nullopt;
        }
        note->userData = *userData;
        return note;
    }

    std::optional<kit::Note> NoteEntriesDialog::others() const {
        return m_others;
    }

    QPlainTextEdit *NoteEntriesDialog::builtInEdit() const {
        return m_builtIn;
    }

    QPlainTextEdit *NoteEntriesDialog::userDataEdit() const {
        return m_userData;
    }

    QLabel *NoteEntriesDialog::warningLabel() const {
        return m_warnings;
    }

    void NoteEntriesDialog::accept() {
        m_others.reset();
        if (m_editable) {
            kit::DiagnosticList diagnostics;
            const auto note = editedNote(diagnostics);
            if (!note) {
                DiagnosticBox::report(this, windowTitle(), diagnostics);
                return;
            }
            using Entries = kit::UstNoteEntries;
            if (Entries::builtInLinesOf(*note) != Entries::builtInLinesOf(m_note) ||
                note->userData != m_note.userData) {
                m_others = note;
            }
        }
        QDialog::accept();
    }

    // The warnings follow the lines that read as user data. A line that does not read as user
    // data is reported when the dialog is accepted.
    void NoteEntriesDialog::updateWarnings() {
        kit::DiagnosticList diagnostics;
        const auto userData = kit::UstNoteEntries::userDataOf(linesOf(m_userData), diagnostics);
        QStringList messages;
        if (userData) {
            for (const auto &warning : kit::UstNoteEntries::utauChangesOf(*userData)) {
                messages.push_back(warning.message.toHtmlEscaped());
            }
        }
        // The warnings are bold and in color, as in the warning of ProjectPropertiesDialog.
        m_warnings->setText(
            messages.isEmpty()
                ? QString()
                : QStringLiteral("<span style=\"color:%1; font-weight:bold;\">%2</span>")
                      .arg(m_warningColor.name(), messages.join(QStringLiteral("<br>"))));
        m_warnings->setVisible(!messages.isEmpty());
    }

    QColor NoteEntriesDialog::warningColor() const {
        return m_warningColor;
    }

    void NoteEntriesDialog::setWarningColor(const QColor &color) {
        m_warningColor = color;
        updateWarnings();
    }

}
