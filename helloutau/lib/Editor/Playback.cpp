#include "Playback.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>

#include <stdcorelib/pimpl.h>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/RealtimeSynth.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/Synth/WavtoolMixer.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Audio/BufferSource.h>
#include <helloutau/Audio/StreamSource.h>

namespace hello::daw {

    namespace {

        // The track file of a render, in the cache directory. Its name does not begin with a
        // note number, so that no render takes it for a fragment of a note.
        constexpr char OutputFileName[] = "playback.wav";

        void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::DiagnosticSeverity::Error, message, std::nullopt});
        }

        // The inputs of a plan, taken on the main thread, where the document lives. The plan is
        // made on a worker thread, because it takes long enough for a track of many notes to
        // stall the window.
        struct PlanInput {
            kit::Project project;
            std::shared_ptr<const kit::VoiceBank> bank;
            kit::SynthPlan::Options options;
        };

        // One render: its inputs, taken on the main thread, and its results, filled on the
        // worker thread.
        struct Job {
            PlanInput input;
            // The key of the render kept when the job started
            QString keptKey;
            kit::SynthEngines engines;
            std::shared_ptr<const kit::SynthRunner> runner;
            int deviceRate = 0;
            // Whether the render only writes its track file, for renderTrack()
            bool fileOnly = false;
            std::atomic<bool> cancel = false;

            std::optional<kit::SynthPlan> plan;
            QString key;
            // Whether the plan has the key of the kept render, which then plays again
            bool sameAsKept = false;
            bool rendered = false;
            kit::DiagnosticList diagnostics;
            std::shared_ptr<const std::vector<float>> samples;
            int channels = 0;
            double startTime = 0;
        };

        // What a render depends on: every argument of every engine call, among them the names
        // of the fragments, which stand for the state of the samples, the engines, and the rate
        // it is played at. A render of the same key sounds the same.
        QString keyOf(const kit::SynthPlan &plan, const kit::SynthEngines &engines,
                      int deviceRate) {
            QStringList parts{QString::fromStdU16String(engines.resampler.u16string()),
                              QString::fromStdU16String(engines.wavtool.u16string()),
                              QString::fromStdU16String(plan.outputFile().u16string()),
                              QString::number(deviceRate)};
            for (const auto &step : plan.steps()) {
                parts.push_back(step.resamplerArguments.join(QChar(0x1f)));
                parts.push_back(step.wavtoolArguments.join(QChar(0x1f)));
            }
            return parts.join(QChar(0x1e));
        }

        // The playback that renders report to, read and written on the main thread only, and
        // null once the playback is gone. A cancelled render ends on its worker thread after
        // the playback, which does not wait for it when its window closes.
        struct Recipient {
            Playback *playback = nullptr;
        };

        // Calls \a function on the main thread with the playback, if it is still there.
        template <class Function>
        void deliver(const std::shared_ptr<Recipient> &recipient, Function function) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [recipient, function = std::move(function)] {
                    if (recipient->playback) {
                        function(*recipient->playback);
                    }
                },
                Qt::QueuedConnection);
        }

        // Relays progress to the main thread and cancellation to the runner.
        class Observer : public kit::SynthObserver {
        public:
            Observer(Job &job, std::shared_ptr<Recipient> recipient)
                : m_job(job), m_recipient(std::move(recipient)) {
            }

            void progressed(int done, int total) override {
                deliver(m_recipient, [done, total](Playback &playback) {
                    Q_EMIT playback.progressed(done, total);
                });
            }

            bool cancelled() override {
                return m_job.cancel.load();
            }

        private:
            Job &m_job;
            std::shared_ptr<Recipient> m_recipient;
        };

        // Relays the progress of a plan to the main thread, at its start and end and otherwise
        // at most every 50 milliseconds, and its cancellation to kit::SynthPlan::make().
        class PlanObserver : public kit::SynthObserver {
        public:
            // Called on the main thread with the playback, if any
            using Report = std::function<void(Playback &, int, int)>;

            PlanObserver(const std::atomic<bool> &cancel, std::shared_ptr<Recipient> recipient,
                         Report report)
                : m_cancel(cancel), m_recipient(std::move(recipient)), m_report(std::move(report)) {
            }

            void progressed(int done, int total) override {
                const auto now = std::chrono::steady_clock::now();
                if (!m_report ||
                    (done > 0 && done < total && now - m_last < std::chrono::milliseconds(50))) {
                    return;
                }
                m_last = now;
                deliver(m_recipient, [report = m_report, done, total](Playback &playback) {
                    report(playback, done, total);
                });
            }

            bool cancelled() override {
                return m_cancel.load();
            }

        private:
            const std::atomic<bool> &m_cancel;
            std::shared_ptr<Recipient> m_recipient;
            Report m_report;
            std::chrono::steady_clock::time_point m_last;
        };

        // Runs \a function on a thread of its own, which deletes itself once it ends.
        template <class Function>
        void detach(Function function) {
            const auto thread = QThread::create(std::move(function));
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            thread->start();
        }

    }

    class Playback::Impl {
    public:
        using Decl = Playback;

        explicit Impl(Decl *decl)
            : _decl(decl), recipient(std::make_shared<Recipient>(Recipient{decl})) {
        }

        Decl *_decl;
        std::shared_ptr<Recipient> recipient;
        State state = Stopped;
        std::shared_ptr<const kit::SynthRunner> runner =
            std::make_shared<kit::ClassicSynthRunner>();
        AudioOutput *output = nullptr;
        std::unique_ptr<QTemporaryDir> temporary;

        // The render in progress, and the workers that have not yet finished, including those
        // of cancelled renders
        std::shared_ptr<Job> job;
        QList<QPointer<QThread>> workers;
        // The last render, kept to be played again while it sounds the same, and paused in
        struct Rendered {
            QString key;
            std::shared_ptr<const std::vector<float>> samples;
            int channels = 0;
            // Where its track file starts, and the rate it plays at
            double startTime = 0;
            int deviceRate = 0;
            // The track file that the render wrote
            std::filesystem::path file;
        };
        std::optional<Rendered> kept;
        // Where playback was paused, and whether a preview was, while it is
        std::optional<double> pausedAt;
        bool pausedPreview = false;

        // The preview: the synthesis, kept between previews for the fragments it holds, the
        // stream it feeds, and the sample of the track file at which the stream began. The
        // stream refers to the synthesis and goes first.
        std::unique_ptr<kit::RealtimeSynth> synth;
        kit::SynthEngines synthEngines;
        // The thread count set, and that of the synthesis, zero for one per hardware thread
        int threadCount = 0;
        int synthThreads = 0;
        std::shared_ptr<StreamSource> stream;
        qint64 streamStart = 0;

        // The plan of the synth made on a worker thread: the cancellation of the plan under
        // way, the number of the latest request, whose result alone is taken, and its progress
        // in notes while it is made
        std::shared_ptr<std::atomic<bool>> planCancel;
        int planRequest = 0;
        std::optional<std::pair<int, int>> planProgress;
        // The time in milliseconds from which the synth renders once the plan arrives, if
        // prepare() or preview() requested one, and the device rate of the preview that then
        // starts
        std::optional<std::optional<double>> plannedFrom;
        std::optional<int> waitingPreview;

        // Without a synth, the states of the notes by the fragments in the render cache, and
        // the scan of the cache made on a worker thread
        QList<kit::RealtimeSynth::NoteState> cacheStates;
        std::shared_ptr<std::atomic<bool>> scanCancel;
        int scanRequest = 0;

        // The inputs of the plan of the whole track of \a document for the realtime mode,
        // whose track file is never written
        std::optional<PlanInput> previewInput(const kit::ProjectDocument &document,
                                              kit::DiagnosticList &diagnostics) {
            stdc_decl_t;
            const auto bank = document.voiceBank();
            if (!bank) {
                fail(diagnostics, Playback::tr("The project has no voice bank to sing with."));
                return std::nullopt;
            }
            PlanInput input{document.session()->snapshot(), bank, {}};
            input.options.cacheDirectory = decl.cacheDirectoryFor(document);
            input.options.outputFile = input.options.cacheDirectory / OutputFileName;
            return input;
        }

        // Makes the synth, or makes it anew for other engines or another thread count.
        void ensureSynth(const kit::SynthEngines &engines) {
            if (!synth || synthEngines.resampler != engines.resampler ||
                synthThreads != threadCount) {
                endPreview();
                synth = std::make_unique<kit::RealtimeSynth>(engines, threadCount);
                synthEngines = engines;
                synthThreads = threadCount;
            }
        }

        // Makes the plan of \a input for the synth on a worker thread, in place of the plan
        // under way. planned() receives it.
        void requestPlan(PlanInput input) {
            if (planCancel) {
                planCancel->store(true);
            }
            const auto cancel = std::make_shared<std::atomic<bool>>(false);
            planCancel = cancel;
            const int request = ++planRequest;
            planProgress = std::make_pair(0, 0);
            detach([recipient = recipient, cancel, request, input = std::move(input)] {
                PlanObserver observer(*cancel, recipient,
                                      [request](Playback &playback, int done, int total) {
                                          playback._impl->planProgressed(request, done, total);
                                      });
                auto diagnostics = std::make_shared<kit::DiagnosticList>();
                const auto plan =
                    std::make_shared<std::optional<kit::SynthPlan>>(kit::SynthPlan::make(
                        input.project, *input.bank, input.options, *diagnostics, &observer));
                if (cancel->load()) {
                    return;
                }
                deliver(recipient, [request, plan, diagnostics](Playback &playback) {
                    playback._impl->planned(request, *plan, *diagnostics);
                });
            });
        }

        void planProgressed(int request, int done, int total) {
            stdc_decl_t;
            if (request == planRequest && planProgress) {
                planProgress = std::make_pair(done, total);
                Q_EMIT decl.planProgressed(done, total);
            }
        }

        // Gives the synth the plan of request, unless a later request superseded it, and starts
        // the preview that waits for it. Without a plan the synth keeps the plan it has, and
        // the preview that waits fails.
        void planned(int request, const std::optional<kit::SynthPlan> &plan,
                     const kit::DiagnosticList &diagnostics) {
            stdc_decl_t;
            if (request != planRequest) {
                return;
            }
            planCancel.reset();
            planProgress.reset();
            if (!synth) {
                return;
            }
            if (!plan) {
                plannedFrom.reset();
                if (waitingPreview) {
                    waitingPreview.reset();
                    setState(Stopped);
                    Q_EMIT decl.failed(diagnostics);
                }
                return;
            }
            synth->setPlan(*plan);
            qint64 start = 0;
            if (plannedFrom) {
                // The sample of the track file at that time; the file starts at startTime().
                const auto fromTime = *plannedFrom;
                start = fromTime
                            ? std::clamp<qint64>(std::llround((*fromTime - synth->startTime()) *
                                                              kit::WavtoolMixer::sampleRate / 1000),
                                                 0, synth->length())
                            : 0;
                synth->setPosition(start);
                plannedFrom.reset();
            }
            if (waitingPreview) {
                const int deviceRate = *waitingPreview;
                waitingPreview.reset();
                startStream(start, deviceRate);
            }
            Q_EMIT decl.noteStatesChanged();
        }

        // Plays the synth from sample start of the track file on the output device.
        void startStream(qint64 start, int deviceRate) {
            stdc_decl_t;
            // On the thread of the stream: waits briefly for the notes of the next block, and
            // otherwise gives nothing, so that the device plays silence while they are rendered.
            const auto at = std::make_shared<std::atomic<qint64>>(start);
            auto generator = [synth = synth.get(), at](float *out, qsizetype frames) -> qsizetype {
                const qint64 from = at->load();
                const qint64 count = std::min<qint64>(frames, synth->length() - from);
                if (count <= 0) {
                    return -1;
                }
                std::vector<qint16> block(static_cast<size_t>(count));
                if (!synth->waitReady(from, count, std::chrono::milliseconds(50)) ||
                    !synth->mix(from, count, block.data())) {
                    return 0;
                }
                for (qint64 i = 0; i < count; ++i) {
                    out[i] = float(block[size_t(i)]) / 32768;
                }
                at->store(from + count);
                synth->setPosition(from + count);
                return count;
            };
            stream = std::make_shared<StreamSource>(std::move(generator),
                                                    kit::WavtoolMixer::sampleRate, deviceRate);
            streamStart = start;
            stream->start();

            QString error;
            if (!output->start(stream, &error)) {
                endPreview();
                setState(Stopped);
                kit::DiagnosticList diagnostics;
                fail(diagnostics, error);
                Q_EMIT decl.failed(diagnostics);
                return;
            }
            setState(Playing);
        }

        // Ends the plans and the scans under way, whose results are then ignored, and the
        // preview that waits for a plan.
        void stopPlanning() {
            for (const auto &cancel : {planCancel, scanCancel}) {
                if (cancel) {
                    cancel->store(true);
                }
            }
            planCancel.reset();
            scanCancel.reset();
            ++planRequest;
            ++scanRequest;
            planProgress.reset();
            plannedFrom.reset();
            waitingPreview.reset();
        }

        // Scans the render cache for the fragments of the plan of \a document on a worker
        // thread, for the states of the notes without a synth.
        void scanCache(const kit::ProjectDocument &document) {
            stdc_decl_t;
            kit::DiagnosticList ignored;
            auto input = previewInput(document, ignored);
            if (!input) {
                cacheStates.clear();
                Q_EMIT decl.noteStatesChanged();
                return;
            }
            if (scanCancel) {
                scanCancel->store(true);
            }
            const auto cancel = std::make_shared<std::atomic<bool>>(false);
            scanCancel = cancel;
            const int request = ++scanRequest;
            detach([recipient = recipient, cancel, request, input = std::move(*input)] {
                using State = kit::RealtimeSynth::NoteState;
                PlanObserver observer(*cancel, recipient, {});
                kit::DiagnosticList diagnostics;
                const auto plan = kit::SynthPlan::make(input.project, *input.bank, input.options,
                                                       diagnostics, &observer);
                QList<State> states;
                for (const auto &step : plan ? plan->steps() : QList<kit::SynthStep>()) {
                    if (cancel->load()) {
                        return;
                    }
                    if (step.noteIndex >= states.size()) {
                        states.resize(step.noteIndex + 1, State::Silent);
                    }
                    if (!step.silent) {
                        std::error_code error;
                        states[step.noteIndex] = std::filesystem::exists(step.cacheFile, error)
                                                     ? State::Ready
                                                     : State::Waiting;
                    }
                }
                if (cancel->load()) {
                    return;
                }
                deliver(recipient, [request, states](Playback &playback) {
                    playback._impl->scanned(request, states);
                });
            });
        }

        void scanned(int request, const QList<kit::RealtimeSynth::NoteState> &states) {
            stdc_decl_t;
            if (request != scanRequest || synth) {
                return;
            }
            scanCancel.reset();
            cacheStates = states;
            Q_EMIT decl.noteStatesChanged();
        }

        // Makes the plan, renders, reads and converts, on the worker thread. A plan with the
        // key of the kept render is not rendered, because the kept render plays again.
        static void run(const std::shared_ptr<Job> &job,
                        const std::shared_ptr<Recipient> &recipient) {
            PlanObserver planObserver(job->cancel, recipient,
                                      [job](Playback &playback, int done, int total) {
                                          if (playback._impl->job == job) {
                                              Q_EMIT playback.planProgressed(done, total);
                                          }
                                      });
            job->plan = kit::SynthPlan::make(job->input.project, *job->input.bank,
                                             job->input.options, job->diagnostics, &planObserver);
            if (!job->plan || job->cancel.load()) {
                return;
            }
            const auto &plan = *job->plan;
            if (!job->fileOnly) {
                job->key = keyOf(plan, job->engines, job->deviceRate);
                if (job->key == job->keptKey) {
                    job->sameAsKept = true;
                    return;
                }
            }
            std::error_code error;
            std::filesystem::create_directories(plan.cacheDirectory(), error);
            Observer observer(*job, recipient);
            // The render starts. A runner reports its steps from here on, if at all, as the
            // classic runner does only once its script has ended.
            observer.progressed(0, 0);
            const auto outcome =
                job->runner->render(plan, job->engines, &observer, job->diagnostics);
            if (!outcome.rendered || outcome.cancelled || job->cancel.load()) {
                return;
            }
            if (job->fileOnly) {
                job->rendered = true;
                return;
            }
            const auto audio = kit::WaveAudio::read(plan.outputFile(), job->diagnostics);
            if (!audio) {
                return;
            }
            job->samples = std::make_shared<const std::vector<float>>(
                resampled(audio->samples, audio->channels, audio->sampleRate, job->deviceRate));
            job->channels = audio->channels;
            job->startTime = plan.startTime();
            job->rendered = true;
        }

        // Starts job on a worker thread, which reports to rendered() once the render ends.
        void start(const std::shared_ptr<Job> &started) {
            job = started;
            const auto worker = QThread::create([recipient = recipient, started] {
                run(started, recipient);
                deliver(recipient,
                        [started](Playback &playback) { playback._impl->rendered(started); });
            });
            QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            workers.removeAll(nullptr);
            workers.push_back(worker);
            worker->start();
            setState(Rendering);
        }

        // Whether a render cancelled before has yet to end: the engine calls under way, or a
        // script, which ends within moments with the engines it started.
        bool rendersStill() {
            workers.removeAll(nullptr);
            return std::any_of(
                workers.cbegin(), workers.cend(),
                [](const QPointer<QThread> &worker) { return worker && worker->isRunning(); });
        }

        // The generator of the stream calls the synth, which may go before the stream does: the
        // device can release the stream later than it stops.
        void endPreview() {
            if (stream) {
                stream->stop();
            }
            stream.reset();
        }

        void setState(State value) {
            stdc_decl_t;
            if (state != value) {
                state = value;
                Q_EMIT decl.stateChanged(value);
            }
        }

        void cancelRender() {
            if (job) {
                job->cancel.store(true);
                job.reset();
            }
        }

        // Plays the result of \a finished, unless it was cancelled or superseded meanwhile.
        void rendered(const std::shared_ptr<Job> &finished) {
            stdc_decl_t;
            if (finished != job) {
                return;
            }
            job.reset();
            if (finished->sameAsKept) {
                if (kept && kept->key == finished->key) {
                    playRendered(0);
                } else {
                    setState(Stopped);
                }
                return;
            }
            if (!finished->rendered) {
                setState(Stopped);
                Q_EMIT decl.failed(finished->diagnostics);
                return;
            }
            if (finished->fileOnly) {
                setState(Stopped);
                Q_EMIT decl.trackRendered(finished->plan->outputFile());
                return;
            }
            kept =
                Rendered{finished->key,       finished->samples,    finished->channels,
                         finished->startTime, finished->deviceRate, finished->plan->outputFile()};
            playRendered(0);
        }

        // Plays the kept render from frame first on.
        void playRendered(qsizetype first) {
            stdc_decl_t;
            QString error;
            if (!output->start(std::make_shared<BufferSource>(kept->samples, kept->channels, first),
                               &error)) {
                setState(Stopped);
                kit::DiagnosticList diagnostics;
                fail(diagnostics, error);
                Q_EMIT decl.failed(diagnostics);
                return;
            }
            setState(Playing);
        }
    };

    Playback::Playback(QObject *parent) : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.output = new AudioOutput(this);
        connect(impl.output, &AudioOutput::finished, this, [this] {
            stdc_impl_t;
            impl.endPreview();
            if (impl.state == Playing) {
                impl.setState(Stopped);
            }
        });
    }

    Playback::~Playback() {
        stdc_impl_t;
        impl.output->stop();
        impl.endPreview();
        impl.cancelRender();
        impl.stopPlanning();
        // The workers are not waited for. A cancelled render ends on its own within moments.
        impl.recipient->playback = nullptr;
    }

    void Playback::setRunner(std::shared_ptr<const kit::SynthRunner> runner) {
        stdc_impl_t;
        impl.runner = std::move(runner);
        impl.kept.reset();
    }

    void Playback::setThreadCount(int count) {
        stdc_impl_t;
        impl.threadCount = std::max(0, count);
    }

    Playback::State Playback::state() const {
        stdc_impl_t;
        return impl.state;
    }

    bool Playback::play(const kit::ProjectDocument &document,
                        std::optional<std::pair<int, int>> range, const kit::SynthEngines &engines,
                        kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        stop();
        if (impl.rendersStill()) {
            fail(diagnostics, tr("The previous render has not ended yet. Closing its console "
                                 "window stops it."));
            return false;
        }
        if (engines.resampler.empty() || engines.wavtool.empty()) {
            fail(diagnostics, tr("Set the resampler and the wavtool in the settings first."));
            return false;
        }
        const auto bank = document.voiceBank();
        if (!bank) {
            fail(diagnostics, tr("The project has no voice bank to sing with."));
            return false;
        }
        const int deviceRate = AudioOutput::deviceSampleRate();
        if (deviceRate <= 0) {
            fail(diagnostics, tr("There is no audio output device."));
            return false;
        }
        auto job = std::make_shared<Job>();
        job->input = {document.session()->snapshot(), bank, {}};
        job->input.options.cacheDirectory = cacheDirectoryFor(document);
        job->input.options.outputFile = job->input.options.cacheDirectory / OutputFileName;
        job->input.options.range = range;
        // The same notes as the last render play again without the engines.
        job->keptKey = impl.kept ? impl.kept->key : QString();
        job->engines = engines;
        job->runner = impl.runner;
        job->deviceRate = deviceRate;
        impl.start(job);
        return true;
    }

    bool Playback::renderTrack(const kit::ProjectDocument &document,
                               const std::filesystem::path &file, const kit::SynthEngines &engines,
                               kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        stop();
        if (impl.rendersStill()) {
            fail(diagnostics, tr("The previous render has not ended yet. Closing its console "
                                 "window stops it."));
            return false;
        }
        if (engines.resampler.empty() || engines.wavtool.empty()) {
            fail(diagnostics, tr("Set the resampler and the wavtool in the settings first."));
            return false;
        }
        const auto bank = document.voiceBank();
        if (!bank) {
            fail(diagnostics, tr("The project has no voice bank to sing with."));
            return false;
        }
        auto job = std::make_shared<Job>();
        job->input = {document.session()->snapshot(), bank, {}};
        job->input.options.cacheDirectory = cacheDirectoryFor(document);
        job->input.options.outputFile = file;
        job->engines = engines;
        job->runner = impl.runner;
        job->fileOnly = true;
        impl.start(job);
        return true;
    }

    bool Playback::preview(const kit::ProjectDocument &document, std::optional<double> fromTime,
                           const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        stop();
        if (engines.resampler.empty()) {
            fail(diagnostics, tr("Set the resampler in the settings first."));
            return false;
        }
        const int deviceRate = AudioOutput::deviceSampleRate();
        if (deviceRate <= 0) {
            fail(diagnostics, tr("There is no audio output device."));
            return false;
        }
        auto input = impl.previewInput(document, diagnostics);
        if (!input) {
            return false;
        }
        impl.ensureSynth(engines);
        impl.plannedFrom = fromTime;
        impl.waitingPreview = deviceRate;
        impl.requestPlan(std::move(*input));
        impl.setState(Rendering);
        return true;
    }

    bool Playback::isBuffering() const {
        stdc_impl_t;
        return impl.stream && impl.stream->isStarved();
    }

    bool Playback::prepare(const kit::ProjectDocument &document, std::optional<double> fromTime,
                           const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (impl.stream) {
            updatePlan(document);
            return true;
        }
        if (engines.resampler.empty()) {
            fail(diagnostics, tr("Set the resampler in the settings first."));
            return false;
        }
        auto input = impl.previewInput(document, diagnostics);
        if (!input) {
            return false;
        }
        impl.ensureSynth(engines);
        impl.plannedFrom = fromTime;
        impl.requestPlan(std::move(*input));
        return true;
    }

    void Playback::release() {
        stdc_impl_t;
        impl.stopPlanning();
        if (impl.synth) {
            stop();
            impl.synth.reset();
        }
    }

    std::optional<std::pair<int, int>> Playback::planProgress() const {
        stdc_impl_t;
        return impl.planProgress;
    }

    int Playback::pendingNotes() const {
        stdc_impl_t;
        return impl.synth ? impl.synth->pendingCount() : 0;
    }

    QList<kit::RealtimeSynth::NoteState> Playback::noteStates() const {
        stdc_impl_t;
        return impl.synth ? impl.synth->noteStates() : impl.cacheStates;
    }

    void Playback::refreshNoteStates(const kit::ProjectDocument &document) {
        stdc_impl_t;
        if (impl.synth) {
            Q_EMIT noteStatesChanged();
            return;
        }
        impl.scanCache(document);
    }

    void Playback::updatePlan(const kit::ProjectDocument &document) {
        stdc_impl_t;
        if (!impl.synth) {
            return;
        }
        kit::DiagnosticList ignored;
        if (auto input = impl.previewInput(document, ignored)) {
            impl.requestPlan(std::move(*input));
        }
    }

    kit::DiagnosticList Playback::takePreviewDiagnostics() {
        stdc_impl_t;
        return impl.synth ? impl.synth->takeDiagnostics() : kit::DiagnosticList();
    }

    bool Playback::pause() {
        stdc_impl_t;
        if (impl.state != Playing) {
            return false;
        }
        impl.pausedAt = position();
        impl.pausedPreview = bool(impl.stream);
        // Paused before the output stops, so that its end does not stop playback
        impl.setState(Paused);
        impl.output->stop();
        impl.endPreview();
        return true;
    }

    bool Playback::isPreviewPaused() const {
        stdc_impl_t;
        return impl.state == Paused && impl.pausedPreview;
    }

    bool Playback::resume() {
        stdc_impl_t;
        if (impl.state != Paused || impl.pausedPreview || !impl.kept || !impl.pausedAt) {
            return false;
        }
        const auto &rendered = *impl.kept;
        const auto first =
            std::llround((*impl.pausedAt - rendered.startTime) * rendered.deviceRate / 1000);
        impl.playRendered(qsizetype(first));
        return impl.state == Playing;
    }

    void Playback::stop() {
        stdc_impl_t;
        impl.cancelRender();
        // The plan goes on for the rendering in the background.
        impl.waitingPreview.reset();
        impl.output->stop();
        impl.endPreview();
        impl.setState(Stopped);
    }

    std::filesystem::path Playback::lastRenderFile() const {
        stdc_impl_t;
        return impl.kept ? impl.kept->file : std::filesystem::path();
    }

    std::optional<double> Playback::position() const {
        stdc_impl_t;
        if (impl.state == Paused) {
            return impl.pausedAt;
        }
        if (impl.state != Playing) {
            return std::nullopt;
        }
        // What the device plays, which it pulled some time before, from the start until it
        // has pulled
        const double heard = impl.output->heardPosition().value_or(0);
        if (impl.stream) {
            const auto sample = double(impl.streamStart) + heard;
            return impl.synth->startTime() + sample * 1000 / kit::WavtoolMixer::sampleRate;
        }
        return impl.kept ? impl.kept->startTime + heard * 1000 / std::max(1, impl.kept->deviceRate)
                         : std::optional<double>();
    }

    std::optional<int> Playback::clearCache(const kit::ProjectDocument &document,
                                            kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (impl.state == Rendering || impl.rendersStill()) {
            fail(diagnostics, tr("A render has not ended yet. Closing its console window stops "
                                 "it."));
            return std::nullopt;
        }
        // The synth waits for the engine calls under way, which would write into the cache.
        stop();
        impl.stopPlanning();
        impl.synth.reset();
        impl.kept.reset();
        impl.cacheStates.clear();

        namespace fs = std::filesystem;
        const auto directory = cacheDirectoryFor(document);
        int deleted = 0;
        std::error_code error;
        for (fs::directory_iterator it(directory, error), end; !error && it != end;
             it.increment(error)) {
            std::error_code status;
            if (!it->is_regular_file(status)) {
                continue;
            }
            std::error_code removal;
            if (fs::remove(it->path(), removal)) {
                ++deleted;
            } else {
                diagnostics.push_back({kit::DiagnosticSeverity::Warning,
                                       tr("\"%1\" could not be deleted.")
                                           .arg(QDir::toNativeSeparators(
                                               QString::fromStdU16String(it->path().u16string()))),
                                       std::nullopt});
            }
        }
        return deleted;
    }

    std::filesystem::path Playback::cacheDirectoryFor(const kit::ProjectDocument &document) {
        stdc_impl_t;
        // The .usth, or else the UST imported, whose cache UTAU uses as well
        const auto file = document.filePath().empty() ? document.sourcePath() : document.filePath();
        if (!file.empty()) {
            return kit::Project::cacheDirectoryOf(file);
        }
        if (!impl.temporary) {
            impl.temporary = std::make_unique<QTemporaryDir>();
        }
        return std::filesystem::path(impl.temporary->path().toStdU16String());
    }

}
