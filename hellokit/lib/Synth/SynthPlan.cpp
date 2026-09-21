#include "SynthPlan.h"

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>

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
                                             const Options &options,
                                             DiagnosticList &diagnostics) {
        if (project.tracks.size() != 1) {
            fail(diagnostics, SynthPlan::tr("A render takes one track, and this project holds %1.")
                                  .arg(project.tracks.size()));
            return std::nullopt;
        }

        const auto &notes = project.tracks.first().notes;
        if (notes.isEmpty()) {
            fail(diagnostics, SynthPlan::tr("This track holds no notes."));
            return std::nullopt;
        }
        if (options.outputFile.empty() || options.cacheDirectory.empty()) {
            fail(diagnostics, SynthPlan::tr("A render needs somewhere to write and somewhere to cache."));
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
            fail(diagnostics, SynthPlan::tr("There is no note in the range that was asked for."));
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

        const auto params = utau::Synth::calc(limits, range, project.settings.tempo,
                                              utf8(project.settings.flags), noteGetter,
                                              otoEntryGetter);

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
                         SynthPlan::tr("This voice bank has nothing to sing \"%1\" with, so the note is "
                            "silent.")
                             .arg(notes.at(noteIndex).lyric),
                         noteIndex);
            }

            // calc() names the cache file but not where it goes, and leaves the track file to
            // the caller entirely.
            step.cacheFile = options.cacheDirectory / fs::u8path(resampler.outFile);

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
