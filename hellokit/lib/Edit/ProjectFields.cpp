#include "ProjectFields_p.h"

#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        QJsonValue envelopeToJson(const QVariant &value) {
            return SlotValue<Envelope>::fromVariant(value).toJson();
        }

        std::optional<QVariant> envelopeFromJson(const QJsonValue &json) {
            const auto envelope =
                json.isObject() ? Envelope::fromJson(json.toObject()) : std::nullopt;
            if (!envelope) {
                return std::nullopt;
            }
            return SlotValue<Envelope>::toVariant(*envelope);
        }

        QJsonValue vibratoToJson(const QVariant &value) {
            return SlotValue<Vibrato>::fromVariant(value).toJson();
        }

        std::optional<QVariant> vibratoFromJson(const QJsonValue &json) {
            if (!json.isObject()) {
                return std::nullopt;
            }
            return SlotValue<Vibrato>::toVariant(Vibrato::fromJson(json.toObject()));
        }

        QJsonValue portamentoTypeToJson(const QVariant &value) {
            return PortamentoPoint::typeName(SlotValue<PortamentoPoint::Type>::fromVariant(value));
        }

        std::optional<QVariant> portamentoTypeFromJson(const QJsonValue &json) {
            const auto type = PortamentoPoint::typeFromName(json.toString());
            if (!type) {
                return std::nullopt;
            }
            return SlotValue<PortamentoPoint::Type>::toVariant(*type);
        }

        const ValueFormat envelopeFormat{"envelope", envelopeToJson, envelopeFromJson};
        const ValueFormat vibratoFormat{"vibrato", vibratoToJson, vibratoFromJson};
        const ValueFormat portamentoTypeFormat{"portamento type", portamentoTypeToJson,
                                               portamentoTypeFromJson};

    }

    template <>
    const ValueFormat &formatOf<Envelope>() {
        return envelopeFormat;
    }

    template <>
    const ValueFormat &formatOf<Vibrato>() {
        return vibratoFormat;
    }

    template <>
    const ValueFormat &formatOf<PortamentoPoint::Type>() {
        return portamentoTypeFormat;
    }

    namespace {

        const RecordInfo &pitchBendRecord() {
            static const RecordInfo record{
                "pitchBend",
                PitchBendType,
                {
                  valueField(PitchBendSlots::Start),
                  arrayField(PitchBendSlots::Values, PitchValuesType, ValueFormats::number),
                  },
                [](const QJsonObject &json, DiagnosticList &) {
                    return treeOf(PitchBend::fromJson(json));
                  },
                [](const ss::Node *tree) { return fromTree<PitchBend>(tree).toJson(); },
            };
            return record;
        }

        const RecordInfo &portamentoPointRecord() {
            static const RecordInfo record{
                "portamentoPoint",
                PortamentoPointType,
                {
                  valueField(PortamentoSlots::X),
                  valueField(PortamentoSlots::Y),
                  valueField(PortamentoSlots::Type),
                  },
                [](const QJsonObject &json, DiagnosticList &diagnostics) {
                    return treeOf(PortamentoPoint::fromJson(json, diagnostics));
                  },
                [](const ss::Node *tree) { return fromTree<PortamentoPoint>(tree).toJson(); },
            };
            return record;
        }

        const RecordInfo &noteRecord() {
            static const RecordInfo record{
                "note",
                NoteType,
                {
                  valueField(NoteSlots::Lyric),
                  valueField(NoteSlots::Length),
                  valueField(NoteSlots::NoteNum),
                  valueField(NoteSlots::Intensity),
                  valueField(NoteSlots::Modulation),
                  valueField(NoteSlots::Velocity),
                  valueField(NoteSlots::PreUtterance),
                  valueField(NoteSlots::VoiceOverlap),
                  valueField(NoteSlots::StartPoint),
                  valueField(NoteSlots::Tempo),
                  valueField(NoteSlots::Flags),
                  valueField(NoteSlots::Envelope),
                  valueField(NoteSlots::Vibrato),
                  listField(NoteSlots::Portamento, portamentoPointRecord()),
                  recordField(NoteSlots::PitchBend, pitchBendRecord(), true),
                  valueField(NoteSlots::Label),
                  valueField(NoteSlots::Direct),
                  valueField(NoteSlots::Patch),
                  valueField(NoteSlots::Region),
                  valueField(NoteSlots::RegionEnd),
                  mappingField(NoteSlots::UserData, ValueFormats::string),
                  },
                [](const QJsonObject &json,
                   DiagnosticList &diagnostics) -> std::unique_ptr<ss::Node> {
                    const auto note = Note::fromJson(json, diagnostics);
                    if (!note) {
                        return nullptr;
                    }
                    return treeOf(*note);
                  },
                [](const ss::Node *tree) { return fromTree<Note>(tree).toJson(); },
            };
            return record;
        }

        // A track is not created from JSON, because a project holds exactly one track.
        const RecordInfo &trackRecord() {
            static const RecordInfo record{
                "track",
                TrackType,
                {
                  valueField(TrackSlots::Name),
                  valueField(TrackSlots::VoiceDir),
                  listField(TrackSlots::Notes, noteRecord()),
                  },
            };
            return record;
        }

        const RecordInfo &settingsRecord() {
            static const RecordInfo record{
                "settings",
                SettingsType,
                {
                  valueField(SettingsSlots::Name),
                  valueField(SettingsSlots::Tempo),
                  valueField(SettingsSlots::Flags),
                  valueField(SettingsSlots::OutputFile),
                  valueField(SettingsSlots::CacheDir),
                  valueField(SettingsSlots::Wavtool),
                  valueField(SettingsSlots::Resampler),
                  valueField(SettingsSlots::Mode2),
                  },
            };
            return record;
        }

    }

    const RecordInfo &projectRecord() {
        static const RecordInfo record{
            "project",
            ProjectType,
            {
              recordField(ProjectSlots::Settings, settingsRecord(), false),
              listField(ProjectSlots::Tracks, trackRecord()),
              mappingField(ProjectSlots::UnknownFields, ValueFormats::json),
              },
        };
        return record;
    }

    const RecordInfo *projectRecordOf(int nodeType) {
        static const RecordInfo *const records[] = {
            &projectRecord(), &settingsRecord(),        &trackRecord(),
            &noteRecord(),    &portamentoPointRecord(), &pitchBendRecord(),
        };
        for (const auto record : records) {
            if (record->nodeType == nodeType) {
                return record;
            }
        }
        return nullptr;
    }

}
