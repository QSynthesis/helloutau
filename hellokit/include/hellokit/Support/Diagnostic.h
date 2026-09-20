#ifndef HELLOKIT_SUPPORT_DIAGNOSTIC_H
#define HELLOKIT_SUPPORT_DIAGNOSTIC_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    enum class DiagnosticSeverity {
        /// Something was decided or left out, and the result is still what was asked for. A
        /// choice made on the user's behalf is reported at this level.
        Note,

        /// The result is usable but is not what the input said. Anything dropped in a lossy
        /// conversion belongs here.
        Warning,

        /// The operation did not produce a result.
        Error,
    };

    /// One thing worth telling the user about an operation that has already run.
    ///
    /// Reading a project, scanning a voice bank and converting a foreign format all lose or
    /// change things that the input said, and losing them quietly is the failure mode to avoid.
    /// A diagnostic list is therefore part of what those operations return rather than something
    /// written to a log, so that the caller has to decide what to do with it.
    ///
    /// \note Every message here is shown to the user, so it is written for the user. It says what
    ///       happened to their data, not which function noticed.
    struct Diagnostic {
        DiagnosticSeverity severity = DiagnosticSeverity::Note;
        QString message;

        /// Which note this is about, where it is about one. Indices are into the track the
        /// operation produced, not into whatever the input numbered them.
        std::optional<int> noteIndex;
    };

    using DiagnosticList = QList<Diagnostic>;

    /// Whether \a diagnostics holds anything at \c Error.
    HELLOKIT_SUPPORT_EXPORT bool hasError(const DiagnosticList &diagnostics);

}

#endif // HELLOKIT_SUPPORT_DIAGNOSTIC_H
