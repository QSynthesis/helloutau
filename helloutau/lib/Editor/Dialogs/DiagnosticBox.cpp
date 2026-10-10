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
        for (const auto &diagnostic : diagnostics) {
            lines.push_back(
                diagnostic.noteIndex
                    ? tr("Note %1: %2").arg(*diagnostic.noteIndex).arg(diagnostic.message)
                    : diagnostic.message);
        }
        setText(lines.join(u'\n'));
    }

    DiagnosticBox::~DiagnosticBox() = default;

    void DiagnosticBox::report(QWidget *parent, const QString &title,
                               const kit::DiagnosticList &diagnostics) {
        if (diagnostics.isEmpty()) {
            return;
        }
        DiagnosticBox box(title, diagnostics, parent);
        box.exec();
    }

}
