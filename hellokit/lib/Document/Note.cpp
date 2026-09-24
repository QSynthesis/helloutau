#include "Note.h"

#include <utility>

#include <QtCore/QJsonArray>

#include "JsonFields_p.h"

namespace hello::kit {

    QJsonObject Envelope::toJson() const {
        QJsonArray anchors;
        for (const auto &anchor : anchorsInTimeOrder()) {
            anchors.append(QJsonObject{
                {QLatin1String("x"), anchor.x},
                {QLatin1String("y"), anchor.y}
            });
        }
        return QJsonObject{
            {QLatin1String("anchors"), anchors}
        };
    }

    std::optional<Envelope> Envelope::fromJson(const QJsonObject &object) {
        QList<EnvelopeAnchor> anchors;
        for (const auto anchor : object.value(QLatin1String("anchors")).toArray()) {
            const auto fields = anchor.toObject();
            anchors.push_back({fields.value(QLatin1String("x")).toDouble(),
                               fields.value(QLatin1String("y")).toDouble()});
        }
        return fromTimeOrder(anchors);
    }

    QJsonObject Vibrato::toJson() const {
        return QJsonObject{
            {QLatin1String("length"),    length   },
            {QLatin1String("period"),    period   },
            {QLatin1String("amplitude"), amplitude},
            {QLatin1String("attack"),    attack   },
            {QLatin1String("release"),   release  },
            {QLatin1String("phase"),     phase    },
            {QLatin1String("offset"),    offset   },
            {QLatin1String("intensity"), intensity},
        };
    }

