#include "InterchangeReader.h"

#include <QCoreApplication>

#include <hellokit/Interchange/InterchangeSelector.h>

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
            // inspect() is expected to have said why. Saying nothing at all would leave the
            // caller with an empty result and no way to tell it from a cancellation.
            if (!hasError(result.diagnostics)) {
                result.diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    QCoreApplication::translate("hello::kit::InterchangeReader",
                                                "The file could not be read."),
                });
            }
            return result;
        }

        AutomaticSelector fallback;
        const int mark = int(result.diagnostics.size());
        auto request = (selector ? *selector : static_cast<InterchangeSelector &>(fallback))
                           .selectImport(*this, *source, limits, result.diagnostics);
        if (!request) {
            // Nothing came back, for one of two reasons the return type cannot tell apart. An
            // error the selector recorded means the question could not be put, which is a
            // failure. Nothing recorded means the user said no, which is not.
            result.cancelled = !hasError(result.diagnostics.mid(mark));
            return result;
        }

        result.project = convert(path, *source, *request, result.diagnostics);
        if (!result.project && !hasError(result.diagnostics)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                QCoreApplication::translate("hello::kit::InterchangeReader",
                                            "The file could not be converted."),
            });
        }
        return result;
    }

}
