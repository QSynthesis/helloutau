#ifndef HELLOKIT_EDIT_PROJECTSCHEMA_H
#define HELLOKIT_EDIT_PROJECTSCHEMA_H

#include <optional>

#include <QtCore/QString>

#include <hellokit/Document/Note.h>

#include <hellokit/EditBase/Slot.h>

namespace hello::kit {

    // The slots of the nodes of a project tree. Each record node has the slots of one namespace
    // below. A slot name equals the name of the corresponding .usth field, see
    // docs/UsthFormat.md. A slot of a std::optional type is empty if the field is absent.

    /// The slots of the root node.
    namespace ProjectSlots {
        /// A record with the slots of \c SettingsSlots.
        inline constexpr edit::ChildSlot Settings{0, "settings"};

        /// A list of records with the slots of \c TrackSlots. It holds exactly one track.
        inline constexpr edit::ChildSlot Tracks{1, "tracks"};

        /// A mapping of the unrecognized top-level fields. Each value holds a \c QJsonValue.
        inline constexpr edit::ChildSlot UnknownFields{2, "unknownFields"};

        inline constexpr int count = 3;
    }

    namespace SettingsSlots {
        inline constexpr edit::Slot<QString> Name{0, "name"};
        inline constexpr edit::Slot<double> Tempo{1, "tempo", edit::Range<double>::greaterThan(0)};
        inline constexpr edit::Slot<QString> Flags{2, "flags"};
        inline constexpr edit::Slot<QString> OutputFile{3, "outputFile"};
        inline constexpr edit::Slot<QString> CacheDir{4, "cacheDir"};
        inline constexpr edit::Slot<QString> Wavtool{5, "wavtool"};
        inline constexpr edit::Slot<QString> Resampler{6, "resampler"};
        inline constexpr edit::Slot<bool> Mode2{7, "mode2"};

        inline constexpr int count = 8;
    }

    namespace TrackSlots {
        inline constexpr edit::Slot<QString> Name{0, "name"};
        inline constexpr edit::Slot<QString> VoiceDir{1, "voiceDir"};

        /// A list of records with the slots of \c NoteSlots.
        inline constexpr edit::ChildSlot Notes{2, "notes"};

        inline constexpr int count = 3;
    }

    namespace NoteSlots {
        inline constexpr edit::Slot<QString> Lyric{0, "lyric"};
        inline constexpr edit::Slot<int> Length{1, "length", edit::Range<int>::atLeast(1)};
        inline constexpr edit::Slot<int> NoteNum{2, "noteNum", edit::Range<int>::between(0, 127)};
        inline constexpr edit::Slot<std::optional<double>> Intensity{3, "intensity"};
        inline constexpr edit::Slot<std::optional<double>> Modulation{4, "modulation"};
        inline constexpr edit::Slot<std::optional<double>> Velocity{5, "velocity"};
        inline constexpr edit::Slot<std::optional<double>> PreUtterance{6, "preUtterance"};
        inline constexpr edit::Slot<std::optional<double>> VoiceOverlap{7, "voiceOverlap"};
        inline constexpr edit::Slot<std::optional<double>> StartPoint{8, "startPoint"};
        inline constexpr edit::Slot<std::optional<double>> Tempo{
            9, "tempo", edit::Range<double>::greaterThan(0)};
        inline constexpr edit::Slot<QString> Flags{10, "flags"};

        /// The envelope as one value. Changing an anchor replaces the value.
        inline constexpr edit::Slot<std::optional<hello::kit::Envelope>> Envelope{11, "envelope"};

        /// The vibrato as one value. Changing a parameter replaces the value.
        inline constexpr edit::Slot<std::optional<hello::kit::Vibrato>> Vibrato{12, "vibrato"};

        /// A list of records with the slots of \c PortamentoSlots.
        inline constexpr edit::ChildSlot Portamento{13, "portamento"};

        /// A record with the slots of \c PitchBendSlots, or empty if the note has no Mode1 pitch
        /// curve.
        inline constexpr edit::ChildSlot PitchBend{14, "pitchBend"};

        inline constexpr edit::Slot<QString> Label{15, "label"};
        inline constexpr edit::Slot<QString> Direct{16, "direct"};
        inline constexpr edit::Slot<QString> Patch{17, "patch"};
        inline constexpr edit::Slot<QString> Region{18, "region"};
        inline constexpr edit::Slot<QString> RegionEnd{19, "regionEnd"};

        /// A mapping of the entries without a corresponding field. Each value holds a \c QString.
        inline constexpr edit::ChildSlot UserData{20, "userData"};

        inline constexpr int count = 21;
    }

    namespace PortamentoSlots {
        inline constexpr edit::Slot<double> X{0, "x"};
        inline constexpr edit::Slot<double> Y{1, "y"};
        inline constexpr edit::Slot<PortamentoPoint::Type> Type{2, "type"};

        inline constexpr int count = 3;
    }

    namespace PitchBendSlots {
        inline constexpr edit::Slot<std::optional<double>> Start{0, "start"};

        /// An array of the values in the order of \c PitchBend::values.
        inline constexpr edit::ChildSlot Values{1, "values"};

        inline constexpr int count = 2;
    }

    /// Stores the portamento type as its integer value, which requires no registration of the
    /// enumeration with the Qt meta-type system.
    template <>
    struct edit::SlotValue<PortamentoPoint::Type> {
        static inline QVariant toVariant(PortamentoPoint::Type value) {
            return int(value);
        }

        static inline PortamentoPoint::Type fromVariant(const QVariant &variant) {
            return PortamentoPoint::Type(variant.toInt());
        }
    };

}

#endif // HELLOKIT_EDIT_PROJECTSCHEMA_H
