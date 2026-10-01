#include "RealtimeSynth.h"

#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <thread>

#include <QtCore/QThread>

#include <stdcorelib/pimpl.h>

#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/Synth/WavtoolMixer.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        using Samples = std::shared_ptr<const std::vector<qint16>>;

        // A fragment read from a file of the resampler, whose samples are 16-bit at 44100 Hz.
        // Of several channels the first is taken.
        Samples readFragment(const fs::path &path) {
            std::error_code error;
            if (!fs::exists(path, error)) {
                return nullptr;
            }
            DiagnosticList ignored;
            const auto audio = WaveAudio::read(path, ignored);
            if (!audio) {
                return nullptr;
            }
            auto samples = std::make_shared<std::vector<qint16>>();
            samples->reserve(size_t(audio->frameCount()));
            for (qsizetype i = 0; i < audio->frameCount(); ++i) {
                const float value = audio->samples[size_t(i * audio->channels)];
                samples->push_back(qint16(std::clamp(std::lround(value * 32768), -32768L, 32767L)));
            }
            return samples;
        }

    }

    class RealtimeSynth::Impl {
    public:
        using Decl = RealtimeSynth;

        // A fragment is never Silent: silent steps have none.
        struct Fragment {
            NoteState state = Waiting;
            Samples samples;
        };

        SynthEngines engines;
        EngineFactory engineFactory;

        mutable std::mutex mutex;
        std::condition_variable work;
        mutable std::condition_variable changed;
        bool stopping = false;

        // From the current plan
        QList<SynthStep> steps;
        QList<WavtoolMixer::Segment> segments;
        fs::path cacheDirectory;
        double startTime = 0;
        qint64 position = 0;

        std::map<fs::path, Fragment> fragments;
        // The fragment of each step, null for a silent step. The queries under the lock go
        // through it, because the comparison of paths in fragments takes long enough to keep
        // the main thread waiting for the lock while the workers look for the next step.
        std::vector<Fragment *> fragmentOf;
        DiagnosticList diagnostics;
        std::vector<std::thread> workers;

        // Whether step \a index sounds and still needs its fragment
        bool needs(int index) const {
            const auto fragment = fragmentOf[size_t(index)];
            return fragment && (fragment->state == Waiting || fragment->state == Running);
        }

        bool ready(qint64 first, qint64 count) const {
            const qint64 last = first + count;
            for (int i = 0; i < segments.size(); ++i) {
                const auto &segment = segments[i];
                if (segment.start < last && segment.start + segment.length > first && needs(i)) {
                    return false;
                }
            }
            return true;
        }

        // The waiting step nearest after the position, or else nearest before it, or -1
        int next() const {
            int best = -1;
            qint64 bestKey = 0;
            for (int i = 0; i < steps.size(); ++i) {
                const auto fragment = fragmentOf[size_t(i)];
                if (!fragment || fragment->state != Waiting) {
                    continue;
                }
                const auto &segment = segments[i];
                const qint64 end = segment.start + segment.length;
                const qint64 key = end > position
                                       ? std::max<qint64>(0, segment.start - position)
                                       : std::numeric_limits<qint32>::max() + (position - end);
                if (best < 0 || key < bestKey) {
                    best = i;
                    bestKey = key;
                }
            }
            return best;
        }

        void runWorker() {
            const auto engine = engineFactory();
            std::unique_lock lock(mutex);
            while (!stopping) {
                const int index = next();
                if (index < 0) {
                    work.wait(lock);
                    continue;
                }
                const auto step = steps[index];
                const auto directory = cacheDirectory;
                fragmentOf[size_t(index)]->state = Running;
                lock.unlock();

                // The fragment of an earlier render is taken as it is, as the other runners do.
                DiagnosticList engineDiagnostics;
                std::error_code error;
                if (!fs::exists(step.cacheFile, error)) {
                    fs::create_directories(directory, error);
                    engine->run(engines.resampler, step.resamplerArguments, engineDiagnostics);
                }
                auto samples = readFragment(step.cacheFile);

                lock.lock();
                auto &fragment = fragments[step.cacheFile];
                if (samples) {
                    fragment.state = Ready;
                    fragment.samples = std::move(samples);
                } else {
                    fragment.state = Failed;
                    diagnostics.append(engineDiagnostics);
                    diagnostics.push_back({DiagnosticSeverity::Warning,
                                           tr("This note could not be rendered, and is silent."),
                                           step.noteIndex});
                }
                changed.notify_all();
            }
        }
    };

    RealtimeSynth::RealtimeSynth(SynthEngines engines, int threadCount, EngineFactory engineFactory)
        : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.engines = std::move(engines);
        impl.engineFactory = engineFactory ? std::move(engineFactory)
                                           : [] { return std::make_unique<EngineProcess>(); };
        const int count = threadCount > 0 ? threadCount : std::max(1, QThread::idealThreadCount());
        for (int i = 0; i < count; ++i) {
            impl.workers.emplace_back([this] {
                stdc_impl_t;
                impl.runWorker();
            });
        }
    }

    RealtimeSynth::~RealtimeSynth() {
        stdc_impl_t;
        {
            const std::lock_guard lock(impl.mutex);
            impl.stopping = true;
        }
        impl.work.notify_all();
        impl.changed.notify_all();
        for (auto &worker : impl.workers) {
            worker.join();
        }
    }

    void RealtimeSynth::setPlan(const SynthPlan &plan) {
        stdc_impl_t;
        QList<WavtoolCall> calls;
        DiagnosticList unreadable;
        for (const auto &step : plan.steps()) {
            auto call = WavtoolCall::parse(step.wavtoolArguments);
            if (!call) {
                // Silent rather than misplaced: a call without an envelope contributes nothing.
                unreadable.push_back({DiagnosticSeverity::Warning,
                                      tr("The wavtool arguments of this note cannot be read, so it "
                                         "is silent."),
                                      step.noteIndex});
                call = WavtoolCall();
            }
            calls.push_back(*call);
        }
        auto segments = WavtoolMixer::layOut(calls);

        {
            const std::lock_guard lock(impl.mutex);
            impl.steps = plan.steps();
            impl.segments = std::move(segments);
            impl.cacheDirectory = plan.cacheDirectory();
            impl.startTime = plan.startTime();
            impl.diagnostics.append(unreadable);

            // Fragments no longer in the plan are forgotten, unless a worker renders one now.
            std::map<fs::path, Impl::Fragment> kept;
            for (const auto &step : std::as_const(impl.steps)) {
                if (step.silent) {
                    continue;
                }
                const auto found = impl.fragments.find(step.cacheFile);
                kept[step.cacheFile] =
                    found != impl.fragments.end() ? found->second : Impl::Fragment();
            }
            for (const auto &[path, fragment] : impl.fragments) {
                if (fragment.state == Running) {
                    kept.emplace(path, fragment);
                }
            }
            impl.fragments = std::move(kept);
            impl.fragmentOf.assign(size_t(impl.steps.size()), nullptr);
            for (int i = 0; i < impl.steps.size(); ++i) {
                const auto &step = impl.steps[i];
                if (!step.silent && !impl.segments[i].silent) {
                    impl.fragmentOf[size_t(i)] = &impl.fragments.at(step.cacheFile);
                }
            }
        }
        impl.work.notify_all();
        impl.changed.notify_all();
    }

    void RealtimeSynth::setPosition(qint64 sample) {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        impl.position = sample;
    }

    qint64 RealtimeSynth::length() const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        return WavtoolMixer::lengthOf(impl.segments);
    }

    qint64 RealtimeSynth::startOf(int noteIndex) const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        for (int i = 0; i < impl.steps.size(); ++i) {
            if (impl.steps[i].noteIndex == noteIndex) {
                return impl.segments[i].start;
            }
        }
        return 0;
    }

    double RealtimeSynth::startTime() const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        return impl.startTime;
    }

    bool RealtimeSynth::isReady(qint64 first, qint64 count) const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        return impl.ready(first, count);
    }

    bool RealtimeSynth::waitReady(qint64 first, qint64 count,
                                  std::chrono::milliseconds timeout) const {
        stdc_impl_t;
        std::unique_lock lock(impl.mutex);
        return impl.changed.wait_for(lock, timeout, [&] {
            return impl.stopping || impl.ready(first, count);
        }) && !impl.stopping;
    }

    bool RealtimeSynth::mix(qint64 first, qint64 count, qint16 *out) const {
        stdc_impl_t;
        QList<WavtoolMixer::Segment> segments;
        std::vector<Samples> samples;
        {
            const std::lock_guard lock(impl.mutex);
            if (!impl.ready(first, count)) {
                return false;
            }
            segments = impl.segments;
            samples.resize(size_t(segments.size()));
            for (int i = 0; i < segments.size(); ++i) {
                if (const auto fragment = impl.fragmentOf[size_t(i)]) {
                    samples[size_t(i)] = fragment->samples;
                }
            }
        }
        // Mixed outside the lock, which the workers need meanwhile; the fragments are shared
        // and outlive a new plan.
        WavtoolMixer::mix(
            segments, [&samples](int index) { return samples[size_t(index)].get(); }, first, count,
            out);
        return true;
    }

    int RealtimeSynth::pendingCount() const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        int count = 0;
        for (int i = 0; i < impl.steps.size(); ++i) {
            if (impl.needs(i)) {
                ++count;
            }
        }
        return count;
    }

    QList<RealtimeSynth::NoteState> RealtimeSynth::noteStates() const {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        QList<NoteState> states;
        for (int i = 0; i < impl.steps.size(); ++i) {
            const auto &step = impl.steps[i];
            if (step.noteIndex >= states.size()) {
                states.resize(step.noteIndex + 1, Silent);
            }
            if (const auto fragment = impl.fragmentOf[size_t(i)]) {
                states[step.noteIndex] = fragment->state;
            }
        }
        return states;
    }

    DiagnosticList RealtimeSynth::takeDiagnostics() {
        stdc_impl_t;
        const std::lock_guard lock(impl.mutex);
        return std::exchange(impl.diagnostics, {});
    }

}
