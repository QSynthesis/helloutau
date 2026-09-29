#include "Playback.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QtCore/QCoreApplication>
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

namespace hello::daw {

    namespace {

        // The track file of a render, in the cache directory. Its name does not begin with a
        // note number, so that no render takes it for a fragment of a note.
        constexpr char OutputFileName[] = "playback.wav";

        void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::DiagnosticSeverity::Error, message, std::nullopt});
        }

        // One render: its inputs, taken on the main thread, and its results, filled on the
        // worker thread.
        struct Job {
            std::optional<kit::SynthPlan> plan;
            QString key;
            kit::SynthEngines engines;
            std::shared_ptr<const kit::SynthRunner> runner;
            int deviceRate = 0;
            std::atomic<bool> cancel = false;

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
        // null once the playback is gone. A render by script runs on until the script ends,
        // which the playback does not wait for when its window closes.
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

        // Renders, reads and converts, on the worker thread.
        void run(Job &job, const std::shared_ptr<Recipient> &recipient) {
            std::error_code error;
            std::filesystem::create_directories(job.plan->cacheDirectory(), error);
            const auto &plan = job.plan;
            Observer observer(job, recipient);
            const auto outcome = job.runner->render(*plan, job.engines, &observer, job.diagnostics);
            if (!outcome.rendered || outcome.cancelled || job.cancel.load()) {
                return;
            }
            const auto audio = kit::WaveAudio::read(plan->outputFile(), job.diagnostics);
            if (!audio) {
                return;
            }
            job.samples = std::make_shared<const std::vector<float>>(
                resampled(audio->samples, audio->channels, audio->sampleRate, job.deviceRate));
            job.channels = audio->channels;
            job.startTime = plan->startTime();
            job.rendered = true;
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
        std::shared_ptr<StreamSource> stream;
        qint64 streamStart = 0;

        // The plan of the whole track of \a document for the preview, whose track file is
        // never written
        std::optional<kit::SynthPlan> previewPlan(const kit::ProjectDocument &document,
                                                  kit::DiagnosticList &diagnostics) {
            stdc_decl_t;
            const auto bank = document.voiceBank();
            if (!bank) {
                fail(diagnostics, Playback::tr("The project has no voice bank to sing with."));
                return std::nullopt;
            }
            kit::SynthPlan::Options options;
            options.cacheDirectory = decl.cacheDirectoryFor(document);
            options.outputFile = options.cacheDirectory / OutputFileName;
            return kit::SynthPlan::make(document.session()->snapshot(), *bank, options,
                                        diagnostics);
        }

        // Gives the synth the plan of \a document and the position of \a fromTime, making the
        // synth first or anew for other engines, and returns that position in samples of the
        // track file.
        std::optional<qint64> prepare(const kit::ProjectDocument &document,
                                      std::optional<double> fromTime,
                                      const kit::SynthEngines &engines,
                                      kit::DiagnosticList &diagnostics) {
            if (engines.resampler.empty()) {
                fail(diagnostics, Playback::tr("Set the resampler in the settings first."));
                return std::nullopt;
            }
            const auto plan = previewPlan(document, diagnostics);
            if (!plan) {
                return std::nullopt;
            }
            if (!synth || synthEngines.resampler != engines.resampler) {
                endPreview();
                synth = std::make_unique<kit::RealtimeSynth>(engines);
                synthEngines = engines;
            }
            synth->setPlan(*plan);
            // The sample of the track file at that time; the file starts at startTime().
            const qint64 start =
                fromTime ? std::clamp<qint64>(std::llround((*fromTime - synth->startTime()) *
                                                           kit::WavtoolMixer::sampleRate / 1000),
                                              0, synth->length())
                         : 0;
            synth->setPosition(start);
            return start;
        }

        // Whether a render cancelled before has yet to end, as a script does, which runs in its
        // console until it ends or its window is closed.
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
            if (!finished->rendered) {
                setState(Stopped);
                Q_EMIT decl.failed(finished->diagnostics);
                return;
            }
            kept = Rendered{finished->key, finished->samples, finished->channels,
                            finished->startTime, finished->deviceRate};
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
        // The workers are not waited for: a script runs until it ends or its window is closed.
        impl.recipient->playback = nullptr;
    }

    void Playback::setRunner(std::shared_ptr<const kit::SynthRunner> runner) {
        stdc_impl_t;
        impl.runner = std::move(runner);
        impl.kept.reset();
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
        kit::SynthPlan::Options options;
        options.cacheDirectory = cacheDirectoryFor(document);
        options.outputFile = options.cacheDirectory / OutputFileName;
        options.range = range;
        auto plan =
            kit::SynthPlan::make(document.session()->snapshot(), *bank, options, diagnostics);
        if (!plan) {
            return false;
        }

        // The same notes as the last render, which plays again without the engines
        auto key = keyOf(*plan, engines, deviceRate);
        if (impl.kept && impl.kept->key == key) {
            impl.playRendered(0);
            return impl.state == Playing;
        }

        auto job = std::make_shared<Job>();
        job->plan = std::move(plan);
        job->key = std::move(key);
        job->engines = engines;
        job->runner = impl.runner;
        job->deviceRate = deviceRate;
        impl.job = job;

        const auto worker = QThread::create([recipient = impl.recipient, job] {
            run(*job, recipient);
            deliver(recipient, [job](Playback &playback) { playback._impl->rendered(job); });
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        impl.workers.removeAll(nullptr);
        impl.workers.push_back(worker);
        worker->start();
        impl.setState(Rendering);
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
        const auto prepared = impl.prepare(document, fromTime, engines, diagnostics);
        if (!prepared) {
            return false;
        }
        const qint64 start = *prepared;
        const auto &synth = impl.synth;

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
        impl.stream = std::make_shared<StreamSource>(std::move(generator),
                                                     kit::WavtoolMixer::sampleRate, deviceRate);
        impl.streamStart = start;
        impl.stream->start();

        QString error;
        if (!impl.output->start(impl.stream, &error)) {
            impl.endPreview();
            fail(diagnostics, error);
            return false;
        }
        impl.setState(Playing);
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
        return impl.prepare(document, fromTime, engines, diagnostics).has_value();
    }

    void Playback::release() {
        stdc_impl_t;
        if (impl.synth) {
            stop();
            impl.synth.reset();
        }
    }

    int Playback::pendingNotes() const {
        stdc_impl_t;
        return impl.synth ? impl.synth->pendingCount() : 0;
    }

    QList<kit::RealtimeSynth::NoteState>
        Playback::noteStates(const kit::ProjectDocument &document) {
        stdc_impl_t;
        using State = kit::RealtimeSynth::NoteState;
        if (impl.synth) {
            return impl.synth->noteStates();
        }
        kit::DiagnosticList ignored;
        const auto plan = impl.previewPlan(document, ignored);
        if (!plan) {
            return {};
        }
        QList<State> states;
        for (const auto &step : plan->steps()) {
            if (step.noteIndex >= states.size()) {
                states.resize(step.noteIndex + 1, State::Silent);
            }
            if (!step.silent) {
                std::error_code error;
                states[step.noteIndex] =
                    std::filesystem::exists(step.cacheFile, error) ? State::Ready : State::Waiting;
            }
        }
        return states;
    }

    void Playback::updatePlan(const kit::ProjectDocument &document) {
        stdc_impl_t;
        if (!impl.synth) {
            return;
        }
        kit::DiagnosticList diagnostics;
        if (const auto plan = impl.previewPlan(document, diagnostics)) {
            impl.synth->setPlan(*plan);
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
        impl.output->stop();
        impl.endPreview();
        impl.setState(Stopped);
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
        impl.synth.reset();
        impl.kept.reset();

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
                                           .arg(QString::fromStdU16String(it->path().u16string())),
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
