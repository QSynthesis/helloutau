#ifndef HELLOKIT_SYNTH_SYNTHFAILURE_H
#define HELLOKIT_SYNTH_SYNTHFAILURE_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QStringList>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthToolProcess.h>

namespace hello::kit {

    /// The record of a synth tool call that failed or wrote a file that is not a wave file, kept
    /// for the diagnosis. See docs/SynthDiagnostics.md.
    struct HELLOKIT_SYNTH_EXPORT SynthFailure {
        enum Stage {
            /// The resampler wrote no fragment.
            Resampler,

            /// The wavtool did not start or wrote no track.
            Wavtool,

            /// The fragment exists but is not a RIFF/WAVE file, whether or not the track was
            /// written.
            Fragment,
        };

        Stage stage = Resampler;

        /// The note, or \c std::nullopt for the whole track
        std::optional<int> noteIndex;

        std::filesystem::path program;
        QStringList arguments;

        /// The result of the call, or \c std::nullopt if unknown, as for the calls of the script
        /// of the classic runner
        std::optional<SynthToolRun> run;

        std::filesystem::path outputFile;

        /// The size of outputFile, or -1 if the file does not exist
        qint64 outputSize = -1;

        /// The first \c headSize bytes of outputFile, empty if the file does not exist
        QByteArray outputHead;

        static constexpr qsizetype headSize = 256;

        /// Reads outputSize and outputHead from outputFile.
        void readOutput();

        /// Returns whether \a head is the start of a RIFF/WAVE file.
        static bool isWave(QByteArrayView head);
    };

}

#endif // HELLOKIT_SYNTH_SYNTHFAILURE_H
