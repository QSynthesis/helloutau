#ifndef HELLOKIT_SUPPORT_DIAGNOSTIC_H
#define HELLOKIT_SUPPORT_DIAGNOSTIC_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    enum class DiagnosticSeverity {
        /// The operation made a decision or omitted something, and the result still matches the
        /// request. Choices made on behalf of the user are reported at this level.
        Note,

        /// The result is usable but deviates from the input. Data dropped by a lossy conversion
        /// is reported at this level.
        Warning,

        /// The operation produced no result.
        Error,
    };

    /// A user-facing message about a completed operation.
    ///
    /// Reading a project, scanning a voice bank and converting a foreign format may each drop or
    /// alter input data, and silent data loss must be avoided. These operations therefore return
    /// a diagnostic list as part of their result instead of writing to a log, which obliges the
    /// caller to handle it.
    ///
    /// \note Messages are displayed to the user and are phrased accordingly. A message describes
    ///       the effect on the user's data, not the function that detected it.
    struct Diagnostic {
        DiagnosticSeverity severity = DiagnosticSeverity::Note;
        QString message;

        /// The index of the affected note, if any. The index refers to the track produced by the
        /// operation, not to the numbering of the input.
        std::optional<int> noteIndex;
    };

    using DiagnosticList = QList<Diagnostic>;

    /// Returns whether \a diagnostics contains an entry of severity \c Error.
    HELLOKIT_SUPPORT_EXPORT bool hasError(const DiagnosticList &diagnostics);

}

#endif // HELLOKIT_SUPPORT_DIAGNOSTIC_H
