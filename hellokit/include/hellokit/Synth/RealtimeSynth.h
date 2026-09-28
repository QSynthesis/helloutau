#ifndef HELLOKIT_SYNTH_REALTIMESYNTH_H
#define HELLOKIT_SYNTH_REALTIMESYNTH_H

#include <chrono>
#include <functional>
#include <memory>

#include <QtCore/QCoreApplication>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/EngineProcess.h>
#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/SynthRunner.h>

namespace hello::kit {

    /// Renders a track for playback while it plays: resamples the notes in the order the
    /// playback position requires, and mixes any part of the track in the process as the
    /// wavtool would. See the section on realtime rendering in docs/Synth.md.
    ///
    /// Worker threads run the resampler, one note at a time each, always taking the note
    /// nearest after the playback position, then the notes before it. A fragment already in the
    /// cache directory is read instead of rendered. A note the resampler fails to render is
    /// silent, with a warning, so that playback does not wait for it forever.
    ///
    /// After an edit the new plan replaces the old one. Fragments are identified by their cache
    /// file, whose name stands for every input of the note, so a note that did not change keeps
    /// its fragment and a changed one is rendered anew without further analysis.
    ///
    /// Every function may be called from any thread. A running engine call is never stopped,
    /// since that would leave a partial fragment in the cache; destruction waits for them.
    class HELLOKIT_SYNTH_EXPORT RealtimeSynth {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::RealtimeSynth)
    public:
        using EngineFactory = std::function<std::unique_ptr<EngineProcess>()>;

        /// \param threadCount the number of worker threads, or zero for one per hardware thread
        /// \param engineFactory creates the object that starts the resampler, which tests
        ///        replace; by default an EngineProcess
        explicit RealtimeSynth(SynthEngines engines, int threadCount = 0,
                               EngineFactory engineFactory = {});
        ~RealtimeSynth();

        RealtimeSynth(const RealtimeSynth &) = delete;
        RealtimeSynth &operator=(const RealtimeSynth &) = delete;

        /// Replaces the plan, which must cover the whole track. Only its resampler arguments,
        /// cache files and wavtool arguments are used; its track file is never written.
        void setPlan(const SynthPlan &plan);

        /// Sets the playback position, in samples of the track file, from which the notes are
        /// rendered first.
        void setPosition(qint64 sample);

        /// The length of the track file in samples at 44100 Hz.
        qint64 length() const;

        /// The position in the track, in milliseconds, of the first sample of the track file.
        /// \sa SynthPlan::startTime()
        double startTime() const;

        /// Returns whether every note that sounds in samples \a first to \a first + \a count is
        /// rendered, or has failed.
        bool isReady(qint64 first, qint64 count) const;

        /// Waits until isReady() holds for the samples, or \a timeout passes, and returns it.
        bool waitReady(qint64 first, qint64 count, std::chrono::milliseconds timeout) const;

        /// Writes samples \a first to \a first + \a count of the track into \a out if isReady()
        /// holds for them, and returns whether it did.
        bool mix(qint64 first, qint64 count, qint16 *out) const;

        /// The number of notes still to render.
        int pendingCount() const;

        /// Returns the warnings of the notes that failed since the last call, and forgets them.
        DiagnosticList takeDiagnostics();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_SYNTH_REALTIMESYNTH_H
