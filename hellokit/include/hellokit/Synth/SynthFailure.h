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

    /// The record of a synth tool call whose failure was observed, kept for the diagnosis. A
    /// render records only the failures that it observes. The format of a fragment is never
    /// examined, because a resampler may write an intermediate format that its own wavtool
    /// joins. See docs/SynthDiagnostics.md.
    struct HELLOKIT_SYNTH_EXPORT SynthFailure {
        enum Stage {
            /// The resampler wrote no fragment.
            Resampler,

            /// The wavtool did not start, failed for the note without appending to the track,
            /// or wrote no track.
            Wavtool,

            /// WavtoolMixer cannot read the fragment, in realtime playback.
            Fragment,

            /// The wavtool wrote no track, and the fragment of the note exists. The classic runner
            /// observes no individual wavtool call and records every existing fragment, because
            /// the fragments that the wavtool rejected are unknown. Only a diagnoser that
            /// recognizes the content of a fragment claims such a failure.
            Unconfirmed,
        };

        Stage stage = Resampler;

        /// The note, or \c std::nullopt for the whole track
        std::optional<int> noteIndex;

        std::filesystem::path program;
        QStringList arguments;

        /// The result of the call, or \c std::nullopt if unknown, as for the calls of the script
        /// of the classic runner
        std::optional<SynthToolRun> run;

        /// The file that the diagnosis examines: the fragment of the note, or the track if the
        /// failure concerns the whole track
        std::filesystem::path file;

        /// The size of file, or -1 if the file does not exist
        qint64 fileSize = -1;

        /// The first \c headSize bytes of file, empty if the file does not exist
        QByteArray fileHead;

        static constexpr qsizetype headSize = 256;

        /// Reads fileSize and fileHead from file.
        void readFile();

        /// Returns whether \a head is the start of a RIFF/WAVE file.
        static bool isWave(QByteArrayView head);
    };

}

#endif // HELLOKIT_SYNTH_SYNTHFAILURE_H
