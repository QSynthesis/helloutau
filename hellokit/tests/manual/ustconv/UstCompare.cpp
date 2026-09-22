#include "UstCompare.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>

#include <stdutau/ustfile.h>

#include <hellokit/Document/DocumentConstants.h>

namespace ustconv {

    namespace {

        std::string show(bool value) {
            return value ? "true" : "false";
        }

        std::string show(int value) {
            return std::to_string(value);
        }

        /// Sufficient digits to distinguish any two doubles, which is the purpose of printing them.
        std::string show(double value) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.10g", value);
            return buffer;
        }

        std::string show(const std::string &value) {
            return '"' + value + '"';
        }

        std::string show(const std::optional<double> &value) {
            return value ? show(*value) : std::string("absent");
        }

        std::string show(utau::Point::Type type) {
            const auto written = utau::Point::typeToString(type);
            return written.empty() ? std::string("\"\" (S curve)") : show(written);
        }

        /// Collects differences, each under a prefix identifying its location in the file.
        class Recorder {
        public:
            explicit Recorder(std::vector<Difference> &out) : _out(out) {
            }

            void enter(std::string where) {
                _where = std::move(where);
            }

            template <class T>
            void check(const std::string &field, const T &before, const T &after) {
                if (before == after) {
                    return;
                }
                _out.push_back({_where + field, show(before), show(after)});
            }

            void record(const std::string &field, std::string before, std::string after) {
                _out.push_back({_where + field, std::move(before), std::move(after)});
            }

        private:
            std::vector<Difference> &_out;
            std::string _where;
        };

        std::string at(const char *name, size_t i) {
            return name + ('[' + std::to_string(i) + ']');
        }

        /// \return whether both sides have the value. A value present on only one side is
        ///         recorded as a difference.
        bool bothHave(Recorder &r, const char *field, bool before, bool after) {
            if (before == after) {
                return before;
            }
            r.record(field, before ? "present" : "absent", after ? "present" : "absent");
            return false;
        }

        void compareEnvelope(Recorder &r, const std::optional<utau::Envelope> &before,
                             const std::optional<utau::Envelope> &after) {
            if (!bothHave(r, "envelope", before.has_value(), after.has_value())) {
                return;
            }
            r.check("envelope anchors", before->count(), after->count());

            const int count = std::min(before->count(), after->count());
            for (int i = 0; i < count; ++i) {
                const auto &a = before->anchors.at(size_t(i));
                const auto &b = after->anchors.at(size_t(i));
                r.check(at("envelope", size_t(i)) + ".x", a.x, b.x);
                r.check(at("envelope", size_t(i)) + ".y", a.y, b.y);
            }
        }

        void compareVibrato(Recorder &r, const std::optional<utau::Vibrato> &before,
                            const std::optional<utau::Vibrato> &after) {
            if (!bothHave(r, "vibrato", before.has_value(), after.has_value())) {
                return;
            }
            r.check("vibrato.length", before->length, after->length);
            r.check("vibrato.period", before->period, after->period);
            r.check("vibrato.amplitude", before->amplitude, after->amplitude);
            r.check("vibrato.attack", before->attack, after->attack);
            r.check("vibrato.release", before->release, after->release);
            r.check("vibrato.phase", before->phase, after->phase);
            r.check("vibrato.offset", before->offset, after->offset);
            r.check("vibrato.intensity", before->intensity, after->intensity);
        }

        void comparePortamento(Recorder &r, const std::vector<utau::Point> &before,
                               const std::vector<utau::Point> &after) {
            r.check("portamento points", int(before.size()), int(after.size()));

            const size_t count = std::min(before.size(), after.size());
            for (size_t i = 0; i < count; ++i) {
                const auto name = at("portamento", i);
                r.check(name + ".x", before[i].x, after[i].x);
                r.check(name + ".y", before[i].y, after[i].y);
                r.check(name + ".type", before[i].type, after[i].type);
            }
        }

        void comparePitches(Recorder &r, const std::vector<double> &before,
                            const std::vector<double> &after) {
            r.check("pitch samples", int(before.size()), int(after.size()));

            const size_t count = std::min(before.size(), after.size());
            for (size_t i = 0; i < count; ++i) {
                r.check(at("pitches", i), before[i], after[i]);
            }
        }

        using UserData = std::map<std::string, std::string>;

