#include "DiagnosticBox_p.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtWidgets/QMessageBox>

namespace hello::daw {

    void DiagnosticBox::show(QWidget *parent, const QString &title,
                             const kit::DiagnosticList &diagnostics) {
        if (diagnostics.isEmpty()) {
            return;
        }

        auto severity = kit::DiagnosticSeverity::Note;
        QStringList lines;
        for (const auto &diagnostic : diagnostics) {
            severity = std::max(severity, diagnostic.severity);
            lines.push_back(diagnostic.noteIndex ? QCoreApplication::translate(
                                                       "hello::daw::DiagnosticBox", "Note %1: %2")
                                                       .arg(*diagnostic.noteIndex)
                                                       .arg(diagnostic.message)
                                                 : diagnostic.message);
        }

        const auto icon = severity == kit::DiagnosticSeverity::Error     ? QMessageBox::Critical
                          : severity == kit::DiagnosticSeverity::Warning ? QMessageBox::Warning
                                                                         : QMessageBox::Information;
        QMessageBox box(icon, title, lines.join(u'\n'), QMessageBox::Ok, parent);
        box.exec();
    }

}
