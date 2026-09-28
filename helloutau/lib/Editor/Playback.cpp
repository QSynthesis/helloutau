#include "Playback.h"

#include <atomic>

#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Synth/RealtimeSynth.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>
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
            kit::Project project;
            std::shared_ptr<const kit::VoiceBank> bank;
            kit::SynthPlan::Options options;
            kit::SynthEngines engines;
            std::shared_ptr<const kit::SynthRunner> runner;
            int deviceRate = 0;
            std::atomic<bool> cancel = false;

            bool rendered = false;
            kit::DiagnosticList diagnostics;
            std::vector<float> samples;
            int channels = 0;
            double startTime = 0;
        };

        // Relays progress to the main thread and cancellation to the runner.
        class Observer : public kit::SynthObserver {
        public:
            Observer(Job &job, Playback *playback) : m_job(job), m_playback(playback) {
            }

            void progressed(int done, int total) override {
                QMetaObject::invokeMethod(
                    m_playback,
                    [playback = m_playback, done, total] {
                        Q_EMIT playback->progressed(done, total);
                    },
                    Qt::QueuedConnection);
            }

            bool cancelled() override {
                return m_job.cancel.load();
            }

        private:
            Job &m_job;
            Playback *m_playback;
        };

        // Renders, reads and converts, on the worker thread.
        void run(Job &job, Playback *playback) {
            std::error_code error;
            std::filesystem::create_directories(job.options.cacheDirectory, error);
            const auto plan =
                kit::SynthPlan::make(job.project, *job.bank, job.options, job.diagnostics);
            if (!plan) {
                return;
            }
            Observer observer(job, playback);
            const auto outcome = job.runner->render(*plan, job.engines, &observer, job.diagnostics);
            if (!outcome.rendered || outcome.cancelled || job.cancel.load()) {
                return;
            }
            const auto audio = kit::WaveAudio::read(plan->outputFile(), job.diagnostics);
            if (!audio) {
                return;
            }
            job.samples =
                resampled(audio->samples, audio->channels, audio->sampleRate, job.deviceRate);
            job.channels = audio->channels;
            job.startTime = plan->startTime();
            job.rendered = true;
        }

    }

    class Playback::Impl {
    public:
        explicit Impl(Playback *decl) : _decl(decl) {
        }

        Playback *_decl;
        State state = Stopped;
        std::shared_ptr<const kit::SynthRunner> runner =
            std::make_shared<kit::ThreadedSynthRunner>();
        AudioOutput *output = nullptr;
        std::unique_ptr<QTemporaryDir> temporary;

        // The render in progress, and the workers that have not yet finished, including those
        // of cancelled renders
        std::shared_ptr<Job> job;
        QList<QPointer<QThread>> workers;
        double startTime = 0;

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
            const auto bank = document.voiceBank();
            if (!bank) {
                fail(diagnostics, Playback::tr("The project has no voice bank to sing with."));
                return std::nullopt;
            }
            kit::SynthPlan::Options options;
            options.cacheDirectory = _decl->cacheDirectoryFor(document);
            options.outputFile = options.cacheDirectory / OutputFileName;
            return kit::SynthPlan::make(document.session()->snapshot(), *bank, options,
                                        diagnostics);
        }

        void endPreview() {
            stream.reset();
        }

        void setState(State value) {
            if (state != value) {
                state = value;
                Q_EMIT _decl->stateChanged(value);
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
            if (finished != job) {
                return;
            }
            job.reset();
            if (!finished->rendered) {
                setState(Stopped);
                Q_EMIT _decl->failed(finished->diagnostics);
                return;
            }
            QString error;
            startTime = finished->startTime;
            if (!output->start(std::make_shared<BufferSource>(std::move(finished->samples),
                                                              finished->channels),
                               &error)) {
                setState(Stopped);
                kit::DiagnosticList diagnostics;
                fail(diagnostics, error);
                Q_EMIT _decl->failed(diagnostics);
                return;
            }
            setState(Playing);
        }
    };

    Playback::Playback(QObject *parent) : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        _impl->output = new AudioOutput(this);
        connect(_impl->output, &AudioOutput::finished, this, [this] {
            _impl->endPreview();
            if (_impl->state == Playing) {
                _impl->setState(Stopped);
            }
        });
    }

    Playback::~Playback() {
        _impl->output->stop();
        _impl->endPreview();
        _impl->cancelRender();
        // A worker refers to this object to report progress, so it must end first. A cancelled
        // render ends once the engine calls under way return.
        for (const auto &worker : std::as_const(_impl->workers)) {
            if (worker) {
                worker->wait();
            }
        }
    }

    void Playback::setRunner(std::shared_ptr<const kit::SynthRunner> runner) {
        _impl->runner = std::move(runner);
    }

    Playback::State Playback::state() const {
        return _impl->state;
    }

    bool Playback::play(const kit::ProjectDocument &document,
                        std::optional<std::pair<int, int>> range, const kit::SynthEngines &engines,
                        kit::DiagnosticList &diagnostics) {
        stop();
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
        job->project = document.session()->snapshot();
        job->bank = bank;
        job->options.cacheDirectory = cacheDirectoryFor(document);
        job->options.outputFile = job->options.cacheDirectory / OutputFileName;
        job->options.range = range;
        job->engines = engines;
        job->runner = _impl->runner;
        job->deviceRate = deviceRate;
        _impl->job = job;

        const auto worker = QThread::create([this, job] {
            run(*job, this);
            QMetaObject::invokeMethod(
                this, [this, job] { _impl->rendered(job); }, Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        _impl->workers.removeAll(nullptr);
        _impl->workers.push_back(worker);
        worker->start();
        _impl->setState(Rendering);
        return true;
    }

    bool Playback::preview(const kit::ProjectDocument &document, std::optional<int> fromNote,
                           const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics) {
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
        const auto plan = _impl->previewPlan(document, diagnostics);
        if (!plan) {
            return false;
        }

        auto &synth = _impl->synth;
        if (!synth || _impl->synthEngines.resampler != engines.resampler) {
            synth = std::make_unique<kit::RealtimeSynth>(engines);
            _impl->synthEngines = engines;
        }
        synth->setPlan(*plan);
        const qint64 start = fromNote ? synth->startOf(*fromNote) : 0;
        synth->setPosition(start);

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
        _impl->stream = std::make_shared<StreamSource>(std::move(generator),
                                                       kit::WavtoolMixer::sampleRate, deviceRate);
        _impl->streamStart = start;
        _impl->stream->start();

        QString error;
        if (!_impl->output->start(_impl->stream, &error)) {
            _impl->endPreview();
            fail(diagnostics, error);
            return false;
        }
        _impl->setState(Playing);
        return true;
    }

    bool Playback::isBuffering() const {
        return _impl->stream && _impl->stream->isStarved();
    }

    int Playback::pendingNotes() const {
        return _impl->stream ? _impl->synth->pendingCount() : 0;
    }

    void Playback::updatePlan(const kit::ProjectDocument &document) {
        if (!_impl->stream) {
            return;
        }
        kit::DiagnosticList diagnostics;
        if (const auto plan = _impl->previewPlan(document, diagnostics)) {
            _impl->synth->setPlan(*plan);
        }
    }

    kit::DiagnosticList Playback::takePreviewDiagnostics() {
        return _impl->synth ? _impl->synth->takeDiagnostics() : kit::DiagnosticList();
    }

    void Playback::stop() {
        _impl->cancelRender();
        _impl->output->stop();
        _impl->endPreview();
        _impl->setState(Stopped);
    }

    std::optional<double> Playback::position() const {
        if (_impl->state != Playing) {
            return std::nullopt;
        }
        if (_impl->stream) {
            const auto sample = double(_impl->streamStart + _impl->stream->position());
            return _impl->synth->startTime() + sample * 1000 / kit::WavtoolMixer::sampleRate;
        }
        return _impl->startTime + _impl->output->elapsed();
    }

    std::filesystem::path Playback::cacheDirectoryFor(const kit::ProjectDocument &document) {
        // The .usth, or else the UST imported, whose cache UTAU uses as well
        const auto file = document.filePath().empty() ? document.sourcePath() : document.filePath();
        if (!file.empty()) {
            return kit::Project::cacheDirectoryOf(file);
        }
        if (!_impl->temporary) {
            _impl->temporary = std::make_unique<QTemporaryDir>();
        }
        return std::filesystem::path(_impl->temporary->path().toStdU16String());
    }

}
