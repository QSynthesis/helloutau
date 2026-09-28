#include "RealtimeSynth.h"

#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <thread>

#include <QtCore/QThread>

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
        enum State {
            Waiting,
            Running,
            Ready,
            Failed,
        };

        struct Fragment {
            State state = Waiting;
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
        DiagnosticList diagnostics;
        std::vector<std::thread> workers;

        // Whether step \a index sounds and still needs its fragment
        bool needs(int index) const {
            const auto &step = steps[index];
            if (step.silent || segments[index].silent) {
                return false;
            }
            const auto found = fragments.find(step.cacheFile);
            return found == fragments.end() || found->second.state == Waiting ||
                   found->second.state == Running;
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
                const auto &step = steps[i];
                if (step.silent || segments[i].silent) {
                    continue;
                }
                const auto found = fragments.find(step.cacheFile);
                if (found == fragments.end() || found->second.state != Waiting) {
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
                fragments[step.cacheFile].state = Running;
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
        _impl->engines = std::move(engines);
        _impl->engineFactory = engineFactory ? std::move(engineFactory)
                                             : [] { return std::make_unique<EngineProcess>(); };
        const int count = threadCount > 0 ? threadCount : std::max(1, QThread::idealThreadCount());
        for (int i = 0; i < count; ++i) {
            _impl->workers.emplace_back([this] { _impl->runWorker(); });
        }
    }

    RealtimeSynth::~RealtimeSynth() {
        {
            const std::lock_guard lock(_impl->mutex);
            _impl->stopping = true;
        }
        _impl->work.notify_all();
        _impl->changed.notify_all();
        for (auto &worker : _impl->workers) {
            worker.join();
        }
    }

    void RealtimeSynth::setPlan(const SynthPlan &plan) {
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
            const std::lock_guard lock(_impl->mutex);
            _impl->steps = plan.steps();
            _impl->segments = std::move(segments);
            _impl->cacheDirectory = plan.cacheDirectory();
            _impl->startTime = plan.startTime();
            _impl->diagnostics.append(unreadable);

            // Fragments no longer in the plan are forgotten, unless a worker renders one now.
            std::map<fs::path, Impl::Fragment> kept;
            for (const auto &step : std::as_const(_impl->steps)) {
                if (step.silent) {
                    continue;
                }
                const auto found = _impl->fragments.find(step.cacheFile);
                kept[step.cacheFile] =
                    found != _impl->fragments.end() ? found->second : Impl::Fragment();
            }
            for (const auto &[path, fragment] : _impl->fragments) {
                if (fragment.state == Impl::Running) {
                    kept.emplace(path, fragment);
                }
            }
            _impl->fragments = std::move(kept);
        }
        _impl->work.notify_all();
        _impl->changed.notify_all();
    }

    void RealtimeSynth::setPosition(qint64 sample) {
        const std::lock_guard lock(_impl->mutex);
        _impl->position = sample;
    }

    qint64 RealtimeSynth::length() const {
        const std::lock_guard lock(_impl->mutex);
        return WavtoolMixer::lengthOf(_impl->segments);
    }

    qint64 RealtimeSynth::startOf(int noteIndex) const {
        const std::lock_guard lock(_impl->mutex);
        for (int i = 0; i < _impl->steps.size(); ++i) {
            if (_impl->steps[i].noteIndex == noteIndex) {
                return _impl->segments[i].start;
            }
        }
        return 0;
    }

    double RealtimeSynth::startTime() const {
        const std::lock_guard lock(_impl->mutex);
        return _impl->startTime;
    }

    bool RealtimeSynth::isReady(qint64 first, qint64 count) const {
        const std::lock_guard lock(_impl->mutex);
        return _impl->ready(first, count);
    }

    bool RealtimeSynth::waitReady(qint64 first, qint64 count,
                                  std::chrono::milliseconds timeout) const {
        std::unique_lock lock(_impl->mutex);
        return _impl->changed.wait_for(lock, timeout, [&] {
            return _impl->stopping || _impl->ready(first, count);
        }) && !_impl->stopping;
    }

    bool RealtimeSynth::mix(qint64 first, qint64 count, qint16 *out) const {
        QList<WavtoolMixer::Segment> segments;
        std::vector<Samples> samples;
        {
            const std::lock_guard lock(_impl->mutex);
            if (!_impl->ready(first, count)) {
                return false;
            }
            segments = _impl->segments;
            samples.resize(size_t(segments.size()));
            for (int i = 0; i < segments.size(); ++i) {
                const auto found = _impl->fragments.find(_impl->steps[i].cacheFile);
                if (found != _impl->fragments.end()) {
                    samples[size_t(i)] = found->second.samples;
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
        const std::lock_guard lock(_impl->mutex);
        int count = 0;
        for (int i = 0; i < _impl->steps.size(); ++i) {
            if (_impl->needs(i)) {
                ++count;
            }
        }
        return count;
    }

    DiagnosticList RealtimeSynth::takeDiagnostics() {
        const std::lock_guard lock(_impl->mutex);
        return std::exchange(_impl->diagnostics, {});
    }

}
