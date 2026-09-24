#ifndef HELLOKIT_EDIT_PROJECTSCHEMA_H
#define HELLOKIT_EDIT_PROJECTSCHEMA_H

#include <optional>

#include <QtCore/QString>

#include <hellokit/Document/Note.h>

#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    // The slots of the nodes of a project tree. Each record node has the slots of one namespace
    // below. A slot name equals the name of the corresponding .usth field, see
    // docs/UsthFormat.md. A slot of a std::optional type is empty if the field is absent.

    /// The slots of the root node.
    namespace ProjectSlots {
        /// A record with the slots of \c SettingsSlots.
        inline constexpr ChildSlot Settings{0, "settings"};

        /// A list of records with the slots of \c TrackSlots. It holds exactly one track.
        inline constexpr ChildSlot Tracks{1, "tracks"};

        /// A mapping of the unrecognized top-level fields. Each value holds a \c QJsonValue.
        inline constexpr ChildSlot UnknownFields{2, "unknownFields"};

        inline constexpr int count = 3;
    }

    namespace SettingsSlots {
        inline constexpr Slot<QString> Name{0, "name"};
        inline constexpr Slot<double> Tempo{1, "tempo", Range::greaterThan(0)};
        inline constexpr Slot<QString> Flags{2, "flags"};
        inline constexpr Slot<QString> OutputFile{3, "outputFile"};
        inline constexpr Slot<QString> CacheDir{4, "cacheDir"};
        inline constexpr Slot<QString> Wavtool{5, "wavtool"};
        inline constexpr Slot<QString> Resampler{6, "resampler"};
        inline constexpr Slot<bool> Mode2{7, "mode2"};

        inline constexpr int count = 8;
    }

    namespace TrackSlots {
        inline constexpr Slot<QString> Name{0, "name"};
        inline constexpr Slot<QString> VoiceDir{1, "voiceDir"};

        /// A list of records with the slots of \c NoteSlots.
        inline constexpr ChildSlot Notes{2, "notes"};

        inline constexpr int count = 3;
    }

    namespace NoteSlots {
        inline constexpr Slot<QString> Lyric{0, "lyric"};
        inline constexpr Slot<int> Length{1, "length", Range::atLeast(1)};
        inline constexpr Slot<int> NoteNum{2, "noteNum", Range::between(0, 127)};
        inline constexpr Slot<std::optional<double>> Intensity{3, "intensity"};
        inline constexpr Slot<std::optional<double>> Modulation{4, "modulation"};
        inline constexpr Slot<std::optional<double>> Velocity{5, "velocity"};
        inline constexpr Slot<std::optional<double>> PreUtterance{6, "preUtterance"};
        inline constexpr Slot<std::optional<double>> VoiceOverlap{7, "voiceOverlap"};
        inline constexpr Slot<std::optional<double>> StartPoint{8, "startPoint"};
        inline constexpr Slot<std::optional<double>> Tempo{9, "tempo", Range::greaterThan(0)};
        inline constexpr Slot<QString> Flags{10, "flags"};

        /// The envelope as one value. Changing an anchor replaces the value.
        inline constexpr Slot<std::optional<hello::kit::Envelope>> Envelope{11, "envelope"};

        /// The vibrato as one value. Changing a parameter replaces the value.
        inline constexpr Slot<std::optional<hello::kit::Vibrato>> Vibrato{12, "vibrato"};

        /// A list of records with the slots of \c PortamentoSlots.
        inline constexpr ChildSlot Portamento{13, "portamento"};

        /// A record with the slots of \c PitchBendSlots, or empty if the note has no Mode1 pitch
        /// curve.
        inline constexpr ChildSlot PitchBend{14, "pitchBend"};

        inline constexpr Slot<QString> Label{15, "label"};
        inline constexpr Slot<QString> Direct{16, "direct"};
        inline constexpr Slot<QString> Patch{17, "patch"};
        inline constexpr Slot<QString> Region{18, "region"};
        inline constexpr Slot<QString> RegionEnd{19, "regionEnd"};

        /// A mapping of the entries without a corresponding field. Each value holds a \c QString.
        inline constexpr ChildSlot UserData{20, "userData"};

        inline constexpr int count = 21;
    }

    namespace PortamentoSlots {
        inline constexpr Slot<double> X{0, "x"};
        inline constexpr Slot<double> Y{1, "y"};
        inline constexpr Slot<PortamentoPoint::Type> Type{2, "type"};

        inline constexpr int count = 3;
    }

    namespace PitchBendSlots {
        inline constexpr Slot<std::optional<double>> Start{0, "start"};

        /// An array of the values in the order of \c PitchBend::values.
        inline constexpr ChildSlot Values{1, "values"};

        inline constexpr int count = 2;
    }

    /// Stores the portamento type as its integer value, which requires no registration of the
    /// enumeration with the Qt meta-type system.
    template <>
    struct SlotValue<PortamentoPoint::Type> {
        static inline QVariant toVariant(PortamentoPoint::Type value) {
            return int(value);
        }

        static inline PortamentoPoint::Type fromVariant(const QVariant &variant) {
            return PortamentoPoint::Type(variant.toInt());
        }
    };

}

#endif // HELLOKIT_EDIT_PROJECTSCHEMA_H