    Vibrato Vibrato::fromJson(const QJsonObject &object) {
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

    QString PortamentoPoint::typeName(Type type) {
        switch (type) {
            case Linear:
                return QStringLiteral("Linear");
            case R:
                return QStringLiteral("R");
            case J:
                return QStringLiteral("J");
            case S:
                break;
        }
        return QStringLiteral("S");
    }

    std::optional<PortamentoPoint::Type> PortamentoPoint::typeFromName(QStringView name) {
        for (const auto type : {S, Linear, R, J}) {
            if (name == typeName(type)) {
                return type;
            }
        }
        return std::nullopt;
    }

    QJsonObject PortamentoPoint::toJson() const {
        return QJsonObject{
            {QLatin1String("x"),    x             },
            {QLatin1String("y"),    y             },
            {QLatin1String("type"), typeName(type)},
        };
    }

    PortamentoPoint PortamentoPoint::fromJson(const QJsonObject &object,
                                              DiagnosticList &diagnostics) {
        const auto type = typeFromName(object.value(QLatin1String("type")).toString());
        if (!type) {
            JsonFields::complain(diagnostics, tr("A portamento point has an unknown curve type "
                                                 "and was read as the default type."));
        }
        return {object.value(QLatin1String("x")).toDouble(),
                object.value(QLatin1String("y")).toDouble(), type.value_or(S)};
    }

    QJsonObject PitchBend::toJson() const {
        QJsonArray array;
        for (const double value : values) {
            array.append(value);
        }
        QJsonObject object{
            {QLatin1String("values"), array}
        };
        if (start) {
            object.insert(QLatin1String("start"), *start);
        }
        return object;
    }

    PitchBend PitchBend::fromJson(const QJsonObject &object) {
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

    QJsonObject Note::toJson() const {
        QJsonObject object{
            {QLatin1String("lyric"),   lyric  },
            {QLatin1String("length"),  length },
            {QLatin1String("noteNum"), noteNum},
        };

        JsonFields::writeOptionalDouble(object, "intensity", intensity);
        JsonFields::writeOptionalDouble(object, "modulation", modulation);
        JsonFields::writeOptionalDouble(object, "velocity", velocity);
        JsonFields::writeOptionalDouble(object, "preUtterance", preUtterance);
        JsonFields::writeOptionalDouble(object, "voiceOverlap", voiceOverlap);
        JsonFields::writeOptionalDouble(object, "startPoint", startPoint);
        JsonFields::writeOptionalDouble(object, "tempo", tempo);

        if (!flags.isEmpty()) {
            object.insert(QLatin1String("flags"), flags);
        }
        if (envelope) {
            object.insert(QLatin1String("envelope"), envelope->toJson());
        }
        if (vibrato) {
            object.insert(QLatin1String("vibrato"), vibrato->toJson());
        }
        if (!portamento.isEmpty()) {
            QJsonArray points;
            for (const auto &point : portamento) {
                points.append(point.toJson());
            }
            object.insert(QLatin1String("portamento"), points);
        }
        if (pitchBend) {
            object.insert(QLatin1String("pitchBend"), pitchBend->toJson());
        }

        // By reference. Pairing each field with its name by value would copy five strings
        // per note unnecessarily.
        const std::pair<const char *, const QString &> texts[] = {
            {"label",     label    },
            {"direct",    direct   },
            {"patch",     patch    },
            {"region",    region   },
            {"regionEnd", regionEnd},
        };
        for (const auto &[key, value] : texts) {
            if (!value.isEmpty()) {
                object.insert(QLatin1String(key), value);
            }
        }

        if (!userData.isEmpty()) {
            QJsonObject entries;
            for (auto it = userData.begin(); it != userData.end(); ++it) {
                entries.insert(it.key(), it.value());
            }
            object.insert(QLatin1String("userData"), entries);
        }

        return object;
    }

    std::optional<Note> Note::fromJson(const QJsonObject &object, DiagnosticList &diagnostics) {
        const auto lyric = object.value(QLatin1String("lyric"));
        const auto length = object.value(QLatin1String("length"));
        const auto noteNum = object.value(QLatin1String("noteNum"));
        if (!lyric.isString() || !length.isDouble() || !noteNum.isDouble()) {
            JsonFields::fail(diagnostics, tr("A note is missing its lyric, length or pitch."));
            return std::nullopt;
        }

        Note note;
        note.lyric = lyric.toString();
        note.length = int(length.toDouble());
        note.noteNum = int(noteNum.toDouble());

        note.intensity = JsonFields::readOptionalDouble(object, "intensity", diagnostics);
        note.modulation = JsonFields::readOptionalDouble(object, "modulation", diagnostics);
        note.velocity = JsonFields::readOptionalDouble(object, "velocity", diagnostics);
        note.preUtterance = JsonFields::readOptionalDouble(object, "preUtterance", diagnostics);
        note.voiceOverlap = JsonFields::readOptionalDouble(object, "voiceOverlap", diagnostics);
        note.startPoint = JsonFields::readOptionalDouble(object, "startPoint", diagnostics);
        note.tempo = JsonFields::readOptionalDouble(object, "tempo", diagnostics);

        note.flags = JsonFields::readString(object, "flags");

        if (const auto envelope = object.value(QLatin1String("envelope")); envelope.isObject()) {
            note.envelope = Envelope::fromJson(envelope.toObject());
            if (!note.envelope) {
                JsonFields::complain(diagnostics, tr("The envelope of a note does not have four "
                                                     "or five anchors and was ignored."));
            }
        }
        if (const auto vibrato = object.value(QLatin1String("vibrato")); vibrato.isObject()) {
            note.vibrato = Vibrato::fromJson(vibrato.toObject());
        }
        for (const auto point : object.value(QLatin1String("portamento")).toArray()) {
            note.portamento.push_back(PortamentoPoint::fromJson(point.toObject(), diagnostics));
        }
        if (const auto bend = object.value(QLatin1String("pitchBend")); bend.isObject()) {
            note.pitchBend = PitchBend::fromJson(bend.toObject());
        }

        note.label = JsonFields::readString(object, "label");
        note.direct = JsonFields::readString(object, "direct");
        note.patch = JsonFields::readString(object, "patch");
        note.region = JsonFields::readString(object, "region");
        note.regionEnd = JsonFields::readString(object, "regionEnd");

        const auto entries = object.value(QLatin1String("userData")).toObject();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            note.userData.insert(it.key(), it.value().toString());
        }

        return note;
    }

}
