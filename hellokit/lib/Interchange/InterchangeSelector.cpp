#include "InterchangeSelector.h"

#include <algorithm>

#include <QtCore/QCoreApplication>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    InterchangeSelector::~InterchangeSelector() = default;

    AutomaticSelector::AutomaticSelector() = default;

    AutomaticSelector::~AutomaticSelector() = default;

    // Every default taken here is a question the user never saw, so each one is reported. A
    // conversion that silently picked the first of five tracks would look like the file only
    // ever had one.
    static void fillDefaults(const QList<InterchangeOption> &schema, ImportRequest &request,
                             DiagnosticList &diagnostics) {
        for (const auto &option : schema) {
            request.driverOptions.insert(option.key, option.defaultValue);
            diagnostics.push_back({
                DiagnosticSeverity::Note,
                QCoreApplication::translate("hello::kit::AutomaticSelector",
                                            "%1 was left at its default, %2.")
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
                QCoreApplication::translate("hello::kit::AutomaticSelector",
                                            "The file holds nothing that can be imported."),
            });
            return std::nullopt;
        }

        if (source.entries.size() > wanted) {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                QCoreApplication::translate("hello::kit::AutomaticSelector",
                                            "The file holds %1 parts and the first %2 were taken.")
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
