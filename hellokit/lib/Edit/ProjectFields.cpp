#include "ProjectFields_p.h"

#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        QJsonValue envelopeToJson(const QVariant &value) {
            return edit::SlotValue<Envelope>::fromVariant(value).toJson();
        }

        std::optional<QVariant> envelopeFromJson(const QJsonValue &json) {
            const auto envelope =
                json.isObject() ? Envelope::fromJson(json.toObject()) : std::nullopt;
            if (!envelope) {
                return std::nullopt;
            }
            return edit::SlotValue<Envelope>::toVariant(*envelope);
        }

        QJsonValue vibratoToJson(const QVariant &value) {
            return edit::SlotValue<Vibrato>::fromVariant(value).toJson();
        }

        std::optional<QVariant> vibratoFromJson(const QJsonValue &json) {
            if (!json.isObject()) {
                return std::nullopt;
            }
            return edit::SlotValue<Vibrato>::toVariant(Vibrato::fromJson(json.toObject()));
        }

        QJsonValue portamentoTypeToJson(const QVariant &value) {
            return PortamentoPoint::typeName(
                edit::SlotValue<PortamentoPoint::Type>::fromVariant(value));
        }

        std::optional<QVariant> portamentoTypeFromJson(const QJsonValue &json) {
            const auto type = PortamentoPoint::typeFromName(json.toString());
            if (!type) {
                return std::nullopt;
            }
            return edit::SlotValue<PortamentoPoint::Type>::toVariant(*type);
        }

        constexpr edit::ValueFormat envelopeFormat{"envelope", envelopeToJson, envelopeFromJson};
        constexpr edit::ValueFormat vibratoFormat{"vibrato", vibratoToJson, vibratoFromJson};
        constexpr edit::ValueFormat portamentoTypeFormat{"portamento type", portamentoTypeToJson,
                                                         portamentoTypeFromJson};

    }

    template <>
    constexpr const edit::ValueFormat &edit::formatOf<Envelope>() {
        return envelopeFormat;
    }

    template <>
    constexpr const edit::ValueFormat &edit::formatOf<Vibrato>() {
        return vibratoFormat;
    }

    template <>
    constexpr const edit::ValueFormat &edit::formatOf<PortamentoPoint::Type>() {
        return portamentoTypeFormat;
    }

    namespace {

        // The records are defined before the records that refer to them.

        constexpr edit::FieldInfo pitchBendFields[] = {
            edit::valueField(PitchBendSlots::Start),
            edit::arrayField(PitchBendSlots::Values, PitchValuesType, edit::ValueFormats::number),
        };

        constexpr edit::RecordInfo pitchBendRecord{
            "pitchBend",
            PitchBendType,
            pitchBendFields,
            [](const QJsonObject &json, DiagnosticList &) {
                return treeOf(PitchBend::fromJson(json));
            },
            [](const ss::Node *tree) { return edit::fromTree<PitchBend>(tree).toJson(); },
        };

        constexpr edit::FieldInfo portamentoPointFields[] = {
            edit::valueField(PortamentoSlots::X),
            edit::valueField(PortamentoSlots::Y),
            edit::valueField(PortamentoSlots::Type),
        };

        constexpr edit::RecordInfo portamentoPointRecord{
            "portamentoPoint",
            PortamentoPointType,
            portamentoPointFields,
            [](const QJsonObject &json, DiagnosticList &diagnostics) {
                return treeOf(PortamentoPoint::fromJson(json, diagnostics));
            },
            [](const ss::Node *tree) { return edit::fromTree<PortamentoPoint>(tree).toJson(); },
        };

        constexpr edit::FieldInfo noteFields[] = {
            edit::valueField(NoteSlots::Lyric),
            edit::valueField(NoteSlots::Length),
            edit::valueField(NoteSlots::NoteNum),
            edit::valueField(NoteSlots::Intensity),
            edit::valueField(NoteSlots::Modulation),
            edit::valueField(NoteSlots::Velocity),
            edit::valueField(NoteSlots::PreUtterance),
            edit::valueField(NoteSlots::VoiceOverlap),
            edit::valueField(NoteSlots::StartPoint),
            edit::valueField(NoteSlots::Tempo),
            edit::valueField(NoteSlots::Flags),
            edit::valueField(NoteSlots::Envelope),
            edit::valueField(NoteSlots::Vibrato),
            edit::listField(NoteSlots::Portamento, portamentoPointRecord),
            edit::recordField(NoteSlots::PitchBend, pitchBendRecord, true),
            edit::valueField(NoteSlots::Label),
            edit::valueField(NoteSlots::Direct),
            edit::valueField(NoteSlots::Patch),
            edit::valueField(NoteSlots::Region),
            edit::valueField(NoteSlots::RegionEnd),
            edit::mappingField(NoteSlots::UserData, edit::ValueFormats::string),
        };

        constexpr edit::RecordInfo noteRecord{
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
            [](const ss::Node *tree) { return edit::fromTree<Note>(tree).toJson(); },
        };

        constexpr edit::FieldInfo trackFields[] = {
            edit::valueField(TrackSlots::Name),
            edit::valueField(TrackSlots::VoiceDir),
            edit::listField(TrackSlots::Notes, noteRecord),
        };

        // A track is not created from JSON, because a project holds exactly one track.
        constexpr edit::RecordInfo trackRecord{"track", TrackType, trackFields};

        constexpr edit::FieldInfo settingsFields[] = {
            edit::valueField(SettingsSlots::Name),      edit::valueField(SettingsSlots::Tempo),
            edit::valueField(SettingsSlots::Flags),     edit::valueField(SettingsSlots::OutputFile),
            edit::valueField(SettingsSlots::CacheDir),  edit::valueField(SettingsSlots::Wavtool),
            edit::valueField(SettingsSlots::Resampler), edit::valueField(SettingsSlots::Mode2),
        };

        constexpr edit::RecordInfo settingsRecord{"settings", SettingsType, settingsFields};

        constexpr edit::FieldInfo projectFields[] = {
            edit::recordField(ProjectSlots::Settings, settingsRecord, false),
            edit::listField(ProjectSlots::Tracks, trackRecord),
            edit::mappingField(ProjectSlots::UnknownFields, edit::ValueFormats::json),
        };

        constexpr edit::RecordInfo rootRecord{"project", ProjectType, projectFields};

        // Returns whether the fields of record are its count slots in order.
        constexpr bool listsSlotsInOrder(const edit::RecordInfo &record, int count) {
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

        constexpr const edit::RecordInfo *records[] = {
            &rootRecord, &settingsRecord,        &trackRecord,
            &noteRecord, &portamentoPointRecord, &pitchBendRecord,
        };

    }

    const edit::RecordInfo &projectRecord() {
        return rootRecord;
    }

    const edit::RecordInfo *projectRecordOf(int nodeType) {
        for (const auto record : records) {
            if (record->nodeType == nodeType) {
                return record;
            }
        }
        return nullptr;
    }

}
