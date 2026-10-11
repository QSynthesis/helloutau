#ifndef HELLOUTAU_EDITOR_DIALOGS_DIAGNOSTICBOX_H
#define HELLOUTAU_EDITOR_DIALOGS_DIAGNOSTICBOX_H

#include <QtCore/QString>
#include <QtWidgets/QMessageBox>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Synth/SynthDiagnoser.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// A message box of diagnostics, one per line, with the icon of the most severe entry. A
    /// diagnostic of a note is prefixed with the index of the note.
    class HELLOUTAU_EDITOR_EXPORT DiagnosticBox : public QMessageBox {
        Q_OBJECT
    public:
        DiagnosticBox(const QString &title, const kit::DiagnosticList &diagnostics,
                      QWidget *parent = nullptr);

        /// Constructs the box with the probable causes of \a diagnoses, each with its
        /// explanation and remedies, above the details, which hold \a diagnostics. Without
        /// \a diagnoses the box is that of the other constructor. See docs/SynthDiagnostics.md.
        DiagnosticBox(const QString &title, const kit::DiagnosticList &diagnostics,
                      const QList<kit::SynthDiagnosis> &diagnoses, QWidget *parent = nullptr);

        ~DiagnosticBox();

        /// Shows \a diagnostics in a modal box titled \a title, or nothing if \a diagnostics is
        /// empty, so that the caller passes the diagnostics of an operation whatever its result.
        static void report(QWidget *parent, const QString &title,
                           const kit::DiagnosticList &diagnostics);

        /// Shows \a diagnostics with the probable causes of \a diagnoses, or nothing if both are
        /// empty.
        static void report(QWidget *parent, const QString &title,
                           const kit::DiagnosticList &diagnostics,
                           const QList<kit::SynthDiagnosis> &diagnoses);
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_DIAGNOSTICBOX_H
