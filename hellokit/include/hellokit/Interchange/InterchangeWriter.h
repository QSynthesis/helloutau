#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H

#include <filesystem>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>

namespace hello::kit {

    class InterchangeSelector;

    /// Exports a project to one foreign format.
    ///
    /// See InterchangeReader for the reason the two directions are separate classes.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeWriter {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::InterchangeWriter)
    public:
        virtual ~InterchangeWriter();

        virtual QString id() const = 0;
        virtual QString name() const = 0;
        virtual QStringList suffixes() const = 0;

        virtual QList<InterchangeOption> optionSchema() const;
        virtual QString customStepId() const;

        /// Obtains the export settings from \a selector and writes the file.
        ///
        /// \param selector the source of user decisions, or null to accept every default
        ///        through \c AutomaticSelector
        ExportResult write(const Project &project, const std::filesystem::path &path,
                           InterchangeSelector *selector);

    protected:
        /// Writes the file as specified by \a request . Called by write() once all settings are
        /// determined.
        virtual bool convert(const Project &project, const std::filesystem::path &path,
                             const ExportRequest &request, DiagnosticList &diagnostics) = 0;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H
