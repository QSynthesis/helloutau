#include "PresetSelector.h"

#include <algorithm>

namespace hello::daw {

    namespace {

        void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::DiagnosticSeverity::Error, message, std::nullopt});
        }

    }

    PresetSelector::PresetSelector(kit::ImportRequest request, int entryCount)
        : m_import(std::move(request)), m_entryCount(entryCount) {
    }

    PresetSelector::PresetSelector(kit::ExportRequest request) : m_export(std::move(request)) {
    }

    PresetSelector::~PresetSelector() = default;

    std::optional<kit::ImportRequest> PresetSelector::selectImport(
        const kit::InterchangeReader &reader, const kit::InterchangeSource &source,
        const kit::ImportLimits &limits, kit::DiagnosticList &diagnostics) {
        Q_UNUSED(reader);
        if (!m_import) {
            fail(diagnostics, tr("No import was prepared."));
            return std::nullopt;
        }
        const auto changed = [&] {
            fail(diagnostics,
                 tr("The file was modified during the import. Import the file again."));
            return std::nullopt;
        };
        if (source.entries.size() != m_entryCount) {
            return changed();
        }
        const auto count = m_import->entries.size();
        if (count < limits.minEntries || count > limits.maxEntries) {
            fail(diagnostics, tr("The number of chosen entries is outside the range allowed by "
                                 "the project."));
            return std::nullopt;
        }
        for (const int index : std::as_const(m_import->entries)) {
            if (std::none_of(
                    source.entries.begin(), source.entries.end(),
                    [index](const kit::InterchangeEntry &entry) { return entry.index == index; })) {
                return changed();
            }
        }
        return m_import;
    }

    std::optional<kit::ExportRequest>
        PresetSelector::selectExport(const kit::InterchangeWriter &writer,
                                     const kit::Project &project,
                                     kit::DiagnosticList &diagnostics) {
        Q_UNUSED(writer);
        Q_UNUSED(project);
        if (!m_export) {
            fail(diagnostics, tr("No export was prepared."));
            return std::nullopt;
        }
        return m_export;
    }

}
