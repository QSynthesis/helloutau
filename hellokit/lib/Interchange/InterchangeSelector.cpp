#include "InterchangeSelector.h"

#include <algorithm>

#include <QtCore/QCoreApplication>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    InterchangeSelector::~InterchangeSelector() = default;

    AutomaticSelector::AutomaticSelector() = default;

    AutomaticSelector::~AutomaticSelector() = default;

    // Every default selected here is a decision the user did not make, so each one is reported.
    // A conversion that silently selected the first of five tracks would suggest that the file
    // contains only one.
    static void fillDefaults(const QList<InterchangeOption> &schema, ImportRequest &request,
                             DiagnosticList &diagnostics) {
        for (const auto &option : schema) {
            request.driverOptions.insert(option.key, option.defaultValue);
            diagnostics.push_back({
                DiagnosticSeverity::Note,
                AutomaticSelector::tr("%1 was set to its default value, %2.")
                    .arg(option.name, option.defaultValue.toString()),
            });
        }
    }

    std::optional<ImportRequest> AutomaticSelector::selectImport(const InterchangeReader &reader,
                                                                 const InterchangeSource &source,
                                                                 const ImportLimits &limits,
                                                                 DiagnosticList &diagnostics) {
        ImportRequest request;

        const int wanted = std::min<int>(limits.maxEntries, source.entries.size());
        for (int i = 0; i < wanted; ++i) {
            request.entries.push_back(source.entries.at(i).index);
        }

        if (request.entries.size() < limits.minEntries) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                tr("The file contains nothing that can be imported."),
            });
            return std::nullopt;
        }

        if (source.entries.size() > wanted) {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                tr("The file contains %1 parts, and the first %2 were imported.")
                    .arg(source.entries.size())
                    .arg(wanted),
            });
        }

        fillDefaults(reader.optionSchema(), request, diagnostics);
        return request;
    }

    std::optional<ExportRequest> AutomaticSelector::selectExport(const InterchangeWriter &writer,
                                                                 const Project &project,
                                                                 DiagnosticList &diagnostics) {
        Q_UNUSED(project)

        ImportRequest scratch;
        fillDefaults(writer.optionSchema(), scratch, diagnostics);

        ExportRequest request;
        request.driverOptions = scratch.driverOptions;
        return request;
    }

}
