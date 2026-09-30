#include "SamplePreview.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>

#include <stdcorelib/pimpl.h>

#include <hellokit/Document/Project.h>
#include <hellokit/Synth/SynthPlan.h>

#include <helloutau/Audio/AudioOutput.h>

namespace hello::daw {

    namespace {

        // The lyric under which the note finds the sample, which is the only one of its voice
        // bank and is given this alias
        constexpr char lyric[] = "la";

        void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::DiagnosticSeverity::Error, message, std::nullopt});
        }

        // One note to synthesize: its inputs, taken on the main thread, and its results, filled
        // on the worker thread
        struct Job {
            kit::VoiceSample sample;
            int noteNum = 60;
            int length = 480;
            std::filesystem::path resampler;
            std::shared_ptr<QTemporaryDir> directory;
            std::shared_ptr<kit::EngineProcess> process;
            int deviceRate = 0;

            bool rendered = false;
            kit::DiagnosticList diagnostics;
            std::shared_ptr<const kit::WaveAudio> audio;
            std::vector<float> samples;
        };

        // The preview that a job reports to, null once the preview is gone; read and written
        // on the main thread only
        struct Recipient {
            SamplePreview *preview = nullptr;
        };

        // Synthesizes on the worker thread.
        void run(Job &job) {
            const auto folder = std::filesystem::path(job.directory->path().toStdU16String());

            // A voice bank of the sample alone, without a prefix map, so that the lyric finds
            // it whatever its alias and folder
            auto sample = job.sample;
            sample.directory = 0;
            sample.alias = QString::fromLatin1(lyric);
            sample.hasEntry = true;
            const kit::VoiceBank bank(sample.path.parent_path(), {kit::VoiceBankDirectory()},
                                      {sample});

            kit::Note note;
            note.lyric = QString::fromLatin1(lyric);
            note.noteNum = job.noteNum;
            note.length = job.length;
            kit::Track track;
            track.notes.push_back(note);
            kit::Project project;
            project.tracks.push_back(track);

            kit::SynthPlan::Options options;
            options.cacheDirectory = folder;
            options.outputFile = folder / "preview.wav";
            const auto plan = kit::SynthPlan::make(project, bank, options, job.diagnostics);
            if (!plan || plan->steps().isEmpty() || plan->steps().front().silent) {
                fail(job.diagnostics, SamplePreview::tr("The entry could not be made a note."));
                return;
            }
            const auto &step = plan->steps().front();
            std::error_code error;
            std::filesystem::remove(step.cacheFile, error);
            const auto result =
                job.process->run(job.resampler, step.resamplerArguments, job.diagnostics);
            if (!result.started) {
                fail(job.diagnostics,
                     SamplePreview::tr("The resampler \"%1\" could not be started.")
                         .arg(QString::fromStdU16String(job.resampler.u16string())));
                return;
            }
            kit::DiagnosticList reading;
            auto audio = kit::WaveAudio::read(step.cacheFile, reading);
            if (!audio) {
                fail(job.diagnostics,
                     SamplePreview::tr("The resampler wrote no audio (exit code %1). %2")
                         .arg(result.exitCode)
                         .arg(result.output.trimmed()));
                return;
            }
            if (job.deviceRate > 0) {
                job.samples =
                    resampled(audio->samples, audio->channels, audio->sampleRate, job.deviceRate);
            }
            job.audio = std::make_shared<const kit::WaveAudio>(std::move(*audio));
            job.rendered = true;
        }

    }

    class SamplePreview::Impl {
    public:
        using Decl = SamplePreview;

        explicit Impl(Decl *decl)
            : _decl(decl), recipient(std::make_shared<Recipient>(Recipient{decl})) {
        }

        Decl *_decl;
        std::shared_ptr<Recipient> recipient;
        State state = Stopped;
        AudioOutput *output = nullptr;
        std::shared_ptr<kit::EngineProcess> process;
        std::shared_ptr<QTemporaryDir> directory;
        std::shared_ptr<Job> job;
        std::shared_ptr<const kit::WaveAudio> synthesized;

        // Where the audio file plays from, while it does, and the rate it plays at
        std::optional<double> playedFrom;
        int deviceRate = 0;

        void setState(State value) {
            stdc_decl_t;
            if (state != value) {
                state = value;
                Q_EMIT decl.stateChanged(value);
            }
        }

        bool start(std::vector<float> samples, int channels, kit::DiagnosticList &diagnostics) {
            QString error;
            if (!output->start(std::make_shared<BufferSource>(std::move(samples), channels),
                               &error)) {
                fail(diagnostics, error);
                setState(Stopped);
                return false;
            }
            setState(Playing);
            return true;
        }

        void finished(const std::shared_ptr<Job> &done) {
            stdc_decl_t;
            if (done != job) {
                return;
            }
            job.reset();
            if (!done->rendered) {
                setState(Stopped);
                Q_EMIT decl.failed(done->diagnostics);
                return;
            }
            synthesized = done->audio;
            kit::DiagnosticList diagnostics;
            if (!start(std::move(done->samples), done->audio->channels, diagnostics)) {
                Q_EMIT decl.failed(diagnostics);
            }
        }
    };

    SamplePreview::SamplePreview(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.output = new AudioOutput(this);
        impl.process = std::make_shared<kit::EngineProcess>();
        connect(impl.output, &AudioOutput::finished, this, [this] {
            stdc_impl_t;
            if (impl.state == Playing) {
                impl.playedFrom.reset();
                impl.setState(Stopped);
            }
        });
    }

    SamplePreview::~SamplePreview() {
        stdc_impl_t;
        impl.output->stop();
        // The worker keeps what it uses, the directory among it, and reports to no one.
        impl.recipient->preview = nullptr;
    }

    void SamplePreview::setEngineProcess(std::shared_ptr<kit::EngineProcess> process) {
        stdc_impl_t;
        impl.process = std::move(process);
    }

    SamplePreview::State SamplePreview::state() const {
        stdc_impl_t;
        return impl.state;
    }

    bool SamplePreview::play(std::shared_ptr<const kit::WaveAudio> audio, double from,
                             std::optional<double> to, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        stop();
        if (!audio || audio->channels <= 0 || audio->sampleRate <= 0) {
            fail(diagnostics, tr("The audio file does not read."));
            return false;
        }
        const int rate = AudioOutput::deviceSampleRate();
        if (rate <= 0) {
            fail(diagnostics, tr("There is no audio output device."));
            return false;
        }
        const auto frames = audio->frameCount();
        const auto frameAt = [&](double time) {
            return std::clamp<qsizetype>(qsizetype(std::llround(time * audio->sampleRate / 1000)),
                                         0, frames);
        };
        const auto first = frameAt(from);
        const auto last = to ? frameAt(*to) : frames;
        if (last <= first) {
            fail(diagnostics, tr("There is nothing to play there."));
            return false;
        }
        const int channels = audio->channels;
        const std::vector<float> span(audio->samples.begin() + first * channels,
                                      audio->samples.begin() + last * channels);
        if (!impl.start(resampled(span, channels, audio->sampleRate, rate), channels,
                        diagnostics)) {
            return false;
        }
        impl.playedFrom = double(first) * 1000 / audio->sampleRate;
        impl.deviceRate = rate;
        return true;
    }

    bool SamplePreview::synthesize(const kit::VoiceSample &sample, int noteNum, int length,
                                   const std::filesystem::path &resampler,
                                   kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        stop();
        if (resampler.empty()) {
            fail(diagnostics, tr("Set the resampler in the settings first."));
            return false;
        }
        if (!impl.directory) {
            impl.directory = std::make_shared<QTemporaryDir>();
        }
        auto job = std::make_shared<Job>();
        job->sample = sample;
        job->noteNum = noteNum;
        job->length = length;
        job->resampler = resampler;
        job->directory = impl.directory;
        job->process = impl.process;
        job->process->workingDirectory =
            std::filesystem::path(impl.directory->path().toStdU16String());
        job->deviceRate = AudioOutput::deviceSampleRate();
        impl.job = job;

        const auto worker = QThread::create([recipient = impl.recipient, job] {
            run(*job);
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [recipient, job] {
                    if (recipient->preview) {
                        recipient->preview->_impl->finished(job);
                    }
                },
                Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
        impl.setState(Synthesizing);
        return true;
    }

    void SamplePreview::stop() {
        stdc_impl_t;
        impl.job.reset();
        impl.playedFrom.reset();
        impl.output->stop();
        impl.setState(Stopped);
    }

    std::optional<double> SamplePreview::position() const {
        stdc_impl_t;
        if (impl.state != Playing || !impl.playedFrom) {
            return std::nullopt;
        }
        const double heard = impl.output->heardPosition().value_or(0);
        return *impl.playedFrom + heard * 1000 / std::max(1, impl.deviceRate);
    }

    std::shared_ptr<const kit::WaveAudio> SamplePreview::synthesized() const {
        stdc_impl_t;
        return impl.synthesized;
    }

    int SamplePreview::noteNumFor(const QMap<int, kit::VoicePrefix> &prefixMap,
                                  const std::filesystem::path &directory, const QString &alias) {
        const auto folder = QString::fromStdU16String(directory.generic_u16string());
        QList<int> keys;
        for (auto it = prefixMap.cbegin(); it != prefixMap.cend(); ++it) {
            const auto &prefix = it->prefix;
            const auto &suffix = it->suffix;
            if (prefix.isEmpty() && suffix.isEmpty()) {
                continue;
            }
            auto path = prefix;
            path.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const bool inFolder = !folder.isEmpty() && path == folder + QLatin1Char('/');
            const bool enclosed = alias.size() > prefix.size() + suffix.size() &&
                                  alias.startsWith(prefix) && alias.endsWith(suffix);
            if (inFolder || enclosed) {
                keys.push_back(it.key());
            }
        }
        return keys.isEmpty() ? 60 : keys.at(keys.size() / 2);
    }

}
