#include "MidiConvert.h"

#include <algorithm>
#include <map>

#include <QtCore/QCoreApplication>

#include <wolf-midi/MidiFile.h>

#include <hellokit/Support/TextCodec.h>

#include <hellokit/Document/DocumentConstants.h>

namespace hello::kit {

    namespace {

        constexpr char OptionEncoding[] = "encoding";
        constexpr char OptionDefaultLyric[] = "defaultLyric";

        void say(DiagnosticList &diagnostics, DiagnosticSeverity severity, const QString &message,
                 std::optional<int> noteIndex = std::nullopt) {
            diagnostics.push_back({severity, message, noteIndex});
        }

        QByteArray toByteArray(const std::vector<char> &data) {
            return QByteArray(data.data(), qsizetype(data.size()));
        }

        std::vector<char> toVector(const QByteArray &data) {
            return std::vector<char>(data.constData(), data.constData() + data.size());
        }

        /// One note as MIDI had it, before anything is made to fit a single voice.
        struct RawNote {
            int start = 0;
            int end = 0;
            int pitch = 0;
        };

        /// Pairs note on with note off by voice and pitch rather than by position.
        ///
        /// Pairing them by position is what the earlier implementation did, and it comes apart on
        /// any file whose notes overlap, since the off events do not then arrive in the order the
        /// on events did.
        std::vector<RawNote> collectNotes(const std::vector<Midi::MidiEvent *> &events,
                                          int trackEnd, DiagnosticList &diagnostics) {
            std::vector<RawNote> notes;

            // Several of the same pitch may be sounding at once on different voices, and even on
            // one voice a file may start the same pitch twice. The last one started is the one an
            // off event ends.
            std::map<std::pair<int, int>, std::vector<int>> open;

            for (const auto event : events) {
                if (!event->isNoteEvent()) {
                    continue;
                }

                const auto key = std::make_pair(event->voice(), event->note());

                // A note on with no velocity is a note off. Written both ways in the wild, and a
                // file using this form would otherwise look like nothing but beginnings.
                const bool isOff =
                    event->type() == Midi::MidiEvent::NoteOff ||
                    (event->type() == Midi::MidiEvent::NoteOn && event->velocity() == 0);

                if (!isOff) {
                    open[key].push_back(event->tick());
                    continue;
                }

                auto it = open.find(key);
                if (it == open.end() || it->second.empty()) {
                    continue; // an off with no on, which says nothing about any note
                }
                const int start = it->second.back();
                it->second.pop_back();
                if (event->tick() > start) {
                    notes.push_back({start, event->tick(), event->note()});
                }
            }

            int unfinished = 0;
            for (const auto &[key, starts] : open) {
                for (const int start : starts) {
                    if (trackEnd > start) {
                        notes.push_back({start, trackEnd, key.second});
                        ++unfinished;
                    }
                }
            }
            if (unfinished > 0) {
                say(diagnostics, DiagnosticSeverity::Warning,
                    MidiReader::tr("%1 notes were never ended and now run to the end of the track.")
                        .arg(unfinished));
            }

            std::sort(notes.begin(), notes.end(), [](const RawNote &a, const RawNote &b) {
                if (a.start != b.start) {
                    return a.start < b.start;
                }
                return a.pitch > b.pitch; // the top of a chord is the one that survives
            });
            return notes;
        }

        struct Lyric {
            int tick = 0;
            QByteArray text;
        };

    }

    MidiReader::MidiReader() = default;

    MidiReader::~MidiReader() = default;

    QString MidiReader::id() const {
        return QStringLiteral("midi");
    }

    QString MidiReader::name() const {
        return MidiReader::tr("Standard MIDI File");
    }

    QStringList MidiReader::suffixes() const {
        return {QStringLiteral("mid"), QStringLiteral("midi")};
    }

    QList<InterchangeOption> MidiReader::optionSchema() const {
        InterchangeOption encoding;
        encoding.key = QLatin1String(OptionEncoding);
        encoding.name = MidiReader::tr("Encoding");
        encoding.type = InterchangeOption::Choice;
        encoding.defaultValue = QStringLiteral("UTF-8");
        encoding.choices = {
            QStringLiteral("UTF-8"),      QStringLiteral("Shift_JIS"), QStringLiteral("GBK"),
            QStringLiteral("Big5"),       QStringLiteral("EUC-KR"),    QStringLiteral("UTF-16"),
            QStringLiteral("ISO 8859-1"),
        };

        // A note has to say something, because a UST note with no lyric is a rest. This is the
        // one place where a value has to be invented rather than translated, so it is the user's
        // to set.
        InterchangeOption lyric;
        lyric.key = QLatin1String(OptionDefaultLyric);
        lyric.name = MidiReader::tr("Lyric for notes that have none");
        lyric.type = InterchangeOption::Text;
        lyric.defaultValue = QLatin1String(defaultLyric);

        return {encoding, lyric};
    }

