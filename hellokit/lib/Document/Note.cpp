#include "Note.h"

#include <utility>

#include <QtCore/QJsonArray>

#include "JsonFields_p.h"

namespace hello::kit {

    QStringList Note::regionNamesFromUst(const QString &value) {
        return value.split(u'|', Qt::SkipEmptyParts);
    }

    QString Note::regionNamesToUst(const QStringList &names) {
        return names.join(u'|');
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

        // By reference. Pairing each field with its name by value would copy the strings of every
        // note unnecessarily.
        const std::pair<const char *, const QString &> texts[] = {
            {"label",  label },
            {"direct", direct},
            {"patch",  patch },
        };
        for (const auto &[key, value] : texts) {
            if (!value.isEmpty()) {
                object.insert(QLatin1String(key), value);
            }
        }
        const std::pair<const char *, const QStringList &> lists[] = {
            {"regions",    regions   },
            {"regionEnds", regionEnds},
        };
        for (const auto &[key, value] : lists) {
            if (!value.isEmpty()) {
                object.insert(QLatin1String(key), QJsonArray::fromStringList(value));
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
        note.regions = JsonFields::readNames(object, "regions");
        note.regionEnds = JsonFields::readNames(object, "regionEnds");

        const auto entries = object.value(QLatin1String("userData")).toObject();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            note.userData.insert(it.key(), it.value().toString());
        }

        return note;
    }

}
