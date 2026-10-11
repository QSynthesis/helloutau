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

    DiagnosticBox::DiagnosticBox(const QString &title, const QString &summary,
                                 const kit::DiagnosticList &diagnostics,
                                 const QList<kit::SynthDiagnosis> &diagnoses, ListStyle style,
                                 QWidget *parent)
        : DiagnosticBox(title, diagnostics, parent) {
        if (diagnoses.isEmpty()) {
            return;
        }
        // A probable cause is a problem, even if the diagnostics are mere notes.
        if (icon() == QMessageBox::Information) {
            setIcon(QMessageBox::Warning);
        }
        const auto lines = text();
        QString html;
        if (!summary.isEmpty()) {
            html += QStringLiteral("<p>%1</p>").arg(summary.toHtmlEscaped());
        }
        const auto list = style == OrderedList ? QLatin1String("ol") : QLatin1String("ul");
        html += QStringLiteral("<p><b>%1</b></p><%2>")
                    .arg(tr("Probable causes:").toHtmlEscaped(), list);
        for (const auto &diagnosis : diagnoses) {
            html +=
                QStringLiteral("<li><b>%1</b><br>%2")
                    .arg(diagnosis.cause.toHtmlEscaped(), diagnosis.explanation.toHtmlEscaped());
            if (!diagnosis.remedies.isEmpty()) {
                html += QStringLiteral("<ul>");
                for (const auto &remedy : diagnosis.remedies) {
                    html += QStringLiteral("<li>%1</li>").arg(remedy.toHtmlEscaped());
                }
                html += QStringLiteral("</ul>");
            }
            html += QStringLiteral("</li>");
        }
        html += QStringLiteral("</%1>").arg(list);
        setTextFormat(Qt::RichText);
        setText(html);
        setDetailedText(lines);
    }

    DiagnosticBox::~DiagnosticBox() = default;

    void DiagnosticBox::report(QWidget *parent, const QString &title,
                               const kit::DiagnosticList &diagnostics) {
        report(parent, title, {}, diagnostics, {});
    }

    void DiagnosticBox::report(QWidget *parent, const QString &title, const QString &summary,
                               const kit::DiagnosticList &diagnostics,
                               const QList<kit::SynthDiagnosis> &diagnoses, ListStyle style) {
        if (diagnostics.isEmpty() && diagnoses.isEmpty()) {
            return;
        }
        DiagnosticBox box(title, summary, diagnostics, diagnoses, style, parent);
        box.exec();
    }

}
