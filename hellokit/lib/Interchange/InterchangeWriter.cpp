#include "InterchangeWriter.h"

#include <QtCore/QCoreApplication>

#include <hellokit/Interchange/InterchangeSelector.h>

namespace hello::kit {

    InterchangeWriter::~InterchangeWriter() = default;

    QList<InterchangeOption> InterchangeWriter::optionSchema() const {
        return {};
    }

    QString InterchangeWriter::customStepId() const {
        return {};
    }

    ExportResult InterchangeWriter::write(const Project &project,
                                          const std::filesystem::path &path,
                                          InterchangeSelector *selector) {
        ExportResult result;

        AutomaticSelector fallback;
        const int mark = int(result.diagnostics.size());
        auto request = (selector ? *selector : static_cast<InterchangeSelector &>(fallback))
                           .selectExport(*this, project, result.diagnostics);
        if (!request) {
            // See InterchangeReader::read() for why the diagnostics decide this.
            result.cancelled = !hasError(result.diagnostics.mid(mark));
            return result;
        }

        result.written = convert(project, path, *request, result.diagnostics);
        if (!result.written && !hasError(result.diagnostics)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                QCoreApplication::translate("hello::kit::InterchangeWriter",
                                            "The file could not be written."),
            });
        }
        return result;
    }

}