    QString MidiReader::customStepId() const {
        return QStringLiteral("midi.encoding");
    }

    std::optional<InterchangeSource> MidiReader::inspect(const std::filesystem::path &path,
                                                         DiagnosticList &diagnostics) {
        Midi::MidiFile midi;
        if (!midi.load(path)) {
            say(diagnostics, DiagnosticSeverity::Error, MidiReader::tr("This is not a MIDI file."));
            return std::nullopt;
        }
        if (midi.divisionType() != Midi::MidiFile::PPQ) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiReader::tr(
                    "This MIDI file is timed in SMPTE frames, which cannot be turned into bars "
                    "and beats."));
            return std::nullopt;
        }

        InterchangeSource source;
        source.formatId = id();

        for (const int track : midi.tracks()) {
            const auto events = midi.eventsForTrack(track);

            InterchangeEntry entry;
            entry.index = track;

            for (const auto event : events) {
                if (event->type() == Midi::MidiEvent::NoteOn && event->velocity() > 0) {
                    ++entry.noteCount;
                    const int pitch = event->note();
                    entry.lowestNote = std::min(entry.lowestNote.value_or(pitch), pitch);
                    entry.highestNote = std::max(entry.highestNote.value_or(pitch), pitch);
                } else if (event->type() == Midi::MidiEvent::Meta) {
                    switch (event->number()) {
                        case Midi::MidiEvent::TrackName:
                            entry.rawName = toByteArray(event->data());
                            break;
                        case Midi::MidiEvent::Lyric:
                            entry.rawLyrics.push_back(toByteArray(event->data()));
                            break;
                        case Midi::MidiEvent::Marker:
                            source.rawLabels.push_back(toByteArray(event->data()));
                            break;
                        default:
                            break;
                    }
                }
            }

            source.entries.push_back(entry);
        }

        return source;
    }

    std::optional<Project> MidiReader::convert(const std::filesystem::path &path,
                                               const InterchangeSource &source,
                                               const ImportRequest &request,
                                               DiagnosticList &diagnostics) {
        Q_UNUSED(source)

        if (request.entries.isEmpty()) {
            say(diagnostics, DiagnosticSeverity::Error, MidiReader::tr("No track was chosen."));
            return std::nullopt;
        }

        // Loaded again rather than kept from inspect(). One reader serves every import in the
        // application, so holding a parsed file on it would be state shared between unrelated
        // calls.
        Midi::MidiFile midi;
        if (!midi.load(path)) {
            say(diagnostics, DiagnosticSeverity::Error, MidiReader::tr("This is not a MIDI file."));
            return std::nullopt;
        }
        const int resolution = midi.resolution();
        if (resolution <= 0) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiReader::tr("This MIDI file does not say how long a beat is."));
            return std::nullopt;
        }

        // Absolute ticks are scaled and lengths are taken from the scaled values, not the other
        // way round. Scaling each length on its own lets the rounding accumulate, and a long
        // track then drifts away from the bar lines.
        const auto scale = [resolution](int tick) {
            return int(std::llround(double(tick) * double(ticksPerQuarter) / double(resolution)));
        };

        if (request.entries.size() > 1) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "A project holds one track, so only the first of the chosen tracks was used."));
        }
        const int wantedTrack = request.entries.first();

        const TextCodec codec(
            request.driverOptions.value(QLatin1String(OptionEncoding), QStringLiteral("UTF-8"))
                .toString());
        if (!codec.isValid()) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiReader::tr("That encoding is not available."));
            return std::nullopt;
        }

        // Undecodable bytes mean the wrong encoding was chosen, and the lyric is better left
        // empty and reported than filled with replacement characters.
        int undecodable = 0;
        const auto decode = [&codec, &undecodable](const QByteArray &bytes) {
            const auto text = codec.decode(bytes);
            if (!text) {
                ++undecodable;
                return QString();
            }
            return *text;
        };

        const QString lyricForSilentNotes =
            request.driverOptions
                .value(QLatin1String(OptionDefaultLyric), QLatin1String(defaultLyric))
                .toString();

        // Tempo is gathered from every track, since a format 1 file keeps it in track 0 while the
        // notes are somewhere else.
        std::map<int, double> tempos;
        for (const int track : midi.tracks()) {
            for (const auto event : midi.eventsForTrack(track)) {
                if (event->type() == Midi::MidiEvent::Meta &&
                    event->number() == Midi::MidiEvent::Tempo) {
                    tempos[scale(event->tick())] = double(event->tempo());
                }
            }
        }

        const auto events = midi.eventsForTrack(wantedTrack);

        QString trackName;
        std::vector<Lyric> lyrics;
        for (const auto event : events) {
            if (event->type() != Midi::MidiEvent::Meta) {
                continue;
            }
            if (event->number() == Midi::MidiEvent::TrackName) {
                trackName = decode(toByteArray(event->data()));
            } else if (event->number() == Midi::MidiEvent::Lyric) {
                lyrics.push_back({scale(event->tick()), toByteArray(event->data())});
            }
        }

        auto raw = collectNotes(events, midi.trackEndTick(wantedTrack), diagnostics);
        for (auto &note : raw) {
            note.start = scale(note.start);
            note.end = scale(note.end);
        }

        Track track;
        track.name = trackName;

        int cursor = 0;
        int clamped = 0;
        int chordNotes = 0;
        int shortened = 0;
        int dropped = 0;

        for (const auto &note : raw) {
            if (note.end <= note.start) {
                ++dropped; // nothing left of it once the resolution was taken into account
                continue;
            }

            // Notes that begin together are a chord, and only the top one can be kept. They are
            // sorted so that it comes first.
            if (!track.notes.isEmpty() && note.start == cursor - track.notes.last().length &&
                !track.notes.last().isRest()) {
                ++chordNotes;
                continue;
            }

            if (note.start < cursor) {
                // Still sounding when this one begins. Shortening what is already there keeps
                // both, where the earlier implementation threw this one away.
                auto &previous = track.notes.last();
                const int overlap = cursor - note.start;
                if (previous.length - overlap <= 0) {
                    ++dropped;
                    continue;
                }
                previous.length -= overlap;
                cursor = note.start;
                ++shortened;
            }

            if (note.start > cursor) {
                Note rest;
                rest.lyric = QLatin1String(restLyric);
                rest.length = note.start - cursor;
                rest.noteNum = track.notes.isEmpty() ? 60 : track.notes.last().noteNum;
                track.notes.push_back(rest);
                cursor = note.start;
            }

            int noteNum = note.pitch;
            if (noteNum < lowestNoteNum || noteNum > highestNoteNum) {
                noteNum = std::clamp(noteNum, lowestNoteNum, highestNoteNum);
                ++clamped;
            }

            Note out;
            out.lyric = lyricForSilentNotes;
            out.length = note.end - note.start;
            out.noteNum = noteNum;
            track.notes.push_back(out);
            cursor = note.end;
        }

        if (chordNotes > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "%1 notes began at the same moment as another and were left out, since a "
                    "track holds one voice.")
                    .arg(chordNotes));
        }
        if (shortened > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "%1 notes were shortened where the next one began before they ended.")
                    .arg(shortened));
        }
        if (dropped > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr("%1 notes were too short to keep and were left out.").arg(dropped));
        }
        if (clamped > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "%1 notes lay outside the keyboard and were moved to its nearest end.")
                    .arg(clamped));
        }

        // Lyrics belong to the note that is sounding when they arrive. Matching them by an exact
        // tick, which is what the earlier implementation did, loses every lyric a sequencer
        // placed a tick early.
        int position = 0;
        std::map<int, int> noteAtTick; // start tick to index, sounding notes only
        for (int i = 0; i < track.notes.size(); ++i) {
            if (!track.notes.at(i).isRest()) {
                noteAtTick[position] = i;
            }
            position += track.notes.at(i).length;
        }

        int unplaced = 0;
        for (const auto &lyric : lyrics) {
            auto it = noteAtTick.upper_bound(lyric.tick);
            if (it == noteAtTick.begin()) {
                ++unplaced;
                continue;
            }
            --it;
            auto &note = track.notes[it->second];
            if (lyric.tick >= it->first + note.length) {
                ++unplaced; // it landed in a rest
                continue;
            }
            note.lyric = decode(lyric.text);
        }
        if (unplaced > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr("%1 lyrics did not fall on any note and were left out.")
                    .arg(unplaced));
        }

        Project project;
        project.settings.tempo = tempos.empty() ? 120.0 : tempos.begin()->second;

        // A tempo change can only sit on a note, so one arriving in the middle of a note moves
        // to the next one. Saying nothing would make the track come out at the wrong speed with
        // no sign of why.
        int moved = 0;
        for (auto it = tempos.begin(); it != tempos.end(); ++it) {
            if (it == tempos.begin()) {
                continue;
            }
            auto at = noteAtTick.lower_bound(it->first);
            if (at == noteAtTick.end()) {
                ++moved;
                continue;
            }
            track.notes[at->second].tempo = it->second;
            if (at->first != it->first) {
                ++moved;
            }
        }
        if (moved > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "%1 tempo changes did not fall on a note and were moved to the next one.")
                    .arg(moved));
        }

        if (undecodable > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiReader::tr(
                    "%1 pieces of text are not valid %2 and were left out, which usually means "
                    "the encoding is not the one this file is in.")
                    .arg(undecodable)
                    .arg(codec.name()));
        }

        project.tracks.push_back(track);
        return project;
    }

    MidiWriter::MidiWriter() = default;

    MidiWriter::~MidiWriter() = default;

    QString MidiWriter::id() const {
        return QStringLiteral("midi");
    }

    QString MidiWriter::name() const {
        return MidiWriter::tr("Standard MIDI File");
    }

    QStringList MidiWriter::suffixes() const {
        return {QStringLiteral("mid"), QStringLiteral("midi")};
    }

    QList<InterchangeOption> MidiWriter::optionSchema() const {
        InterchangeOption encoding;
        encoding.key = QLatin1String(OptionEncoding);
        encoding.name = MidiWriter::tr("Encoding");
        encoding.type = InterchangeOption::Choice;
        encoding.defaultValue = QStringLiteral("UTF-8");
        encoding.choices = {
            QStringLiteral("UTF-8"), QStringLiteral("Shift_JIS"), QStringLiteral("GBK"),
            QStringLiteral("Big5"),  QStringLiteral("EUC-KR"),
        };
        return {encoding};
    }

    QString MidiWriter::customStepId() const {
        return QStringLiteral("midi.encoding");
    }

    bool MidiWriter::convert(const Project &project, const std::filesystem::path &path,
                             const ExportRequest &request, DiagnosticList &diagnostics) {
        if (project.tracks.size() != 1) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiWriter::tr("A MIDI file is written from one track, and this project holds %1.")
                    .arg(project.tracks.size()));
            return false;
        }

        const TextCodec codec(
            request.driverOptions.value(QLatin1String(OptionEncoding), QStringLiteral("UTF-8"))
                .toString());
        if (!codec.isValid()) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiWriter::tr("That encoding is not available."));
            return false;
        }

        // Said every time, because it is what the format is rather than something gone wrong.
        // Everything UTAU renders with lives in entries MIDI has no room for.
        say(diagnostics, DiagnosticSeverity::Warning,
            MidiWriter::tr(
                "A MIDI file holds notes and lyrics. The envelope, the vibrato, the pitch curve, "
                "the flags and the per note values were left out."));

        Midi::MidiFile midi;
        midi.setFileFormat(1);
        midi.setDivisionType(Midi::MidiFile::PPQ);

        // The same resolution the project counts in, so nothing has to be scaled and nothing can
        // drift.
        midi.setResolution(ticksPerQuarter);
        const int track = midi.createTrack();

        const auto &source = project.tracks.first();
        int unrepresentable = 0;
        const auto out = [&codec, &unrepresentable](const QString &text) {
            // No escaping here, unlike a UST. Escaping is only ever read back where a control
            // note says the file is ours, and MIDI has nowhere to put one, so an escape written
            // into a lyric would come back as its own literal text.
            if (!text.isEmpty() && !codec.canEncode(text)) {
                ++unrepresentable;
            }
            return toVector(codec.encode(text));
        };

        if (!source.name.isEmpty()) {
            midi.createMetaEvent(track, 0, Midi::MidiEvent::TrackName, out(source.name));
        }
        midi.createTempoEvent(track, 0, float(project.settings.tempo));

        int position = 0;
        int unwritable = 0;
        for (const auto &note : source.notes) {
            if (note.tempo && position > 0) {
                midi.createTempoEvent(track, position, float(*note.tempo));
            }
            if (note.isRest()) {
                position += note.length;
                continue;
            }

            const int pitch = std::clamp(note.noteNum, 0, 127);
            if (pitch != note.noteNum) {
                ++unwritable;
            }
            midi.createNote(track, position, position + note.length, 0, pitch, 100, 64);
            midi.createLyricEvent(track, position, out(note.lyric));
            position += note.length;
        }

        if (unrepresentable > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiWriter::tr(
                    "%1 lyrics have no spelling in %2 and were written as question marks.")
                    .arg(unrepresentable)
                    .arg(codec.name()));
        }
        if (unwritable > 0) {
            say(diagnostics, DiagnosticSeverity::Warning,
                MidiWriter::tr(
                    "%1 notes lay outside what MIDI can name and were moved to its nearest end.")
                    .arg(unwritable));
        }

        if (!midi.save(path)) {
            say(diagnostics, DiagnosticSeverity::Error,
                MidiWriter::tr("This file could not be written."));
            return false;
        }
        return true;
    }

}