        void compareUserData(Recorder &r, const UserData &before, const UserData &after,
                             const Normalizer &beforeText, const Normalizer &afterText) {
            // Keyed by the decoded name, because the two files may use different encodings.
            // UTAU preserves only entries whose names begin with $, but this library preserves
            // every entry it reads, so any name may occur here.
            UserData first, second;
            for (const auto &[key, value] : before) {
                first[beforeText(key)] = beforeText(value);
            }
            for (const auto &[key, value] : after) {
                second[afterText(key)] = afterText(value);
            }

            for (const auto &[key, value] : first) {
                const auto it = second.find(key);
                if (it == second.end()) {
                    r.record("userData[" + key + "]", show(value), "absent");
                    continue;
                }
                r.check("userData[" + key + "]", value, it->second);
            }
            for (const auto &[key, value] : second) {
                if (first.count(key) == 0) {
                    r.record("userData[" + key + "]", "absent", show(value));
                }
            }
        }

        void compareNote(Recorder &r, const utau::Note &before, const utau::Note &after,
                         const Normalizer &beforeText, const Normalizer &afterText) {
            const auto text = [&](const std::string &field, const std::string &a,
                                  const std::string &b) {
                r.check(field, beforeText(a), afterText(b));
            };

            text("lyric", before.lyric, after.lyric);
            r.check("noteNum", before.noteNum, after.noteNum);
            r.check("length", before.length, after.length);
            text("flags", before.flags, after.flags);

            r.check("intensity", before.intensity, after.intensity);
            r.check("modulation", before.modulation, after.modulation);
            r.check("velocity", before.velocity, after.velocity);
            r.check("preUttr", before.preUttr, after.preUttr);
            r.check("overlap", before.overlap, after.overlap);
            r.check("stp", before.stp, after.stp);
            r.check("tempo", before.tempo, after.tempo);

            compareEnvelope(r, before.envelope, after.envelope);
            compareVibrato(r, before.vibrato, after.vibrato);
            comparePortamento(r, before.portamento, after.portamento);

            r.check("pbstart", before.pbstart, after.pbstart);
            comparePitches(r, before.pitches, after.pitches);
            text("pbtype", before.pbtype, after.pbtype);

            text("label", before.label, after.label);
            text("direct", before.direct, after.direct);
            text("patch", before.patch, after.patch);
            text("region", before.region, after.region);
            text("regionEnd", before.regionEnd, after.regionEnd);

            compareUserData(r, before.userData, after.userData, beforeText, afterText);
        }

        bool isControlNote(const utau::Note &note) {
            return note.lyric == hello::kit::controlNoteLyric &&
                   note.userData.count(hello::kit::controlNoteEntry) != 0;
        }

        /// The notes of the project, excluding the control note.
        ///
        /// Only the first control note is excluded, as when a UST is read into a project. A
        /// second one is a note of the user, and silently losing it would be exactly the kind of
        /// defect this program is meant to detect.
        std::vector<const utau::Note *> notesOf(const utau::UstFile &file) {
            std::vector<const utau::Note *> notes;
            bool eaten = false;
            for (const auto &note : file.notes) {
                if (!eaten && isControlNote(note)) {
                    eaten = true;
                    continue;
                }
                notes.push_back(&note);
            }
            return notes;
        }

    }

    std::vector<Difference> compare(const utau::UstFile &before, const utau::UstFile &after,
                                    const Normalizer &beforeText, const Normalizer &afterText) {
        std::vector<Difference> out;
        Recorder r(out);

        // version.charset is not compared. UST can declare only UTF-8, so that line does not
        // record the encoding of a file written by this program, and the encoding of each side
        // was determined before reading.
        r.enter({});
        r.check("format version", before.version.version, after.version.version);

        const auto &first = before.settings;
        const auto &second = after.settings;
        const auto setting = [&](const std::string &field, const std::string &a,
                                 const std::string &b) {
            r.check(field, beforeText(a), afterText(b));
        };

        r.enter("settings.");
        r.check("tempo", first.tempo, second.tempo);
        r.check("mode2", first.isMode2, second.isMode2);
        setting("projectName", first.projectName, second.projectName);
        setting("flags", first.flags, second.flags);
        setting("outputFileName", first.outputFileName, second.outputFileName);
        setting("project", first.project, second.project);
        setting("voiceDir", first.voiceDir, second.voiceDir);
        setting("cacheDir", first.cacheDir, second.cacheDir);
        setting("wavtoolPath", first.wavtoolPath, second.wavtoolPath);
        setting("resamplerPath", first.resamplerPath, second.resamplerPath);

        const auto ours = notesOf(before);
        const auto theirs = notesOf(after);

        r.enter({});
        r.check("note count", int(ours.size()), int(theirs.size()));

        const size_t count = std::min(ours.size(), theirs.size());
        for (size_t i = 0; i < count; ++i) {
            r.enter("note " + std::to_string(i + 1) + " ");
            compareNote(r, *ours[i], *theirs[i], beforeText, afterText);
        }
        return out;
    }

}
