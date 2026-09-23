#include "Project.h"

#include <fstream>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include "DocumentConstants.h"
#include "PayloadCodec.h"

namespace hello::kit {

    namespace {

        constexpr char KeyFormat[] = "$format";
        constexpr char KeyVersion[] = "version";
        constexpr char KeySettings[] = "settings";
        constexpr char KeyTracks[] = "tracks";

        constexpr char FormatName[] = "usth";

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message,
                      std::optional<int> noteIndex = std::nullopt) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, noteIndex});
        }

        // An absent field and a null field both mean unspecified, as the format defines, which
        // differs from a value of zero. A present field of the wrong type is different: it holds
        // an unreadable value, so it is reported rather than treated as absent.
        std::optional<double> readOptionalDouble(const QJsonObject &object, const char *key,
                                                 DiagnosticList &diagnostics,
                                                 std::optional<int> noteIndex) {
            const auto value = object.value(QLatin1String(key));
            if (value.isUndefined() || value.isNull()) {
                return std::nullopt;
            }
            if (!value.isDouble()) {
                complain(
                    diagnostics,
                    Project::tr("\"%1\" is not a number and was omitted.").arg(QLatin1String(key)),
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
            // Omitted rather than written as null. The format defines both as equivalent, and
            // omission cannot be mistaken for a value.
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
                anchors.append(QJsonObject{
                    {QLatin1String("x"), anchor.x},
                    {QLatin1String("y"), anchor.y}
                });
            }
            return QJsonObject{
                {QLatin1String("anchors"), anchors}
            };
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
                {QLatin1String("length"),    vibrato.length   },
                {QLatin1String("period"),    vibrato.period   },
                {QLatin1String("amplitude"), vibrato.amplitude},
                {QLatin1String("attack"),    vibrato.attack   },
                {QLatin1String("release"),   vibrato.release  },
                {QLatin1String("phase"),     vibrato.phase    },
                {QLatin1String("offset"),    vibrato.offset   },
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
            QJsonObject object{
                {QLatin1String("values"), values}
            };
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
                {QLatin1String("lyric"),   note.lyric  },
                {QLatin1String("length"),  note.length },
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
                        {QLatin1String("x"),    point.x                                      },
                        {QLatin1String("y"),    point.y                                      },
                        {QLatin1String("type"), QLatin1String(portamentoTypeName(point.type))},
                    });
                }
                object.insert(QLatin1String("portamento"), points);
            }
            if (note.pitchBend) {
                object.insert(QLatin1String("pitchBend"), pitchBendToJson(*note.pitchBend));
            }

            // By reference. Pairing each field with its name by value would copy five strings
            // per note unnecessarily.
            const std::pair<const char *, const QString &> texts[] = {
                {"label",     note.label    },
                {"direct",    note.direct   },
                {"patch",     note.patch    },
                {"region",    note.region   },
                {"regionEnd", note.regionEnd},
            };
            for (const auto &[key, value] : texts) {
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
                     Project::tr("Note %1 is missing its lyric, length or pitch.").arg(index + 1));
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

            if (const auto envelope = object.value(QLatin1String("envelope"));
                envelope.isObject()) {
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
                             Project::tr("A portamento point of note %1 has an unknown curve type "
                                         "and was read as the default type.")
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
                {QLatin1String("name"),       settings.name      },
                {QLatin1String("tempo"),      settings.tempo     },
                {QLatin1String("flags"),      settings.flags     },
                {QLatin1String("outputFile"), settings.outputFile},
                {QLatin1String("cacheDir"),   settings.cacheDir  },
                {QLatin1String("wavtool"),    settings.wavtool   },
                {QLatin1String("resampler"),  settings.resampler },
                {QLatin1String("mode2"),      settings.mode2     },
            };
        }

        ProjectSettings settingsFromJson(const QJsonObject &object, DiagnosticList &diagnostics) {
            ProjectSettings settings;
            settings.name = readString(object, "name");
            settings.tempo = readOptionalDouble(object, "tempo", diagnostics, std::nullopt)
                                 .value_or(utau::DEFAULT_VALUE_TEMPO);
            settings.flags = readString(object, "flags");
            settings.outputFile = readString(object, "outputFile");
            settings.cacheDir = readString(object, "cacheDir");

            // Stored verbatim. Executing them is a separate, guarded decision, whereas discarding
            // them here would delete a deliberate user setting. See the security section of
            // AGENTS.md.
            settings.wavtool = readString(object, "wavtool");
            settings.resampler = readString(object, "resampler");

            const auto mode2 = object.value(QLatin1String("mode2"));
            settings.mode2 = mode2.isBool() ? mode2.toBool() : true;
            return settings;
        }

    }

    std::optional<Project> Project::open(const std::filesystem::path &path,
                                         DiagnosticList &diagnostics) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            fail(diagnostics, tr("This file could not be opened."));
            return std::nullopt;
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return fromJson(QByteArrayView(bytes.data(), qsizetype(bytes.size())), diagnostics);
    }

    bool Project::save(const std::filesystem::path &path, DiagnosticList &diagnostics) const {
        if (tracks.size() != 1) {
            fail(diagnostics, tr("This version of HelloUTAU supports one track per project, but "
                                 "this project contains %1.")
                                  .arg(tracks.size()));
            return false;
        }

        const auto bytes = toJson();

        // Written in binary mode, so that line feeds are not converted to CRLF on Windows. The
        // format specifies LF line endings, and project files are compared with diff tools.
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


    std::optional<Project> Project::fromJson(QByteArrayView json, DiagnosticList &diagnostics) {
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

        // Checked first, because a foreign file may still be valid JSON and would otherwise be
        // read field by field into a project consisting of defaults.
        if (root.value(QLatin1String(KeyFormat)).toString() != QLatin1String(FormatName)) {
            fail(diagnostics, tr("This file is not a HelloUTAU project."));
            return std::nullopt;
        }

        const auto version = root.value(QLatin1String(KeyVersion));
        if (!version.isDouble()) {
            fail(diagnostics, tr("This project does not specify its format version."));
            return std::nullopt;
        }
        if (int(version.toDouble()) > usthFormatVersion) {
            fail(diagnostics, tr("This project was saved by a newer version of HelloUTAU and "
                                 "cannot be opened by this version."));
            return std::nullopt;
        }

        const auto tracks = root.value(QLatin1String(KeyTracks));
        if (!tracks.isArray()) {
            fail(diagnostics, tr("This project contains no tracks."));
            return std::nullopt;
        }

        // Exactly one track. Any other count is rejected rather than truncated. The array exists
        // to allow multiple tracks later, and a build that cannot represent them must report
        // this instead of opening the file with part of the music missing.
        const auto trackArray = tracks.toArray();
        if (trackArray.size() != 1) {
            fail(diagnostics, tr("This project contains %1 tracks, but this version of HelloUTAU "
                                 "supports only one.")
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

    QByteArray Project::toJson() const {
        // Starts from the fields not recognized when the file was read, so that they are
        // preserved. The known fields are inserted afterward, so that a stale copy of a known
        // field cannot take precedence.
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
                {QLatin1String("name"),     track.name    },
                {QLatin1String("voiceDir"), track.voiceDir},
                {QLatin1String("notes"),    notes         },
            });
        }
        root.insert(QLatin1String(KeyTracks), trackArray);

        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

}
