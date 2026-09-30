#ifndef HELLOUTAU_INTERCHANGE_PRESETSELECTOR_H
#define HELLOUTAU_INTERCHANGE_PRESETSELECTOR_H

#include <optional>

#include <hellokit/Interchange/InterchangeSelector.h>

#include <Interchange/InterchangePluginGlobal.h>

namespace hello::daw {

    /// A selector that returns a request prepared in advance by a wizard. See
    /// docs/ImportExport.md.
    ///
    /// The import wizard inspects the file to display its entries and then calls
    /// \c InterchangeReader::read(), which inspects the file again. If the entries of the second
    /// inspection do not match the prepared request, selectImport() reports an error, so that a
    /// file modified between the two inspections is not imported with a stale request.
    class INTERCHANGEPLUGIN_EXPORT PresetSelector : public kit::InterchangeSelector {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::PresetSelector)
    public:
        /// Constructs a selector that returns \a request for an import of a file with
        /// \a entryCount entries.
        PresetSelector(kit::ImportRequest request, int entryCount);

        /// Constructs a selector that returns \a request for an export.
        explicit PresetSelector(kit::ExportRequest request);

        ~PresetSelector() override;

        /// Returns the prepared import request. Reports an error and returns \c std::nullopt if
        /// no import request was prepared, if the entry count or the entry indices of \a source
        /// differ from the prepared request, or if the number of chosen entries violates
        /// \a limits.
        std::optional<kit::ImportRequest> selectImport(const kit::InterchangeReader &reader,
                                                       const kit::InterchangeSource &source,
                                                       const kit::ImportLimits &limits,
                                                       kit::DiagnosticList &diagnostics) override;

        /// Returns the prepared export request. Reports an error and returns \c std::nullopt if
        /// no export request was prepared.
        std::optional<kit::ExportRequest> selectExport(const kit::InterchangeWriter &writer,
                                                       const kit::Project &project,
                                                       kit::DiagnosticList &diagnostics) override;

    private:
        std::optional<kit::ImportRequest> m_import;
        int m_entryCount = 0;
        std::optional<kit::ExportRequest> m_export;
    };

}

#endif // HELLOUTAU_INTERCHANGE_PRESETSELECTOR_H
