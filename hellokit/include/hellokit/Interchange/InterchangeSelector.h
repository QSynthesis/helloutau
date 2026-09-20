#ifndef HELLOKIT_INTERCHANGE_INTERCHANGESELECTOR_H
#define HELLOKIT_INTERCHANGE_INTERCHANGESELECTOR_H

#include <optional>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>
#include <hellokit/Interchange/InterchangeSource.h>

namespace hello::kit {

    class InterchangeReader;
    class InterchangeWriter;

    /// Answers the things a file did not say, so that a driver can go on.
    ///
    /// Implemented wherever there is a user interface, which is not here: this library does not
    /// link QtWidgets, so a dialog cannot appear anywhere in this module. A driver calls out
    /// through this instead.
    ///
    /// Asked once per operation rather than once per question. Splitting it up would put a
    /// separate dialog on screen for picking a track and another for picking an encoding.
    ///
    /// \warning Nothing here says which thread it runs on, and the implementor has to settle
    ///          that. A Qt dialog only opens on the GUI thread, so an import moved onto a worker
    ///          thread has to marshal these calls back, for instance with a blocking queued
    ///          connection.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeSelector {
    public:
        virtual ~InterchangeSelector();

        /// \return what to import, or nothing where the user cancelled. Cancelling is not an
        ///         error and must not be reported as one.
        /// \note Returning nothing means two different things, and \a diagnostics is what tells
        ///       them apart. Nothing with an \c Error recorded means the question could not be
        ///       put at all, for instance because the file holds no part that could be chosen.
        ///       Nothing with no error recorded means the user declined. An implementation that
        ///       gives up has to say so here, or a failure arrives at the caller looking like a
        ///       cancellation and is passed over in silence.
        /// \param reader the driver asking, whose \c optionSchema() says which settings to offer
        /// \param limits how much the destination can take
        virtual std::optional<ImportRequest> selectImport(const InterchangeReader &reader,
                                                          const InterchangeSource &source,
                                                          const ImportLimits &limits,
                                                          DiagnosticList &diagnostics) = 0;

        virtual std::optional<ExportRequest> selectExport(const InterchangeWriter &writer,
                                                          const Project &project,
                                                          DiagnosticList &diagnostics) = 0;
    };

    /// Answers every question with its default and says so.
    ///
    /// What a driver uses when it was handed no selector, which is the case for the command line
    /// tools, for batch conversion and for the tests. A test that reached a dialog would hang
    /// rather than fail, so there has to be an answer available without a user.
    ///
    /// Every decision it makes on the user's behalf is recorded as a \c Note diagnostic, since
    /// the caller has no other way to find out that a choice was made at all.
    class HELLOKIT_INTERCHANGE_EXPORT AutomaticSelector : public InterchangeSelector {
    public:
        AutomaticSelector();
        ~AutomaticSelector();

        /// Takes the entries in order, up to \c ImportLimits::maxEntries, and every option's
        /// declared default.
        std::optional<ImportRequest> selectImport(const InterchangeReader &reader,
                                                  const InterchangeSource &source,
                                                  const ImportLimits &limits,
                                                  DiagnosticList &diagnostics) override;

        std::optional<ExportRequest> selectExport(const InterchangeWriter &writer,
                                                  const Project &project,
                                                  DiagnosticList &diagnostics) override;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGESELECTOR_H
