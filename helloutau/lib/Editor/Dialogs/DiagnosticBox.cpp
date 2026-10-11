#include "DiagnosticBox.h"

#include <algorithm>

#include <QtCore/QStringList>

namespace hello::daw {

    namespace {

        // The icon of the most severe of diagnostics
        QMessageBox::Icon iconOf(const kit::DiagnosticList &diagnostics) {
            auto severity = kit::DiagnosticSeverity::Note;
            for (const auto &diagnostic : diagnostics) {
                severity = std::max(severity, diagnostic.severity);
            }
            switch (severity) {
                case kit::DiagnosticSeverity::Error:
                    return QMessageBox::Critical;
                case kit::DiagnosticSeverity::Warning:
                    return QMessageBox::Warning;
                default:
                    return QMessageBox::Information;
            }
        }

    }

    DiagnosticBox::DiagnosticBox(const QString &title, const kit::DiagnosticList &diagnostics,
                                 QWidget *parent)
        : QMessageBox(iconOf(diagnostics), title, QString(), QMessageBox::Ok, parent) {
        QStringList lines;
        // Notes are numbered from 1, as the region dialog and the import wizard number them.
        for (const auto &diagnostic : diagnostics) {
            lines.push_back(
                diagnostic.noteIndex
                    ? tr("Note %1: %2").arg(*diagnostic.noteIndex + 1).arg(diagnostic.message)
                    : diagnostic.message);
        }
        setText(lines.join(u'\n'));
    }

    DiagnosticBox::DiagnosticBox(const QString &title, const kit::DiagnosticList &diagnostics,
                                 const QList<kit::SynthDiagnosis> &diagnoses, QWidget *parent)
        : DiagnosticBox(title, diagnostics, parent) {
        if (diagnoses.isEmpty()) {
            return;
        }
        // A probable cause is a problem, even if the diagnostics are mere notes.
        if (icon() == QMessageBox::Information) {
            setIcon(QMessageBox::Warning);
        }
        const auto lines = text();
        QString html =
            QStringLiteral("<p><b>%1</b></p>").arg(tr("Probable causes:").toHtmlEscaped());
        for (const auto &diagnosis : diagnoses) {
            html +=
                QStringLiteral("<p><b>%1</b><br>%2</p>")
                    .arg(diagnosis.cause.toHtmlEscaped(), diagnosis.explanation.toHtmlEscaped());
            if (!diagnosis.remedies.isEmpty()) {
                html += QStringLiteral("<ul>");
                for (const auto &remedy : diagnosis.remedies) {
                    html += QStringLiteral("<li>%1</li>").arg(remedy.toHtmlEscaped());
                }
                html += QStringLiteral("</ul>");
            }
        }
        setTextFormat(Qt::RichText);
        setText(html);
        setDetailedText(lines);
    }

    DiagnosticBox::~DiagnosticBox() = default;

    void DiagnosticBox::report(QWidget *parent, const QString &title,
                               const kit::DiagnosticList &diagnostics) {
        report(parent, title, diagnostics, {});
    }

    void DiagnosticBox::report(QWidget *parent, const QString &title,
                               const kit::DiagnosticList &diagnostics,
                               const QList<kit::SynthDiagnosis> &diagnoses) {
        if (diagnostics.isEmpty() && diagnoses.isEmpty()) {
            return;
        }
        DiagnosticBox box(title, diagnostics, diagnoses, parent);
        box.exec();
    }

}
