#include "Diagnostic.h"

#include <algorithm>

namespace hello::kit {

    bool hasError(const DiagnosticList &diagnostics) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic &d) {
            return d.severity == DiagnosticSeverity::Error;
        });
    }

}
