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

        constexpr ValueFormat envelopeFormat{"envelope", envelopeToJson, envelopeFromJson};
        constexpr ValueFormat vibratoFormat{"vibrato", vibratoToJson, vibratoFromJson};
        constexpr ValueFormat portamentoTypeFormat{"portamento type", portamentoTypeToJson,
                                                   portamentoTypeFromJson};

    }

    template <>
    constexpr const ValueFormat &formatOf<Envelope>() {
        return envelopeFormat;
    }

    template <>
    constexpr const ValueFormat &formatOf<Vibrato>() {
        return vibratoFormat;
    }

    template <>
    constexpr const ValueFormat &formatOf<PortamentoPoint::Type>() {
        return portamentoTypeFormat;
    }

    namespace {

        // The records are defined before the records that refer to them.

        constexpr FieldInfo pitchBendFields[] = {
            valueField(PitchBendSlots::Start),
            arrayField(PitchBendSlots::Values, PitchValuesType, ValueFormats::number),
        };

        constexpr RecordInfo pitchBendRecord{
            "pitchBend",
            PitchBendType,
            pitchBendFields,
            [](const QJsonObject &json, DiagnosticList &) {
                return treeOf(PitchBend::fromJson(json));
            },
            [](const ss::Node *tree) { return fromTree<PitchBend>(tree).toJson(); },
        };

        constexpr FieldInfo portamentoPointFields[] = {
            valueField(PortamentoSlots::X),
            valueField(PortamentoSlots::Y),
            valueField(PortamentoSlots::Type),
        };

        constexpr RecordInfo portamentoPointRecord{
            "portamentoPoint",
            PortamentoPointType,
            portamentoPointFields,
            [](const QJsonObject &json, DiagnosticList &diagnostics) {
                return treeOf(PortamentoPoint::fromJson(json, diagnostics));
            },
            [](const ss::Node *tree) { return fromTree<PortamentoPoint>(tree).toJson(); },
        };

        constexpr FieldInfo noteFields[] = {
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
            listField(NoteSlots::Portamento, portamentoPointRecord),
            recordField(NoteSlots::PitchBend, pitchBendRecord, true),
            valueField(NoteSlots::Label),
            valueField(NoteSlots::Direct),
            valueField(NoteSlots::Patch),
            valueField(NoteSlots::Region),
            valueField(NoteSlots::RegionEnd),
            mappingField(NoteSlots::UserData, ValueFormats::string),
        };

        constexpr RecordInfo noteRecord{
            "note",
            NoteType,
            noteFields,
            [](const QJsonObject &json, DiagnosticList &diagnostics) -> std::unique_ptr<ss::Node> {
                const auto note = Note::fromJson(json, diagnostics);
                if (!note) {
                    return nullptr;
                }
                return treeOf(*note);
            },
            [](const ss::Node *tree) { return fromTree<Note>(tree).toJson(); },
        };

        constexpr FieldInfo trackFields[] = {
            valueField(TrackSlots::Name),
            valueField(TrackSlots::VoiceDir),
            listField(TrackSlots::Notes, noteRecord),
        };

        // A track is not created from JSON, because a project holds exactly one track.
        constexpr RecordInfo trackRecord{"track", TrackType, trackFields};

        constexpr FieldInfo settingsFields[] = {
            valueField(SettingsSlots::Name),      valueField(SettingsSlots::Tempo),
            valueField(SettingsSlots::Flags),     valueField(SettingsSlots::OutputFile),
            valueField(SettingsSlots::CacheDir),  valueField(SettingsSlots::Wavtool),
            valueField(SettingsSlots::Resampler), valueField(SettingsSlots::Mode2),
        };

        constexpr RecordInfo settingsRecord{"settings", SettingsType, settingsFields};

        constexpr FieldInfo projectFields[] = {
            recordField(ProjectSlots::Settings, settingsRecord, false),
            listField(ProjectSlots::Tracks, trackRecord),
            mappingField(ProjectSlots::UnknownFields, ValueFormats::json),
        };

        constexpr RecordInfo rootRecord{"project", ProjectType, projectFields};

        // Returns whether the fields of record are its count slots in order.
        constexpr bool listsSlotsInOrder(const RecordInfo &record, int count) {
            if (record.fields.size() != count) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (record.fields.at(i).index != i) {
                    return false;
                }
            }
            return true;
        }

        static_assert(listsSlotsInOrder(rootRecord, ProjectSlots::count));
        static_assert(listsSlotsInOrder(settingsRecord, SettingsSlots::count));
        static_assert(listsSlotsInOrder(trackRecord, TrackSlots::count));
        static_assert(listsSlotsInOrder(noteRecord, NoteSlots::count));
        static_assert(listsSlotsInOrder(portamentoPointRecord, PortamentoSlots::count));
        static_assert(listsSlotsInOrder(pitchBendRecord, PitchBendSlots::count));

        constexpr const RecordInfo *records[] = {
            &rootRecord, &settingsRecord,        &trackRecord,
            &noteRecord, &portamentoPointRecord, &pitchBendRecord,
        };

    }

    const RecordInfo &projectRecord() {
        return rootRecord;
    }

    const RecordInfo *projectRecordOf(int nodeType) {
        for (const auto record : records) {
            if (record->nodeType == nodeType) {
                return record;
            }
        }
        return nullptr;
    }

}
