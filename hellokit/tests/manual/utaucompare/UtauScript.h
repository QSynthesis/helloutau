#ifndef UTAUCOMPARE_UTAUSCRIPT_H
#define UTAUCOMPARE_UTAUSCRIPT_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace utaucompare {

    /// One note as rendered by the UTAU render script.
    struct ScriptCall {
        /// The note number assigned by UTAU, which is the last argument passed to its helper.
        /// Absent for a note rendered without the helper, which is the case for a rest.
        std::optional<int> noteIndex;

        /// The resampler arguments passed by UTAU. Empty if no resampler runs, as for a rest.
        QStringList resamplerArguments;

        /// The wavtool arguments passed by UTAU. The wavtool runs for every note.
        QStringList wavtoolArguments;
    };

    /// Reads the script UTAU wrote for a render.
    ///
    /// This script is the only external input of the comparison, so it is parsed rather than
    /// assumed: variables are expanded and lines are split into arguments as a command
    /// processor does, and the two synth tool command lines are taken from the UTAU helper script
    /// rather than defined here. The arguments therefore reflect exactly what UTAU wrote, in
    /// the order it wrote them, and a change in UTAU appears as a different argument rather than
    /// as a parser that silently reads the wrong field.
    ///
    /// The helper is not executed: its \c "if exist" guard is ignored, because the question is
    /// which arguments UTAU would pass to the resampler, not whether a cache file existed at
    /// the time.
    ///
    /// \param charset the encoding of the script. UTAU writes it in the ANSI code page of the
    ///        host, which is neither the project encoding nor UTF-8.
    ///
    /// \return the calls in script order, or \c std::nullopt with the reason in \a error
    std::optional<QList<ScriptCall>> readScript(const std::filesystem::path &script,
                                                const QString &charset, QString *error);

}

#endif // UTAUCOMPARE_UTAUSCRIPT_H
