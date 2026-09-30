#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGEWIZARDSUPPORT_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGEWIZARDSUPPORT_H

#include <filesystem>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

class QListWidget;

namespace hello::daw {

    /// Helper functions of the import and export wizards.
    class InterchangeWizardSupport {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::InterchangeWizardSupport)
    public:
        static QString textOf(const std::filesystem::path &path);
        static std::filesystem::path pathOf(const QString &text);

        /// Returns a file dialog filter named \a name for the extensions \a suffixes.
        static QString filterOf(const QString &name, const QStringList &suffixes);

        /// Lists \a diagnostics in \a list, each with the icon of its severity.
        static void showDiagnostics(QListWidget *list, const kit::DiagnosticList &diagnostics);
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGEWIZARDSUPPORT_H
