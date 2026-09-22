#ifndef HELLOKIT_INTERCHANGE_INTERCHANGESELECTOR_H
#define HELLOKIT_INTERCHANGE_INTERCHANGESELECTOR_H

#include <optional>

#include <QtCore/QCoreApplication>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>
#include <hellokit/Interchange/InterchangeSource.h>

namespace hello::kit {

    class InterchangeReader;
    class InterchangeWriter;

    /// Supplies the settings that a file does not specify, so that a driver can proceed.
    ///
    /// Implemented by the user interface layer, not by this library, which does not link
    /// QtWidgets and therefore cannot display a dialog. A driver obtains user decisions through
    /// this interface instead.
    ///
    /// Called once per operation rather than once per setting. Otherwise track selection and
    /// encoding selection would each require a separate dialog.
    ///
    /// \warning The calling thread is unspecified, and the implementation is responsible for
    ///          thread safety. A Qt dialog can be opened only on the GUI thread, so an import
    ///          running on a worker thread must marshal these calls to the GUI thread, for
    ///          example with a blocking queued connection.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeSelector {
    public:
        virtual ~InterchangeSelector();

        /// \return the import request, or \c std::nullopt if the user cancelled. Cancellation
        ///         is not an error and must not be reported as one.
        /// \note \c std::nullopt has two meanings, distinguished by \a diagnostics . With an
        ///       \c Error recorded, the user could not be asked at all, for example because the
        ///       file contains no selectable entry. Without an error, the user declined. An
        ///       implementation that fails must record an error, otherwise the failure reaches
        ///       the caller as a cancellation and is silently ignored.
        /// \param reader the calling driver, whose \c optionSchema() specifies the settings to
        ///        offer
        /// \param limits the capacity of the import destination
        virtual std::optional<ImportRequest> selectImport(const InterchangeReader &reader,
                                                          const InterchangeSource &source,
                                                          const ImportLimits &limits,
                                                          DiagnosticList &diagnostics) = 0;

        virtual std::optional<ExportRequest> selectExport(const InterchangeWriter &writer,
                                                          const Project &project,
                                                          DiagnosticList &diagnostics) = 0;
    };

    /// Selects the default for every setting and reports each selection.
    ///
    /// Used by a driver that received no selector, as in the command-line tools, batch
    /// conversion and the tests. A test that opened a dialog would hang instead of failing, so
    /// a selector that requires no user is necessary.
    ///
    /// Every decision made on behalf of the user is recorded as a \c Note diagnostic, because
    /// the caller has no other means of learning that a choice was made.
    class HELLOKIT_INTERCHANGE_EXPORT AutomaticSelector : public InterchangeSelector {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::AutomaticSelector)
    public:
        AutomaticSelector();
        ~AutomaticSelector();

        /// Selects the entries in order, up to \c ImportLimits::maxEntries , and the declared
        /// default of every option.
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
