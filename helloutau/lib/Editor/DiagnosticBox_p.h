#ifndef HELLOUTAU_EDITOR_DIAGNOSTICBOX_P_H
#define HELLOUTAU_EDITOR_DIAGNOSTICBOX_P_H

#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

class QWidget;

namespace hello::daw::DiagnosticBox {

    /// Shows \a diagnostics in a message box titled \a title, with the icon of the most severe
    /// entry. Shows nothing if \a diagnostics is empty.
    void show(QWidget *parent, const QString &title, const kit::DiagnosticList &diagnostics);

}

#endif // HELLOUTAU_EDITOR_DIAGNOSTICBOX_P_H
