#include "Project.h"

#include <fstream>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <stdutau/ustfile.h>

#include <hellokit/Support/TextCodec.h>

#include <hellokit/Document/DocumentConstants.h>
#include "PayloadCodec.h"

namespace hello::kit {

    namespace {

        constexpr char KeyFormat[] = "$format";
        constexpr char KeyVersion[] = "version";
        constexpr char KeySettings[] = "settings";
        constexpr char KeyTracks[] = "tracks";

        constexpr char FormatName[] = "usth";

        QString tr(const char *text) {
            return QCoreApplication::translate("hello::kit::Project", text);
        }

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message,
                      std::optional<int> noteIndex = std::nullopt) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, noteIndex});
        }

        // Absent and null both mean the file did not say, which is what the format states and is
        // not the same as a value of zero. A field that is present but of the wrong type is a
        // different matter: it says something that cannot be read, so it is reported rather than
        // passed over as if it had been left out.
        std::optional<double> readOptionalDouble(const QJsonObject &object, const char *key,
                                                 DiagnosticList &diagnostics,
                                                 std::optional<int> noteIndex) {
            const auto value = object.value(QLatin1String(key));
            if (value.isUndefined() || value.isNull()) {
                return std::nullopt;
            }
            if (!value.isDouble()) {
                complain(diagnostics,
                         tr("\"%1\" is not a number and was left out.").arg(QLatin1String(key)),
                         noteIndex);
                return std::nullopt;
            }
            return value.toDouble();
        }

        QString readString(const QJsonObject &object, const char *key) {
            return object.value(QLatin1String(key)).toString();
        }

        void writeOptionalDouble(QJsonObject &object, const char *key,
                                 const std::optional<double> &value) {
            // Left out rather than written as null. The format says the two are the same thing,
            // and leaving it out is the one that cannot be mistaken for a value.
            if (value) {
                object.insert(QLatin1String(key), *value);
            }
        }

        const char *portamentoTypeName(PortamentoType type) {
            switch (type) {
                case PortamentoType::Linear:
                    return "Linear";
                case PortamentoType::R:
                    return "R";
                case PortamentoType::J:
                    return "J";
                case PortamentoType::S:
                    break;
            }
            return "S";
        }

        std::optional<PortamentoType> portamentoTypeFromName(const QString &name) {
            if (name == QLatin1String("S")) {
                return PortamentoType::S;
            }
            if (name == QLatin1String("Linear")) {
                return PortamentoType::Linear;
            }
            if (name == QLatin1String("R")) {
                return PortamentoType::R;
            }
            if (name == QLatin1String("J")) {
                return PortamentoType::J;
            }
            return std::nullopt;
        }

        QJsonObject envelopeToJson(const Envelope &envelope) {
            QJsonArray anchors;
            for (const auto &anchor : envelope.anchors) {
                anchors.append(QJsonObject{{QLatin1String("x"), anchor.x},
                                           {QLatin1String("y"), anchor.y}});
            }
            return QJsonObject{{QLatin1String("anchors"), anchors}};
        }

        Envelope envelopeFromJson(const QJsonObject &object) {
            Envelope envelope;
            for (const auto anchor : object.value(QLatin1String("anchors")).toArray()) {
                const auto fields = anchor.toObject();
                envelope.anchors.push_back({fields.value(QLatin1String("x")).toDouble(),
                                            fields.value(QLatin1String("y")).toDouble()});
            }
            return envelope;
        }

        QJsonObject vibratoToJson(const Vibrato &vibrato) {
            return QJsonObject{
                {QLatin1String("length"), vibrato.length},
                {QLatin1String("period"), vibrato.period},
                {QLatin1String("amplitude"), vibrato.amplitude},
                {QLatin1String("attack"), vibrato.attack},
                {QLatin1String("release"), vibrato.release},
                {QLatin1String("phase"), vibrato.phase},
                {QLatin1String("offset"), vibrato.offset},
                {QLatin1String("intensity"), vibrato.intensity},
            };
        }

        Vibrato vibratoFromJson(const QJsonObject &object) {
            Vibrato vibrato;
            vibrato.length = object.value(QLatin1String("length")).toDouble();
            vibrato.period = object.value(QLatin1String("period")).toDouble();
            vibrato.amplitude = object.value(QLatin1String("amplitude")).toDouble();
            vibrato.attack = object.value(QLatin1String("attack")).toDouble();
            vibrato.release = object.value(QLatin1String("release")).toDouble();
            vibrato.phase = object.value(QLatin1String("phase")).toDouble();
            vibrato.offset = object.value(QLatin1String("offset")).toDouble();
            vibrato.intensity = object.value(QLatin1String("intensity")).toDouble();
            return vibrato;
        }

        QJsonObject pitchBendToJson(const PitchBend &bend) {
            QJsonArray values;
            for (const double value : bend.values) {
                values.append(value);
            }
            QJsonObject object{{QLatin1String("values"), values}};
            if (bend.start) {
                object.insert(QLatin1String("start"), *bend.start);
            }
            return object;
        }

        PitchBend pitchBendFromJson(const QJsonObject &object) {
            PitchBend bend;
            const auto start = object.value(QLatin1String("start"));
            if (start.isDouble()) {
                bend.start = start.toDouble();
            }
            for (const auto value : object.value(QLatin1String("values")).toArray()) {
                bend.values.push_back(value.toDouble());
            }
            return bend;
        }

        QJsonObject noteToJson(const Note &note) {
            QJsonObject object{
                {QLatin1String("lyric"), note.lyric},
                {QLatin1String("length"), note.length},
                {QLatin1String("noteNum"), note.noteNum},
            };

            writeOptionalDouble(object, "intensity", note.intensity);
            writeOptionalDouble(object, "modulation", note.modulation);
            writeOptionalDouble(object, "velocity", note.velocity);
            writeOptionalDouble(object, "preUtterance", note.preUtterance);
            writeOptionalDouble(object, "voiceOverlap", note.voiceOverlap);
            writeOptionalDouble(object, "startPoint", note.startPoint);
            writeOptionalDouble(object, "tempo", note.tempo);

            if (!note.flags.isEmpty()) {
                object.insert(QLatin1String("flags"), note.flags);
            }
            if (note.envelope) {
                object.insert(QLatin1String("envelope"), envelopeToJson(*note.envelope));
            }
            if (note.vibrato) {
                object.insert(QLatin1String("vibrato"), vibratoToJson(*note.vibrato));
            }
            if (!note.portamento.isEmpty()) {
                QJsonArray points;
                for (const auto &point : note.portamento) {
                    points.append(QJsonObject{
                        {QLatin1String("x"), point.x},
                        {QLatin1String("y"), point.y},
                        {QLatin1String("type"), QLatin1String(portamentoTypeName(point.type))},
                    });
                }
                object.insert(QLatin1String("portamento"), points);
            }
            if (note.pitchBend) {
                object.insert(QLatin1String("pitchBend"), pitchBendToJson(*note.pitchBend));
            }

            for (const auto &[key, value] : {std::pair{"label", note.label},
                                             std::pair{"direct", note.direct},
                                             std::pair{"patch", note.patch},
                                             std::pair{"region", note.region},
                                             std::pair{"regionEnd", note.regionEnd}}) {
                if (!value.isEmpty()) {
                    object.insert(QLatin1String(key), value);
                }
            }

            if (!note.userData.isEmpty()) {
                QJsonObject userData;
                for (auto it = note.userData.begin(); it != note.userData.end(); ++it) {
                    userData.insert(it.key(), it.value());
                }
                object.insert(QLatin1String("userData"), userData);
            }

            return object;
        }

        std::optional<Note> noteFromJson(const QJsonObject &object, int index,
                                         DiagnosticList &diagnostics) {
            const auto lyric = object.value(QLatin1String("lyric"));
            const auto length = object.value(QLatin1String("length"));
            const auto noteNum = object.value(QLatin1String("noteNum"));
            if (!lyric.isString() || !length.isDouble() || !noteNum.isDouble()) {
                fail(diagnostics,
                     tr("Note %1 is missing its lyric, length or pitch.").arg(index + 1));
                return std::nullopt;
            }

            Note note;
            note.lyric = lyric.toString();
            note.length = int(length.toDouble());
            note.noteNum = int(noteNum.toDouble());

            note.intensity = readOptionalDouble(object, "intensity", diagnostics, index);
            note.modulation = readOptionalDouble(object, "modulation", diagnostics, index);
            note.velocity = readOptionalDouble(object, "velocity", diagnostics, index);
            note.preUtterance = readOptionalDouble(object, "preUtterance", diagnostics, index);
            note.voiceOverlap = readOptionalDouble(object, "voiceOverlap", diagnostics, index);
            note.startPoint = readOptionalDouble(object, "startPoint", diagnostics, index);
            note.tempo = readOptionalDouble(object, "tempo", diagnostics, index);

            note.flags = readString(object, "flags");

            if (const auto envelope = object.value(QLatin1String("envelope")); envelope.isObject()) {
                note.envelope = envelopeFromJson(envelope.toObject());
            }
            if (const auto vibrato = object.value(QLatin1String("vibrato")); vibrato.isObject()) {
                note.vibrato = vibratoFromJson(vibrato.toObject());
            }
            for (const auto point : object.value(QLatin1String("portamento")).toArray()) {
                const auto fields = point.toObject();
                const auto type =
                    portamentoTypeFromName(fields.value(QLatin1String("type")).toString());
                if (!type) {
                    complain(diagnostics,
                             tr("A portamento point of note %1 has an unknown join and was read "
                                "as a smooth one.")
                                 .arg(index + 1),
                             index);
                }
                note.portamento.push_back({fields.value(QLatin1String("x")).toDouble(),
                                           fields.value(QLatin1String("y")).toDouble(),
                                           type.value_or(PortamentoType::S)});
            }
            if (const auto bend = object.value(QLatin1String("pitchBend")); bend.isObject()) {
                note.pitchBend = pitchBendFromJson(bend.toObject());
            }

            note.label = readString(object, "label");
            note.direct = readString(object, "direct");
            note.patch = readString(object, "patch");
            note.region = readString(object, "region");
            note.regionEnd = readString(object, "regionEnd");

            const auto userData = object.value(QLatin1String("userData")).toObject();
            for (auto it = userData.begin(); it != userData.end(); ++it) {
                note.userData.insert(it.key(), it.value().toString());
            }

            return note;
        }

        QJsonObject settingsToJson(const ProjectSettings &settings) {
            return QJsonObject{
                {QLatin1String("name"), settings.name},
                {QLatin1String("tempo"), settings.tempo},
                {QLatin1String("flags"), settings.flags},
                {QLatin1String("outputFile"), settings.outputFile},
                {QLatin1String("cacheDir"), settings.cacheDir},
                {QLatin1String("wavtool"), settings.wavtool},
                {QLatin1String("resampler"), settings.resampler},
                {QLatin1String("mode2"), settings.mode2},
            };
        }

        ProjectSettings settingsFromJson(const QJsonObject &object, DiagnosticList &diagnostics) {
            ProjectSettings settings;
            settings.name = readString(object, "name");
            settings.tempo =
                readOptionalDouble(object, "tempo", diagnostics, std::nullopt)
                    .value_or(utau::DEFAULT_VALUE_TEMPO);
            settings.flags = readString(object, "flags");
            settings.outputFile = readString(object, "outputFile");
            settings.cacheDir = readString(object, "cacheDir");

            // Kept as they were found. Running them is a separate decision and a guarded one,
            // but dropping them here would delete a setting the user made on purpose. See the
            // security section of AGENTS.md.
            settings.wavtool = readString(object, "wavtool");
            settings.resampler = readString(object, "resampler");

            const auto mode2 = object.value(QLatin1String("mode2"));
            settings.mode2 = mode2.isBool() ? mode2.toBool() : true;
            return settings;
        }

        QByteArray bytesOf(const std::string &s) {
            return QByteArray(s.data(), qsizetype(s.size()));
        }

        std::string stdOf(const QByteArray &b) {
            return std::string(b.constData(), size_t(b.size()));
        }

        bool isControlNote(const utau::Note &note) {
            return note.lyric == controlNoteLyric &&
                   note.userData.count(controlNoteEntry) != 0;
        }

        /// What the control note carries, as it was found.
        ///
        /// Unknown fields are kept so that a file a newer build wrote keeps them on the way back
        /// out, which is the same promise the \c .usth top level makes.
        QJsonObject payloadOf(const utau::Note &note) {
            const auto it = note.userData.find(controlNoteEntry);
            if (it == note.userData.end()) {
                return {};
            }
            const auto decoded = PayloadCodec::decode(bytesOf(it->second));
            if (!decoded) {
                return {};
            }
            return QJsonDocument::fromJson(*decoded).object();
        }

        // Reading a note back. Every string arrives as bytes and leaves as UTF-8, and nothing
        // else in this file touches a std::string.
        class Reader {
        public:
            Reader(const TextCodec &codec, bool unescaping)
                : _codec(codec), _unescaping(unescaping) {
            }

            QString text(const std::string &bytes, bool *ok = nullptr) const {
                const auto decoded = _codec.decode(bytesOf(bytes));
                if (ok) {
                    *ok = decoded.has_value();
                }
                if (!decoded) {
                    return {};
                }
                return _unescaping ? TextCodec::unescape(*decoded) : *decoded;
            }

        private:
            const TextCodec &_codec;
            bool _unescaping;
        };

        PortamentoType joinOf(utau::Point::Type type) {
            switch (type) {
                case utau::Point::LinearJoin:
                    return PortamentoType::Linear;
                case utau::Point::RJoin:
                    return PortamentoType::R;
                case utau::Point::JJoin:
                    return PortamentoType::J;
                case utau::Point::SJoin:
                    break;
            }
            return PortamentoType::S;
        }

        utau::Point::Type joinOf(PortamentoType type) {
            switch (type) {
                case PortamentoType::Linear:
                    return utau::Point::LinearJoin;
                case PortamentoType::R:
                    return utau::Point::RJoin;
                case PortamentoType::J:
                    return utau::Point::JJoin;
                case PortamentoType::S:
                    break;
            }
            return utau::Point::SJoin;
        }

        Note noteFrom(const utau::Note &from, const Reader &reader) {
            Note note;
            note.lyric = reader.text(from.lyric);
            note.length = from.length;
            note.noteNum = from.noteNum;

            note.intensity = from.intensity;
            note.modulation = from.modulation;
            note.velocity = from.velocity;
            note.preUtterance = from.preUttr;
            note.voiceOverlap = from.overlap;
            note.startPoint = from.stp;
            note.tempo = from.tempo;

            note.flags = reader.text(from.flags);
            note.label = reader.text(from.label);
            note.direct = reader.text(from.direct);
            note.patch = reader.text(from.patch);
            note.region = reader.text(from.region);
            note.regionEnd = reader.text(from.regionEnd);

            if (from.envelope) {
                Envelope envelope;
                for (int i = 0; i < from.envelope->count(); ++i) {
                    const auto &anchor = from.envelope->anchors.at(size_t(i));
                    envelope.anchors.push_back({anchor.x, anchor.y});
                }
                note.envelope = envelope;
            }

            if (from.vibrato) {
                Vibrato vibrato;
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
                note.portamento.push_back({point.x, point.y, joinOf(point.type)});
            }

            if (from.pbstart || !from.pitches.empty()) {
                PitchBend bend;
                bend.start = from.pbstart;
                for (const double value : from.pitches) {
                    bend.values.push_back(value);
                }
                note.pitchBend = bend;
            }

            for (const auto &[key, value] : from.userData) {
                note.userData.insert(reader.text(key), reader.text(value));
            }
            return note;
        }

        utau::Note noteTo(const Note &from, const TextCodec &codec, bool escaping) {
            const auto out = [&](const QString &text) {
                return stdOf(codec.encode(escaping ? codec.escape(text) : text));
            };

            utau::Note note;
            note.lyric = out(from.lyric);
            note.length = from.length;
            note.noteNum = from.noteNum;

            note.intensity = from.intensity;
            note.modulation = from.modulation;
            note.velocity = from.velocity;
            note.preUttr = from.preUtterance;
            note.overlap = from.voiceOverlap;
            note.stp = from.startPoint;
            note.tempo = from.tempo;

            note.flags = out(from.flags);
            note.label = out(from.label);
            note.direct = out(from.direct);
            note.patch = out(from.patch);
            note.region = out(from.region);
            note.regionEnd = out(from.regionEnd);

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
                note.portamento.emplace_back(point.x, point.y, joinOf(point.type));
            }

            if (from.pitchBend) {
                note.pbstart = from.pitchBend->start;
                for (const double value : from.pitchBend->values) {
                    note.pitches.push_back(value);
                }
            }

            for (auto it = from.userData.begin(); it != from.userData.end(); ++it) {
                note.userData[out(it.key())] = out(it.value());
            }
            return note;
        }

    }

    std::optional<Project> Project::parse(QByteArrayView json, DiagnosticList &diagnostics) {
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(json.toByteArray(), &error);
        if (error.error != QJsonParseError::NoError) {
            fail(diagnostics, tr("This file is not valid JSON: %1").arg(error.errorString()));
            return std::nullopt;
        }
        if (!document.isObject()) {
            fail(diagnostics, tr("This file is not a HelloUTAU project."));
            return std::nullopt;
        }

        const auto root = document.object();

        // Checked before anything else, since a file that is not one of ours may still parse as
        // JSON and would otherwise be read field by field into a project full of defaults.
        if (root.value(QLatin1String(KeyFormat)).toString() != QLatin1String(FormatName)) {
            fail(diagnostics, tr("This file is not a HelloUTAU project."));
            return std::nullopt;
        }

        const auto version = root.value(QLatin1String(KeyVersion));
        if (!version.isDouble()) {
            fail(diagnostics, tr("This project does not say which format version it is."));
            return std::nullopt;
        }
        if (int(version.toDouble()) > usthFormatVersion) {
            fail(diagnostics,
                 tr("This project was saved by a newer version of HelloUTAU and cannot be "
                    "opened here."));
            return std::nullopt;
        }

        const auto tracks = root.value(QLatin1String(KeyTracks));
        if (!tracks.isArray()) {
            fail(diagnostics, tr("This project has no tracks."));
            return std::nullopt;
        }

        // One track, and anything else is refused rather than trimmed. The array is here so that
        // several become possible later, and a build that cannot hold them has to say so instead
        // of opening the file with the rest of the music missing.
        const auto trackArray = tracks.toArray();
        if (trackArray.size() != 1) {
            fail(diagnostics,
                 tr("This project holds %1 tracks, and this version of HelloUTAU handles one.")
                     .arg(trackArray.size()));
            return std::nullopt;
        }

        Project project;
        project.settings =
            settingsFromJson(root.value(QLatin1String(KeySettings)).toObject(), diagnostics);

        const auto trackObject = trackArray.first().toObject();
        Track track;
        track.name = readString(trackObject, "name");
        track.voiceDir = readString(trackObject, "voiceDir");

        const auto notes = trackObject.value(QLatin1String("notes")).toArray();
        track.notes.reserve(notes.size());
        for (int i = 0; i < notes.size(); ++i) {
            auto note = noteFromJson(notes.at(i).toObject(), i, diagnostics);
            if (!note) {
                return std::nullopt;
            }
            track.notes.push_back(*note);
        }
        project.tracks.push_back(track);

        for (auto it = root.begin(); it != root.end(); ++it) {
            const auto key = it.key();
            if (key != QLatin1String(KeyFormat) && key != QLatin1String(KeyVersion) &&
                key != QLatin1String(KeySettings) && key != QLatin1String(KeyTracks)) {
                project.unknownFields.insert(key, it.value());
            }
        }

        return project;
    }

    std::optional<Project> Project::read(const std::filesystem::path &path,
                                          DiagnosticList &diagnostics) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            fail(diagnostics, tr("This file could not be opened."));
            return std::nullopt;
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return parse(QByteArrayView(bytes.data(), qsizetype(bytes.size())), diagnostics);
    }

    QByteArray Project::serialize() const {
        // Starts from what was not understood when the file was read, so that those fields come
        // back. Ours are inserted over the top, so a stale copy of one cannot win.
        QJsonObject root = unknownFields;
        root.insert(QLatin1String(KeyFormat), QLatin1String(FormatName));
        root.insert(QLatin1String(KeyVersion), usthFormatVersion);
        root.insert(QLatin1String(KeySettings), settingsToJson(settings));

        QJsonArray trackArray;
        for (const auto &track : tracks) {
            QJsonArray notes;
            for (const auto &note : track.notes) {
                notes.append(noteToJson(note));
            }
            trackArray.append(QJsonObject{
                {QLatin1String("name"), track.name},
                {QLatin1String("voiceDir"), track.voiceDir},
                {QLatin1String("notes"), notes},
            });
        }
        root.insert(QLatin1String(KeyTracks), trackArray);

        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    bool Project::write(const std::filesystem::path &path,
                        DiagnosticList &diagnostics) const {
        if (tracks.size() != 1) {
            fail(diagnostics,
                 tr("A project of this version holds one track, and this one holds %1.")
                     .arg(tracks.size()));
            return false;
        }

        const auto bytes = serialize();

        // Binary, so that nothing turns the newlines into CRLF on Windows. The format says the
        // file is written with newlines, and a project file is something people diff.
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        out.write(bytes.constData(), bytes.size());
        if (!out) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        return true;
    }

    std::optional<QString> UstProbe::settledCharset() const {
        if (recordedCharset) {
            return recordedCharset;
        }
        if (declaresUtf8) {
            return QStringLiteral("UTF-8");
        }
        return std::nullopt;
    }

    std::optional<UstProbe> Project::probeUst(const std::filesystem::path &path,
                                              DiagnosticList &diagnostics) {
        utau::UstFile file;
        if (!file.load(path)) {
            fail(diagnostics, tr("This file could not be read."));
            return std::nullopt;
        }

        UstProbe probe;
        probe.declaresUtf8 = QByteArray(file.version.charset.data(),
                                        qsizetype(file.version.charset.size()))
                                 .compare("UTF-8", Qt::CaseInsensitive) == 0;
        probe.rawProjectName = bytesOf(file.settings.projectName);
        probe.rawVoiceDir = bytesOf(file.settings.voiceDir);

        for (const auto &note : file.notes) {
            if (isControlNote(note)) {
                const auto payload = payloadOf(note);
                const auto charset = payload.value(QLatin1String("ustCharset")).toString();
                if (!charset.isEmpty()) {
                    probe.recordedCharset = charset;
                }
                continue;
            }
            if (!note.lyric.empty()) {
                probe.rawLyrics.push_back(bytesOf(note.lyric));
            }
        }
        return probe;
    }

    std::optional<Project> Project::fromUst(const std::filesystem::path &path,
                                            const QString &charset,
                                            DiagnosticList &diagnostics) {
        const TextCodec codec(charset);
        if (!codec.isValid()) {
            fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(charset));
            return std::nullopt;
        }

        utau::UstFile file;
        if (!file.load(path)) {
            fail(diagnostics, tr("This file could not be read."));
            return std::nullopt;
        }

        // Escaping is only ever applied to files this program wrote, and only where the encoding
        // could not hold everything. A UST from UTAU knows nothing about it, and unescaping one
        // would eat its backslashes. The control note is what says which kind of file this is.
        bool ours = false;
        for (const auto &note : file.notes) {
            ours = ours || isControlNote(note);
        }
        const Reader reader(codec, ours && !codec.isUtf8());

        Project project;
        bool ok = true;
        project.settings.name = reader.text(file.settings.projectName, &ok);
        if (!ok) {
            fail(diagnostics,
                 tr("This file is not in the %1 encoding.").arg(codec.name()));
            return std::nullopt;
        }
        project.settings.tempo = file.settings.tempo;
        project.settings.flags = reader.text(file.settings.flags);
        project.settings.outputFile = reader.text(file.settings.outputFileName);
        project.settings.cacheDir = reader.text(file.settings.cacheDir);
        project.settings.wavtool = reader.text(file.settings.wavtoolPath);
        project.settings.resampler = reader.text(file.settings.resamplerPath);
        project.settings.mode2 = file.settings.isMode2;

        Track track;
        track.voiceDir = reader.text(file.settings.voiceDir);

        // Exactly one control note is taken out. Leaving it in would make it a note of the
        // project, and writing the project back out would add a second one, so a file going
        // round a few times would grow a run of half second leaders.
        bool eaten = false;
        for (const auto &note : file.notes) {
            if (!eaten && isControlNote(note)) {
                eaten = true;
                continue;
            }
            track.notes.push_back(noteFrom(note, reader));
        }
        project.tracks.push_back(track);
        return project;
    }

    bool Project::toUst(const std::filesystem::path &path, const UstExportOptions &options,
                        DiagnosticList &diagnostics) const {
        if (tracks.size() != 1) {
            fail(diagnostics, tr("A UST holds one track, and this project holds %1.")
                                  .arg(tracks.size()));
            return false;
        }

        const TextCodec codec(options.charset);
        if (!codec.isValid()) {
            fail(diagnostics,
                 tr("The encoding \"%1\" is not available.").arg(options.charset));
            return false;
        }
        const bool escaping = !codec.isUtf8();
        const auto out = [&](const QString &text) {
            return stdOf(codec.encode(escaping ? codec.escape(text) : text));
        };

        utau::UstFile file;

        // UST can say "this is UTF-8" and nothing else, so anything else goes unsaid here and is
        // carried by the control note instead.
        file.version.version = "1.2";
        if (codec.isUtf8()) {
            file.version.charset = "UTF-8";
        }

        file.settings.tempo = settings.tempo;
        file.settings.projectName = out(settings.name);
        file.settings.flags = out(settings.flags);
        file.settings.outputFileName = out(settings.outputFile);
        file.settings.cacheDir = out(settings.cacheDir);
        file.settings.voiceDir = out(tracks.first().voiceDir);
        file.settings.isMode2 = settings.mode2;

        // The project's own engines are written as they were found. Where it names none, the
        // local ones go in instead, because UTAU opening a UST with no engine has nothing to
        // render with.
        const QString wavtool = settings.wavtool.isEmpty() ? options.wavtool : settings.wavtool;
        const QString resampler =
            settings.resampler.isEmpty() ? options.resampler : settings.resampler;
        file.settings.wavtoolPath = out(wavtool);
        file.settings.resamplerPath = out(resampler);
        if (wavtool.isEmpty() || resampler.isEmpty()) {
            complain(diagnostics,
                 tr("This UST names no rendering engine, so UTAU will not be able to render it "
                    "until one is set there."));
        }

        QJsonObject payload;
        payload.insert(QLatin1String("version"), controlNotePayloadVersion);
        payload.insert(QLatin1String("ustCharset"), codec.name());

        utau::Note control;
        control.lyric = controlNoteLyric;
        control.length = controlNoteLength;
        control.noteNum = controlNoteNoteNum;
        control.userData[controlNoteEntry] =
            stdOf(PayloadCodec::encode(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
        file.notes.push_back(control);

        for (const auto &note : tracks.first().notes) {
            file.notes.push_back(noteTo(note, codec, escaping));
        }

        if (!file.save(path)) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        return true;
    }

}
