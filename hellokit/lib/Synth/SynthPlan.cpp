#include "SynthPlan.h"

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>

#include <stdutau/synth.h>
#include <stdutau/utautils.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message, int noteIndex) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, noteIndex});
        }

        std::string utf8(const QString &text) {
            const auto bytes = text.toUtf8();
            return std::string(bytes.constData(), size_t(bytes.size()));
        }

        /// A path as the engines are handed it, which is UTF-8 like every other argument.
        std::string utf8(const fs::path &path) {
            return path.u8string();
        }

        /// Six characters standing for a digest, in the alphabet UTAU uses for the same field.
        QString shortened(const QByteArray &digest) {
            static const char ALPHABET[] =
                "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            quint64 value = 0;
            for (int i = 0; i < 8 && i < digest.size(); ++i) {
                value = (value << 8) | quint8(digest.at(i));
            }
            QString out;
            for (int i = 0; i < 6; ++i) {
                out.prepend(QLatin1Char(ALPHABET[value % 62]));
                value /= 62;
            }
            return out;
        }

        /// What the sample is right now, as far as anything here can tell.
        QString sampleState(const fs::path &sample) {
            std::error_code error;
            const auto size = fs::file_size(sample, error);
            if (error) {
                return QStringLiteral("gone");
            }
            const auto when = fs::last_write_time(sample, error).time_since_epoch().count();
            return QStringLiteral("%1/%2").arg(qulonglong(size)).arg(qlonglong(when));
        }

        /// Where one note's rendered piece goes.
        ///
        /// A piece already on disk is taken as done and the resampler is not run for it again,
        /// so the name has to change whenever anything that changes the sound changes. UTAU's
        /// own name does not manage that. On the 455-note probe its six characters follow the
        /// note's timing and nothing else: 358 notes carrying 163 different sets of flags all
        /// came out with the same six, and changing the pitch line, the vibrato, the envelope,
        /// the intensity or the modulation left them alone too. Reusing on that name renders
        /// the note again with its old flags.
        ///
        /// So the last field is a digest of everything the resampler is handed, and of the
        /// sample as it sits on disk. What the *wavtool* is handed is left out on purpose: the
        /// envelope and the start point are applied while the piece is appended, not while it
        /// is rendered, so two notes differing only there share a piece and rightly do.
        ///
        /// The rest of the name is UTAU's, because it is the part a person reads: the note's
        /// place in the track, its lyric and its tone. stdutau builds that and takes the
        /// characters a file name cannot hold out of the lyric, which is why it is kept rather
        /// than assembled again here.
        fs::path cacheFileFor(const std::string &utauName, const utau::ResamplerArguments &wanted,
                              const fs::path &sample, const fs::path &directory) {
            auto forDigest = wanted;
            forDigest.outFile.clear(); // or the name would stand for itself
            QByteArray subject;
            for (const auto &argument : forDigest.arguments()) {
                subject += QByteArray(argument.data(), qsizetype(argument.size()));
                subject += '\n';
            }
            subject += sampleState(sample).toUtf8();

            const QString name = QString::fromUtf8(utauName.data(), qsizetype(utauName.size()));
            const QString stem = name.left(name.lastIndexOf(QLatin1Char('.')));
            const int lastField = stem.lastIndexOf(QLatin1Char('_'));
            const QString kept = lastField < 0 ? stem : stem.left(lastField);
            const QString digest =
                shortened(QCryptographicHash::hash(subject, QCryptographicHash::Sha1));
            return directory /
                   fs::u8path((kept + QLatin1Char('_') + digest).toStdString() + ".wav");
        }

        QStringList listOf(const std::vector<std::string> &arguments) {
            QStringList list;
            list.reserve(qsizetype(arguments.size()));
            for (const auto &argument : arguments) {
                list.push_back(QString::fromUtf8(argument.data(), qsizetype(argument.size())));
            }
            return list;
        }

        /// The note as the synth reads it.
        ///
        /// Only what \c Synth::calc actually looks at. The rest of a note, its label and its
        /// patch and whatever a host stored on it, has no bearing on what the engines are asked
        /// to do, and copying it here would only suggest otherwise.
        utau::Note synthNote(const Note &from) {
            utau::Note note;
            note.lyric = utf8(from.lyric);
            note.flags = utf8(from.flags);
            note.noteNum = from.noteNum;
            note.length = from.length;

            note.intensity = from.intensity;
            note.modulation = from.modulation;
            note.velocity = from.velocity;
            note.preUttr = from.preUtterance;
            note.overlap = from.voiceOverlap;
            note.stp = from.startPoint;
            note.tempo = from.tempo;

            if (from.envelope) {
                utau::Envelope envelope;
                for (qsizetype i = 0; i < from.envelope->anchors.size() && i < 5; ++i) {
                    const auto &anchor = from.envelope->anchors.at(i);
                    envelope.anchors[size_t(i)] = utau::Point(anchor.x, anchor.y);
                }
                note.envelope = envelope;
            }

            if (from.vibrato) {
                utau::Vibrato vibrato;
                vibrato.length = from.vibrato->length;
                vibrato.period = from.vibrato->period;
                vibrato.amplitude = from.vibrato->amplitude;
                vibrato.attack = from.vibrato->attack;
                vibrato.release = from.vibrato->release;
                vibrato.phase = from.vibrato->phase;
                vibrato.offset = from.vibrato->offset;
                vibrato.intensity = from.vibrato->intensity;
                note.vibrato = vibrato;
            }

            for (const auto &point : from.portamento) {
                auto type = utau::Point::SJoin;
                switch (point.type) {
                    case PortamentoType::Linear:
                        type = utau::Point::LinearJoin;
                        break;
                    case PortamentoType::R:
                        type = utau::Point::RJoin;
                        break;
                    case PortamentoType::J:
                        type = utau::Point::JJoin;
                        break;
                    case PortamentoType::S:
                        break;
                }
                note.portamento.emplace_back(point.x, point.y, type);
            }
            return note;
        }

    }

    std::optional<SynthPlan> SynthPlan::make(const Project &project, const VoiceBank &bank,
                                             const Options &options, DiagnosticList &diagnostics) {
        if (project.tracks.size() != 1) {
            fail(diagnostics, tr("A render takes one track, and this project holds %1.")
                                  .arg(project.tracks.size()));
            return std::nullopt;
        }

        const auto &notes = project.tracks.first().notes;
        if (notes.isEmpty()) {
            fail(diagnostics, tr("This track holds no notes."));
            return std::nullopt;
        }
        if (options.outputFile.empty() || options.cacheDirectory.empty()) {
            fail(diagnostics, tr("A render needs somewhere to write and somewhere to cache."));
            return std::nullopt;
        }

        // Converted once. calc() asks for a note several times over, once for itself and again
        // as its neighbours' context.
        std::vector<utau::Note> converted;
        converted.reserve(size_t(notes.size()));
        for (const auto &note : notes) {
            converted.push_back(synthNote(note));
        }

        const std::pair<int, int> limits{0, int(notes.size()) - 1};
        const auto range = options.range.value_or(limits);
        if (range.first < limits.first || range.second > limits.second ||
            range.first > range.second) {
            fail(diagnostics, tr("There is no note in the range that was asked for."));
            return std::nullopt;
        }

        // calc() reaches past the ends on purpose, and asks for the note after the last one
        // without checking. A default note is what it expects to find there.
        const auto noteGetter = [&converted](int index) -> utau::Note {
            if (index < 0 || index >= int(converted.size())) {
                return {};
            }
            return converted[size_t(index)];
        };

        // Which sample sings a note, which is the one thing here the voice bank decides.
        const auto otoEntryGetter = [&bank](const utau::Note &note) -> utau::OtoEntry {
            const auto lyric = QString::fromUtf8(note.lyric.data(), qsizetype(note.lyric.size()));
            const auto *sample = bank.find(note.noteNum, lyric);
            if (!sample) {
                return {};
            }

            utau::OtoEntry entry;
            // The whole path, not the name the oto.ini carried. A bank spreads its samples over
            // subdirectories, so the name alone does not say which file it is by the time it
            // reaches an engine.
            entry.fileName = utf8(sample->path);
            entry.alias = utf8(sample->alias);
            entry.offset = sample->offset;
            entry.consonant = sample->consonant;
            entry.cutoff = sample->cutoff;
            entry.preUtterance = sample->preUtterance;
            entry.voiceOverlap = sample->voiceOverlap;
            return entry;
        };

        const auto params =
            utau::Synth::calc(limits, range, project.settings.tempo, utf8(project.settings.flags),
                              noteGetter, otoEntryGetter);

        SynthPlan plan;
        plan.m_outputFile = options.outputFile;
        plan.m_cacheDirectory = options.cacheDirectory;
        plan.m_steps.reserve(qsizetype(params.size()));

        for (size_t i = 0; i < params.size(); ++i) {
            auto [resampler, wavtool] = params[i];
            const int noteIndex = range.first + int(i);

            SynthStep step;
            step.noteIndex = noteIndex;
            step.sample = fs::u8path(resampler.inFile);
            step.silent = resampler.inFile.empty();

            if (step.silent && !notes.at(noteIndex).isRest()) {
                complain(diagnostics,
                         tr("This voice bank has nothing to sing \"%1\" with, so the note is "
                            "silent.")
                             .arg(notes.at(noteIndex).lyric),
                         noteIndex);
            }

            // calc() names the cache file but not where it goes, and leaves the track file to
            // the caller entirely. The last field of the name is ours, not calc()'s: see
            // cacheFileFor().
            step.cacheFile =
                cacheFileFor(resampler.outFile, resampler, step.sample, options.cacheDirectory);

            resampler.outFile = utf8(step.cacheFile);
            wavtool.inFile = utf8(step.cacheFile);
            wavtool.outFile = utf8(options.outputFile);

            if (!step.silent) {
                step.resamplerArguments = listOf(resampler.arguments());
            }
            step.wavtoolArguments = listOf(wavtool.arguments());

            plan.m_steps.push_back(std::move(step));
        }
        return plan;
    }

}
