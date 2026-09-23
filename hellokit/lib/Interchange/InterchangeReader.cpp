#include "InterchangeReader.h"

#include <QtCore/QCoreApplication>

#include "InterchangeSelector.h"

namespace hello::kit {

    InterchangeReader::~InterchangeReader() = default;

    QList<InterchangeOption> InterchangeReader::optionSchema() const {
        return {};
    }

    QString InterchangeReader::customStepId() const {
        return {};
    }

    ImportResult InterchangeReader::read(const std::filesystem::path &path,
                                         InterchangeSelector *selector,
                                         const ImportLimits &limits) {
        ImportResult result;

        auto source = inspect(path, result.diagnostics);
        if (!source) {
            // inspect() is expected to have reported the reason. Otherwise the caller would
            // receive an empty result indistinguishable from a cancellation.
            if (!hasError(result.diagnostics)) {
                result.diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    tr("The file could not be read."),
                });
            }
            return result;
        }

        AutomaticSelector fallback;
        const int mark = int(result.diagnostics.size());
        auto request = (selector ? *selector : static_cast<InterchangeSelector &>(fallback))
                           .selectImport(*this, *source, limits, result.diagnostics);
        if (!request) {
            // No request was returned, for one of two reasons that the return type cannot
            // distinguish. An error recorded by the selector means that the user could not be
            // asked, which is a failure. No recorded error means that the user declined, which is
            // not.
            result.cancelled = !hasError(result.diagnostics.mid(mark));
            return result;
        }

        result.project = convert(path, *source, *request, result.diagnostics);
        if (!result.project && !hasError(result.diagnostics)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                tr("The file could not be converted."),
            });
        }
        return result;
    }

}
