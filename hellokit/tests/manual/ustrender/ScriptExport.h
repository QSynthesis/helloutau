#ifndef USTRENDER_SCRIPTEXPORT_H
#define USTRENDER_SCRIPTEXPORT_H

#include <filesystem>
#include <optional>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Synth/SynthPlan.h>

/// The rendering scripts that compare a synth tool run in two environments step by step, the
/// same calls in the same order with the same arguments but for the roots of the paths. See
/// docs/claude/render-comparison.md.
struct ScriptExport {
    /// The system the script runs on: \c temp.bat for Windows, \c temp.sh for Linux.
    enum Target {
        Windows,
        Linux,
    };

    Target target = Windows;

    /// The project file, as recorded in the manifest.
    QString project;

    /// The voice bank the plan was made from, as this program read it, and the directory that
    /// stands for it in the script, if another. The scripts of both targets are made from the
    /// same copy, so that the names of the fragments, which depend on the samples, agree.
    std::filesystem::path voice;
    QString voiceAs;

    /// The directory of the scripts, which is also the working directory of the synth tools, where
    /// moresampler looks for temp.bat. As the script refers to it.
    QString scriptDirectory;

    /// Where the files are written, if not scriptDirectory itself.
    std::filesystem::path emitDirectory;

    /// Where each step leaves copies of what it wrote, or empty for no copies.
    QString snapshotDirectory;

    /// The programs, each as the arguments that precede those of a call: the path of the exe,
    /// or a loader followed by it.
    QStringList resamplerCommand;
    QStringList wavtoolCommand;

    /// Whether the last sung note passes LAST_NOTE to the wavtool, the other way moresampler
    /// learns that the track ends.
    bool lastNote = false;

    /// Writes the script, the manifest and, for Linux, the temp.bat that moresampler reads
    /// but that is not executed. Returns the exit code of the program.
    int write(const hello::kit::SynthPlan &plan) const;
};

/// Compares two manifests with the roots of their paths replaced by placeholders, and reports
/// the first difference. Returns 0 if they agree.
int compareManifests(const std::filesystem::path &first, const std::filesystem::path &second);

/// Copies a voice bank without the files that synth tools derive from its samples, so that an
/// synth tool analyses every sample again. Returns the exit code of the program.
int copyVoice(const std::filesystem::path &from, const std::filesystem::path &to);

#endif // USTRENDER_SCRIPTEXPORT_H
