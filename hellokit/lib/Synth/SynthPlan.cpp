#include "SynthPlan.h"

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>

#include <stdutau/synth.h>
#include <stdutau/utautils.h>

#include <hellokit/Document/TempoMap.h>

#include "PitchCurve.h"

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

        /// A path in the form passed to the engines, which is UTF-8 like every other argument.
        std::string utf8(const fs::path &path) {
            return path.u8string();
        }

        /// A six-character representation of a digest, in the alphabet UTAU uses for this field.
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

        /// An identifier of the current state of the sample, as far as it can be determined.
        QString sampleState(const fs::path &sample) {
            std::error_code error;
            const auto size = fs::file_size(sample, error);
            if (error) {
                return QStringLiteral("notfound");
            }
            const auto when = fs::last_write_time(sample, error).time_since_epoch().count();
            return QStringLiteral("%1/%2").arg(qulonglong(size)).arg(qlonglong(when));
        }

        /// The path of the rendered fragment of one note.
        ///
        /// A fragment already on disk is treated as complete and the resampler is not run for
        /// it again, so the name must change whenever any input that affects the sound changes.
        /// The UTAU naming scheme does not satisfy this. In the 455-note probe, its six
        /// characters depend only on the timing of the note: 358 notes with 163 distinct flag
        /// sets all received the same six characters, and changes to the pitch curve, vibrato,
        /// envelope, intensity or modulation left them unchanged as well. Reuse based on that
        /// name renders the note with its previous flags.
        ///
        /// The last field is therefore a digest of all resampler arguments and of the current
        /// state of the sample on disk. The wavtool arguments are deliberately excluded: the
        /// envelope and the start point are applied when the fragment is appended, not when it
        /// is rendered, so two notes that differ only in these share a fragment correctly.
        ///
        /// The remainder of the name follows UTAU, because it is the human-readable part: the
        /// position of the note in the track, its lyric and its tone. stdutau builds this part
        /// and removes characters that are invalid in file names from the lyric, which is why
        /// it is reused rather than reimplemented here.
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

        /// The note in the form read by the synthesis calculation.
        ///
        /// Contains only the fields that \c Synth::calc reads. The other fields of a note, such
        /// as its label, its patch and any data stored by a host, do not affect the engine
        /// calls, and copying them would falsely suggest otherwise.
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
                // stdutau takes the anchors in time order. Without a middle anchor, index four
                // keeps its default, from which stdutau infers four anchors.
                utau::Envelope envelope;
                const auto anchors = from.envelope->anchorsInTimeOrder();
                for (qsizetype i = 0; i < anchors.size(); ++i) {
                    envelope.anchors[size_t(i)] = utau::Point(anchors[i].x, anchors[i].y);
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
                    case PortamentoPoint::Linear:
                        type = utau::Point::LinearJoin;
                        break;
                    case PortamentoPoint::R:
                        type = utau::Point::RJoin;
                        break;
                    case PortamentoPoint::J:
                        type = utau::Point::JJoin;
                        break;
                    case PortamentoPoint::S:
                        break;
                }
                note.portamento.emplace_back(point.x, PortamentoPoint::tenthsFromCents(point.y),
                                             type);
            }
            return note;
        }

    }

    std::optional<SynthPlan> SynthPlan::make(const Project &project, const VoiceBank &bank,
                                             const Options &requested,
                                             DiagnosticList &diagnostics) {
        // The paths in the preferred separators of the system. The rendering script and the
        // engines receive them as text, and the copy command of Windows rejects a path written
        // with forward slashes, which a file dialog of Qt returns.
        auto options = requested;
        options.outputFile.make_preferred();
        options.cacheDirectory.make_preferred();

        if (project.tracks.size() != 1) {
            fail(diagnostics,
                 tr("Rendering requires exactly one track, but this project contains %1.")
                     .arg(project.tracks.size()));
            return std::nullopt;
        }

        const auto &notes = project.tracks.first().notes;
        if (notes.isEmpty()) {
            fail(diagnostics, tr("This track contains no notes."));
            return std::nullopt;
        }
        if (options.outputFile.empty() || options.cacheDirectory.empty()) {
            fail(diagnostics, tr("Rendering requires an output file and a cache directory."));
            return std::nullopt;
        }

        // Converted once, because calc() requests each note several times, once for itself and
        // again as context for its neighbors.
        std::vector<utau::Note> converted;
        converted.reserve(size_t(notes.size()));
        for (const auto &note : notes) {
            converted.push_back(synthNote(note));
        }

        const std::pair<int, int> limits{0, int(notes.size()) - 1};
        const auto range = options.range.value_or(limits);
        if (range.first < limits.first || range.second > limits.second ||
            range.first > range.second) {
            fail(diagnostics, tr("The requested range contains no notes."));
            return std::nullopt;
        }

        // calc() deliberately accesses beyond both ends and requests the note after the last
        // one without a bounds check. It requires a default note there.
        const auto noteGetter = [&converted](int index) -> utau::Note {
            if (index < 0 || index >= int(converted.size())) {
                return {};
            }
            return converted[size_t(index)];
        };

        // The sample for a note, the only input here determined by the voice bank.
        const auto otoEntryGetter = [&bank](const utau::Note &note) -> utau::OtoEntry {
            const auto lyric = QString::fromUtf8(note.lyric.data(), qsizetype(note.lyric.size()));
            const auto *sample = bank.find(note.noteNum, lyric);
            if (!sample) {
                return {};
            }

            utau::OtoEntry entry;
            // The full path, not the name in the oto.ini. A voice bank distributes its samples
            // over subdirectories, so the name alone does not identify the file once it reaches
            // an engine.
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
        const auto tempos = TempoMap::of(project);
        plan.m_startTime = tempos.startTime(range.first) -
                           (params.empty() ? 0 : params.front().first.correctPreUttr);

        for (size_t i = 0; i < params.size(); ++i) {
            auto [resampler, wavtool] = params[i];
            const int noteIndex = range.first + int(i);

            // With Mode2 off UTAU passes the curve of Mode1 instead, without the points and
            // the vibrato that calc() drew (docs/Synth.md). Before the cache name, which
            // depends on the curve.
            if (!project.settings.mode2 && !resampler.inFile.empty()) {
                PitchCurve::Timing timing;
                timing.preUtterance = resampler.correctPreUttr;
                timing.startPoint = resampler.correctStp;
                if (i + 1 < params.size()) {
                    timing.nextPreUtterance = params[i + 1].first.correctPreUttr;
                    timing.nextOverlap = params[i + 1].first.correctOverlap;
                }
                const auto curve =
                    PitchCurve(notes, noteIndex, tempos.tempo(noteIndex)).mode1Values(timing);
                resampler.pitchCurves.assign(curve.begin(), curve.end());
            }

            SynthStep step;
            step.noteIndex = noteIndex;
            step.sample = fs::u8path(resampler.inFile);
            step.silent = resampler.inFile.empty();

            if (step.silent && !notes.at(noteIndex).isRest()) {
                complain(diagnostics,
                         tr("This voice bank has no sample for \"%1\", so the note is silent.")
                             .arg(notes.at(noteIndex).lyric),
                         noteIndex);
            }

            // calc() determines the cache file name but not its directory, and leaves the track
            // file entirely to the caller. The last field of the name is computed here, not by
            // calc(). See cacheFileFor().
            step.cacheFile =
                cacheFileFor(resampler.outFile, resampler, step.sample, options.cacheDirectory);

            resampler.outFile = utf8(step.cacheFile);
            // A silent note passes R.wav of the voice bank, as UTAU does (docs/Synth.md). The
            // file does not exist, and the wavtool appends silence for it.
            wavtool.inFile = utf8(step.silent ? bank.root() / "R.wav" : step.cacheFile);
            wavtool.outFile = utf8(options.outputFile);

            if (!step.silent) {
                step.resamplerArguments = listOf(resampler.arguments());
            }
            step.wavtoolArguments = listOf(wavtool.arguments());
            step.preUtterance = resampler.correctPreUttr;
            step.voiceOverlap = resampler.correctOverlap;
            step.startPoint = resampler.correctStp;
            step.pitch = QList<int>(resampler.pitchCurves.begin(), resampler.pitchCurves.end());

            plan.m_steps.push_back(std::move(step));
        }
        return plan;
    }
}
