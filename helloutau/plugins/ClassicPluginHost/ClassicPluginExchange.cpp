#include "ClassicPluginExchange.h"

#include <set>
#include <string>

#include <stdutau/pluginfile.h>
#include <stdutau/utaconst.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/Project.h>
#include <hellokit/Document/TempoMap.h>
#include <hellokit/Document/UstDocument.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/SampleTiming.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include "ClassicPlugin.h"

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        // The text of a file of the plugin, escaped in an encoding other than UTF-8, see
        // TextCodec::escape().
        class Text {
        public:
            explicit Text(const QString &charset)
                : m_codec(charset), m_escaping(!m_codec.isUtf8()) {
            }

            const kit::TextCodec &codec() const {
                return m_codec;
            }

            bool escaping() const {
                return m_escaping;
            }

            std::string encode(const QString &text) const {
                const auto bytes = m_codec.encode(m_escaping ? m_codec.escape(text) : text);
                return std::string(bytes.data(), size_t(bytes.size()));
            }

            QString decode(const std::string &bytes) const {
                const auto text =
                    m_codec.decodeReplacing(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
                return m_escaping ? kit::TextCodec::unescape(text) : text;
            }

        private:
            kit::TextCodec m_codec;
            bool m_escaping;
        };

        // The entries that the note properties hold. Any other entry is user data.
        const std::set<std::string> &propertyKeys() {
            static const std::set<std::string> keys = {
                utau::KEY_NAME_LYRIC,         utau::KEY_NAME_LENGTH,
                utau::KEY_NAME_NOTE_NUM,      utau::KEY_NAME_INTENSITY,
                utau::KEY_NAME_MODULATION,    utau::KEY_NAME_VELOCITY,
                utau::KEY_NAME_PRE_UTTERANCE, utau::KEY_NAME_VOICE_OVERLAP,
                utau::KEY_NAME_START_POINT,   utau::KEY_NAME_TEMPO,
                utau::KEY_NAME_FLAGS,         utau::KEY_NAME_LABEL,
                utau::KEY_NAME_DIRECT,        utau::KEY_NAME_PATCH,
                utau::KEY_NAME_REGION_START,  utau::KEY_NAME_REGION_END,
                utau::KEY_NAME_ENVELOPE,      utau::KEY_NAME_VBR,
                utau::KEY_NAME_PBS,           utau::KEY_NAME_PBW,
                utau::KEY_NAME_PBY,           utau::KEY_NAME_PBM,
                utau::KEY_NAME_PITCH_BEND,    utau::KEY_NAME_PB_START,
            };
            return keys;
        }

        // Sets the properties of \a ref that \a section has to those of \a note , which the
        // section converts to. A property whose entry is empty in the section is blank in the
        // note, which removes it.
        void assign(const kit::NoteRef &ref, const utau::PluginResult::Section &section,
                    const kit::Note &note, const Text &text) {
            const auto has = [&](const char *key) { return section.keys.count(key) != 0; };

            if (has(utau::KEY_NAME_LYRIC)) {
                ref.setLyric(note.lyric);
            }
            if (has(utau::KEY_NAME_LENGTH)) {
                ref.setLength(note.length);
            }
            if (has(utau::KEY_NAME_NOTE_NUM)) {
                ref.setNoteNum(note.noteNum);
            }
            if (has(utau::KEY_NAME_INTENSITY)) {
                ref.setIntensity(note.intensity);
            }
            if (has(utau::KEY_NAME_MODULATION)) {
                ref.setModulation(note.modulation);
            }
            if (has(utau::KEY_NAME_VELOCITY)) {
                ref.setVelocity(note.velocity);
            }
            if (has(utau::KEY_NAME_PRE_UTTERANCE)) {
                ref.setPreUtterance(note.preUtterance);
            }
            if (has(utau::KEY_NAME_VOICE_OVERLAP)) {
                ref.setVoiceOverlap(note.voiceOverlap);
            }
            if (has(utau::KEY_NAME_START_POINT)) {
                ref.setStartPoint(note.startPoint);
            }
            if (has(utau::KEY_NAME_TEMPO)) {
                ref.setTempo(note.tempo);
            }
            if (has(utau::KEY_NAME_FLAGS)) {
                ref.setFlags(note.flags);
            }
            if (has(utau::KEY_NAME_LABEL)) {
                ref.setLabel(note.label);
            }
            if (has(utau::KEY_NAME_DIRECT)) {
                ref.setDirect(note.direct);
            }
            if (has(utau::KEY_NAME_PATCH)) {
                ref.setPatch(note.patch);
            }
            if (has(utau::KEY_NAME_REGION_START)) {
                ref.setRegion(note.region);
            }
            if (has(utau::KEY_NAME_REGION_END)) {
                ref.setRegionEnd(note.regionEnd);
            }
            if (has(utau::KEY_NAME_ENVELOPE)) {
                ref.setEnvelope(note.envelope);
            }
            if (has(utau::KEY_NAME_VBR)) {
                ref.setVibrato(note.vibrato);
            }

            // The four entries of the Mode2 pitch together make one curve.
            if (has(utau::KEY_NAME_PBS) || has(utau::KEY_NAME_PBW) || has(utau::KEY_NAME_PBY) ||
                has(utau::KEY_NAME_PBM)) {
                const auto points = ref.portamento();
                if (points.size() > 0) {
                    points.remove(0, points.size());
                }
                if (!note.portamento.isEmpty()) {
                    points.insert(0, note.portamento);
                }
            }

            // The start and the values of the Mode1 pitch, each kept if the section omits it
            if (has(utau::KEY_NAME_PITCH_BEND) || has(utau::KEY_NAME_PB_START)) {
                const auto current = ref.pitchBend();
                kit::PitchBend bend = current.isValid() ? current.toPitchBend() : kit::PitchBend();
                const auto given = note.pitchBend.value_or(kit::PitchBend());
                if (has(utau::KEY_NAME_PB_START)) {
                    bend.start = given.start;
                }
                if (has(utau::KEY_NAME_PITCH_BEND)) {
                    bend.values = given.values;
                }
                ref.setPitchBend(bend.start || !bend.values.isEmpty()
                                     ? std::optional<kit::PitchBend>(bend)
                                     : std::nullopt);
            }

            const auto data = ref.userData();
            for (const auto &key : section.keys) {
                if (propertyKeys().count(key) != 0) {
                    continue;
                }
                const auto name = text.decode(key);
                const auto value = note.userData.value(name);
                if (value.isEmpty()) {
                    data.remove(name);
                } else {
                    data.setValue(name, value);
                }
            }
        }

    }

    QByteArray ClassicPluginExchange::input(const ClassicPlugin &plugin,
                                            const kit::Project &project, int first, int count,
                                            const Paths &paths, const kit::VoiceBank *voiceBank) {
        const Text text(plugin.charset);
        const auto &notes = project.tracks.first().notes;
        if (plugin.wholeTrack) {
            first = 0;
            count = int(notes.size());
        }
        const auto tempos = kit::TempoMap::of(project);
        const auto timings = kit::SampleTiming::of(notes, tempos, voiceBank);

        // A note with the values that the synthesis computes for it. A rest has no sample, and
        // its values are zero, as UTAU writes them.
        const auto noteAt = [&](int index) {
            const auto &from = notes.at(index);
            utau::NoteExt note;
            static_cast<utau::Note &>(note) =
                kit::UstDocument::noteToUst(from, text.codec(), text.escaping());
            const auto &timing = timings.at(index);
            note.preUttrRO = timing.preUtterance;
            note.overlapRO = timing.voiceOverlap;
            note.stpRO = timing.startPoint;
            if (voiceBank && !from.isRest()) {
                if (const auto sample = voiceBank->find(from.noteNum, from.lyric)) {
                    const auto file = sample->path.lexically_proximate(voiceBank->root());
                    note.filenameRO = text.encode(textOf(file.lexically_normal().make_preferred()));
                    // Written only where the prefix map sings the lyric under another alias
                    const auto alias = voiceBank->prefixedLyric(from.noteNum, from.lyric);
                    if (alias != from.lyric) {
                        note.aliasRO = text.encode(alias);
                    }
                }
            }
            return note;
        };

        utau::PluginInput file;
        file.settings.project = text.encode(textOf(paths.project));
        file.settings.tempo = notes.isEmpty() ? project.settings.tempo
                                              : tempos.tempo(qMin(first, int(notes.size()) - 1));
        file.settings.voiceDir = text.encode(textOf(paths.voiceDirectory));
        file.settings.cacheDir = text.encode(textOf(paths.cacheDirectory));
        file.settings.isMode2 = project.settings.mode2;
        file.startIndex = first;
        if (!plugin.wholeTrack && first > 0) {
            file.prevNote = noteAt(first - 1);
        }
        for (int i = first; i < first + count; ++i) {
            file.notes.push_back(noteAt(i));
        }
        if (!plugin.wholeTrack && first + count < notes.size()) {
            file.nextNote = noteAt(first + count);
        }
        const auto bytes = file.write();
        return QByteArray(bytes.data(), qsizetype(bytes.size()));
    }

    ClassicPluginExchange::Outcome ClassicPluginExchange::apply(const ClassicPlugin &plugin,
                                                                const kit::NoteListRef &notes,
                                                                int first, int count,
                                                                QByteArrayView result,
                                                                kit::DiagnosticList &diagnostics) {
        utau::PluginResult file;
        file.read(std::string_view(result.data(), size_t(result.size())));
        if (file.isCancelled()) {
            return Cancelled;
        }

        const Text text(plugin.charset);
        if (plugin.wholeTrack) {
            first = 0;
            count = notes.size();
        }
        const auto ignored = [&](const QString &section) {
            diagnostics.push_back(
                {kit::DiagnosticSeverity::Warning,
                 tr("The plugin wrote %1 where there is no note, which was ignored.")
                     .arg(section)});
        };

        auto transaction = notes.session()->transaction(plugin.name);
        // The next note of the selection, and the end of the selection, which the insertions
        // and deletions move
        int next = first;
        int end = first + count;
        for (const auto &section : file.sections) {
            using Section = utau::PluginResult::Section;
            const auto note =
                kit::UstDocument::noteFromUst(section.note, text.codec(), text.escaping());
            switch (section.kind) {
                case Section::Prev:
                    if (plugin.wholeTrack || first == 0) {
                        ignored(QStringLiteral("[#PREV]"));
                        break;
                    }
                    assign(notes.at(first - 1), section, note, text);
                    break;
                case Section::Next:
                    if (plugin.wholeTrack || end >= notes.size()) {
                        ignored(QStringLiteral("[#NEXT]"));
                        break;
                    }
                    assign(notes.at(end), section, note, text);
                    break;
                case Section::Numbered:
                    if (next >= end) {
                        ignored(
                            QStringLiteral("[#%1]").arg(section.number, 4, 10, QLatin1Char('0')));
                        break;
                    }
                    assign(notes.at(next), section, note, text);
                    ++next;
                    break;
                case Section::Delete:
                    if (next >= end) {
                        ignored(QStringLiteral("[#DELETE]"));
                        break;
                    }
                    notes.remove(next, 1);
                    --end;
                    break;
                case Section::Insert: {
                    // What the section omits of the length, the lyric and the note number comes
                    // from the note after it, as UTAU inserts.
                    // At the end of the track, those of a new note.
                    auto inserted = note;
                    kit::Note after;
                    if (next < notes.size()) {
                        after = notes.at(next).toNote();
                    } else {
                        after.lyric = QString::fromLatin1(utau::DEFAULT_LYRIC);
                        after.length = kit::ticksPerQuarter;
                        after.noteNum = 60;
                    }
                    if (section.keys.count(utau::KEY_NAME_LENGTH) == 0) {
                        inserted.length = after.length;
                    }
                    if (section.keys.count(utau::KEY_NAME_LYRIC) == 0) {
                        inserted.lyric = after.lyric;
                    }
                    if (section.keys.count(utau::KEY_NAME_NOTE_NUM) == 0) {
                        inserted.noteNum = after.noteNum;
                    }
                    notes.insert(next, {inserted});
                    ++next;
                    ++end;
                    break;
                }
            }
        }
        return transaction.commit(diagnostics) ? Applied : Failed;
    }

}
