#ifndef UTAUCOMPARE_UTAUSCRIPT_H
#define UTAUCOMPARE_UTAUSCRIPT_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace utaucompare {

    /// One note, as UTAU's own render script renders it.
    struct ScriptCall {
        /// The note UTAU says this is, which is the last argument it hands its helper. Absent
        /// for a note rendered without the helper, which is what a rest looks like.
        std::optional<int> noteIndex;

        /// What UTAU hands the resampler. Empty where it runs none, which is a rest.
        QStringList resamplerArguments;

        /// What UTAU hands the wavtool, which runs for every note.
        QStringList wavtoolArguments;
    };

    /// Reads the script UTAU wrote for a render.
    ///
    /// This is the one thing in the comparison that is not ours, so it is read rather than
    /// assumed: the variables are expanded and the line is split into arguments the way a
    /// command processor would, and the two engine command lines come out of UTAU's helper
    /// script rather than being written down here. What UTAU hands its engines is then whatever
    /// UTAU wrote, in whatever order it wrote it, and a change on UTAU's side shows up as a
    /// different argument rather than as a reader that silently reads the wrong field.
    ///
    /// The helper is skipped over as a program: its \c "if exist" guard is ignored, because the
    /// question is what UTAU would hand the resampler, not whether a cache file happened to be
    /// there when it ran.
    ///
    /// \param charset what the script is written in. UTAU writes it in the code page it is
    ///        running under, which is not the project's encoding and is not UTF-8.
    ///
    /// \return the calls in the order the script makes them, or nothing with the reason in
    ///         \a error
    std::optional<QList<ScriptCall>> readScript(const std::filesystem::path &script,
                                                const QString &charset, QString *error);

}

#endif // UTAUCOMPARE_UTAUSCRIPT_H
