#ifndef HELLOUTAU_EDITOR_SAMPLEPREVIEW_H
#define HELLOUTAU_EDITOR_SAMPLEPREVIEW_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QMap>
#include <QtCore/QObject>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Synth/EngineProcess.h>
#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Plays what the voice bank window lets the user hear of an oto entry: its audio file or a
    /// span of it, and a note that the resampler synthesizes from the entry. See the preview in
    /// docs/VoiceBankEditor.md.
    ///
    /// A note is synthesized on a worker thread by the resampler of the settings alone, without
    /// the wavtool and without a console: one call, whose output is the note as the resampler
    /// makes it from the offset, the consonant and the cutoff. The note is at 120 beats per
    /// minute, and its fragment is written into a temporary directory that lives as long as this
    /// object.
    class HELLOUTAU_EDITOR_EXPORT SamplePreview : public QObject {
        Q_OBJECT
    public:
        enum State {
            Stopped,
            Synthesizing,
            Playing,
        };
        Q_ENUM(State)

        explicit SamplePreview(QObject *parent = nullptr);
        ~SamplePreview() override;

        /// Replaces the object that starts the resampler, for tests.
        void setEngineProcess(std::shared_ptr<kit::EngineProcess> process);

        State state() const;

        /// Plays \a audio from \a from to \a to in milliseconds, or to its end, stopping what
        /// played before.
        ///
        /// \return whether playing started, with the reason in \a diagnostics otherwise
        bool play(std::shared_ptr<const kit::WaveAudio> audio, double from,
                  std::optional<double> to, kit::DiagnosticList &diagnostics);

        /// Synthesizes a note of \a sample at \a noteNum, \a length ticks long, with
        /// \a resampler, and plays it once synthesized. Stops what played before.
        ///
        /// \return whether synthesizing started; the reason is in \a diagnostics otherwise, and
        ///         the reason of a failure after the start in failed()
        bool synthesize(const kit::VoiceSample &sample, int noteNum, int length,
                        const std::filesystem::path &resampler, kit::DiagnosticList &diagnostics);

        /// Stops playing, and forgets a note being synthesized.
        void stop();

        /// The time of the audio file heard, while play() plays it.
        std::optional<double> position() const;

        /// The note last synthesized, as the resampler wrote it.
        std::shared_ptr<const kit::WaveAudio> synthesized() const;

        /// Returns the note at which the voice bank sings an entry of the folder \a directory
        /// with \a alias, by \a prefixMap: of the keys whose prefix is the folder, or whose
        /// prefix and suffix enclose the alias, the middle one; C4 if none.
        static int noteNumFor(const QMap<int, kit::VoicePrefix> &prefixMap,
                              const std::filesystem::path &directory, const QString &alias);

    Q_SIGNALS:
        void stateChanged(State state);

        /// Synthesizing or playing failed, for the reasons in \a diagnostics.
        void failed(const kit::DiagnosticList &diagnostics);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_SAMPLEPREVIEW_H
