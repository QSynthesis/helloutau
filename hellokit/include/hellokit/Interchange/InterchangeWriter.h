#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H

#include <filesystem>

#include <QString>
#include <QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>

namespace hello::kit {

    class InterchangeSelector;

    /// Writes a project out in one foreign format.
    ///
    /// \sa InterchangeReader, for why the two halves are separate classes
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeWriter {
    public:
        virtual ~InterchangeWriter();

        virtual QString id() const = 0;
        virtual QString name() const = 0;
        virtual QStringList suffixes() const = 0;

        virtual QList<InterchangeOption> optionSchema() const;
        virtual QString customStepId() const;

        /// Asks \a selector what to do and writes.
        ///
        /// \param selector where the user's answers come from, or null to take every default
        ///        through \c AutomaticSelector
        ExportResult write(const Project &project, const std::filesystem::path &path,
                           InterchangeSelector *selector);

    protected:
        /// Writes what \a request asked for. Called by write() once the questions are answered.
        virtual bool convert(const Project &project, const std::filesystem::path &path,
                             const ExportRequest &request, DiagnosticList &diagnostics) = 0;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEWRITER_H
