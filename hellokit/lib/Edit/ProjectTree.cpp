#include "ProjectTree_p.h"

#include <QtCore/QJsonValue>

namespace hello::kit {

    namespace {

        template <class T>
        void put(ss::StructNodeBase &node, edit::Slot<T> slot,
                 const typename edit::Slot<T>::ValueType &value) {
            node.setAt(slot.index, edit::SlotValue<T>::toVariant(value));
        }

        void put(ss::StructNodeBase &node, edit::ChildSlot slot, std::unique_ptr<ss::Node> child) {
            node.setAt(slot.index, std::move(child));
        }

        template <class T>
        T get(const ss::StructNodeBase &node, edit::Slot<T> slot) {
            return edit::SlotValue<T>::fromVariant(node.variant(slot.index));
        }

        // Returns the record of type type in node, which the structure of the tree guarantees.
        template <class NodeType>
        const NodeType &recordOf(const ss::Node *node, int type) {
            Q_UNUSED(type)
            Q_ASSERT(node && node->type() == type);
            return static_cast<const NodeType &>(*node);
        }

        template <class T>
        std::unique_ptr<ss::Node> listTree(const QList<T> &values) {
            auto node = std::make_unique<ss::VectorNode>();
            for (const auto &value : values) {
                node->append(treeOf(value));
            }
            return node;
        }

        template <class T>
        QList<T> listOf(const ss::Node *node) {
            Q_ASSERT(node && node->type() == ss::Node::Vector);
            const auto &list = static_cast<const ss::VectorNode &>(*node);
            QList<T> values;
            values.reserve(list.size());
            for (int i = 0; i < list.size(); ++i) {
                values.push_back(edit::fromTree<T>(list.at(i)));
            }
            return values;
        }

        const ss::MappingNode &mappingOf(const ss::Node *node) {
            Q_ASSERT(node && node->type() == ss::Node::Mapping);
            return static_cast<const ss::MappingNode &>(*node);
        }

        // The mapping of a Note::userData, with a QString value in each entry.
        std::unique_ptr<ss::Node> userDataTree(const QMap<QString, QString> &userData) {
            auto node = std::make_unique<ss::MappingNode>();
            for (auto it = userData.cbegin(); it != userData.cend(); ++it) {
                node->setProperty(it.key(), QVariant(it.value()));
            }
            return node;
        }

        QMap<QString, QString> userDataOf(const ss::Node *node) {
            const auto &mapping = mappingOf(node);
            QMap<QString, QString> userData;
            for (const auto &key : mapping.keys()) {
                userData.insert(key, mapping.variant(key).toString());
            }
            return userData;
        }

        // The mapping of a Project::unknownFields, with a QJsonValue in each entry.
        std::unique_ptr<ss::Node> unknownFieldsTree(const QJsonObject &fields) {
            auto node = std::make_unique<ss::MappingNode>();
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
                // value() returns a QJsonValueConstRef, which QVariant would store as that type.
                node->setProperty(it.key(), QVariant::fromValue(QJsonValue(it.value())));
            }
            return node;
        }

        QJsonObject unknownFieldsOf(const ss::Node *node) {
            const auto &mapping = mappingOf(node);
            QJsonObject fields;
            for (const auto &key : mapping.keys()) {
                fields.insert(key, mapping.variant(key).value<QJsonValue>());
            }
            return fields;
        }

    }

    std::unique_ptr<ss::Node> treeOf(const Project &project) {
        auto node = std::make_unique<ProjectNode>(ProjectType);
        put(*node, ProjectSlots::Settings, treeOf(project.settings));
        put(*node, ProjectSlots::Tracks, listTree(project.tracks));
        put(*node, ProjectSlots::UnknownFields, unknownFieldsTree(project.unknownFields));
        return node;
    }

    template <>
    Project edit::fromTree<Project>(const ss::Node *node) {
        const auto &record = recordOf<ProjectNode>(node, ProjectType);
        Project project;
        project.settings =
            edit::fromTree<ProjectSettings>(record.child(ProjectSlots::Settings.index));
        project.tracks = listOf<Track>(record.child(ProjectSlots::Tracks.index));
        project.unknownFields = unknownFieldsOf(record.child(ProjectSlots::UnknownFields.index));
        return project;
    }

    std::unique_ptr<ss::Node> treeOf(const ProjectSettings &settings) {
        auto node = std::make_unique<SettingsNode>(SettingsType);
        put(*node, SettingsSlots::Name, settings.name);
        put(*node, SettingsSlots::Tempo, settings.tempo);
        put(*node, SettingsSlots::Flags, settings.flags);
        put(*node, SettingsSlots::OutputFile, settings.outputFile);
        put(*node, SettingsSlots::CacheDir, settings.cacheDir);
        put(*node, SettingsSlots::Wavtool, settings.wavtool);
        put(*node, SettingsSlots::Resampler, settings.resampler);
        put(*node, SettingsSlots::Mode2, settings.mode2);
        return node;
    }

    template <>
    ProjectSettings edit::fromTree<ProjectSettings>(const ss::Node *node) {
        const auto &record = recordOf<SettingsNode>(node, SettingsType);
        ProjectSettings settings;
        settings.name = get(record, SettingsSlots::Name);
        settings.tempo = get(record, SettingsSlots::Tempo);
        settings.flags = get(record, SettingsSlots::Flags);
        settings.outputFile = get(record, SettingsSlots::OutputFile);
        settings.cacheDir = get(record, SettingsSlots::CacheDir);
        settings.wavtool = get(record, SettingsSlots::Wavtool);
        settings.resampler = get(record, SettingsSlots::Resampler);
        settings.mode2 = get(record, SettingsSlots::Mode2);
        return settings;
    }

    std::unique_ptr<ss::Node> treeOf(const Track &track) {
        auto node = std::make_unique<TrackNode>(TrackType);
        put(*node, TrackSlots::Name, track.name);
        put(*node, TrackSlots::VoiceDir, track.voiceDir);
        put(*node, TrackSlots::Notes, listTree(track.notes));
        return node;
    }

    template <>
    Track edit::fromTree<Track>(const ss::Node *node) {
        const auto &record = recordOf<TrackNode>(node, TrackType);
        Track track;
        track.name = get(record, TrackSlots::Name);
        track.voiceDir = get(record, TrackSlots::VoiceDir);
        track.notes = listOf<Note>(record.child(TrackSlots::Notes.index));
        return track;
    }

    std::unique_ptr<ss::Node> treeOf(const Note &note) {
        auto node = std::make_unique<NoteNode>(NoteType);
        put(*node, NoteSlots::Lyric, note.lyric);
        put(*node, NoteSlots::Length, note.length);
        put(*node, NoteSlots::NoteNum, note.noteNum);
        put(*node, NoteSlots::Intensity, note.intensity);
        put(*node, NoteSlots::Modulation, note.modulation);
        put(*node, NoteSlots::Velocity, note.velocity);
        put(*node, NoteSlots::PreUtterance, note.preUtterance);
        put(*node, NoteSlots::VoiceOverlap, note.voiceOverlap);
        put(*node, NoteSlots::StartPoint, note.startPoint);
        put(*node, NoteSlots::Tempo, note.tempo);
        put(*node, NoteSlots::Flags, note.flags);
        put(*node, NoteSlots::Envelope, note.envelope);
        put(*node, NoteSlots::Vibrato, note.vibrato);
        put(*node, NoteSlots::Portamento, listTree(note.portamento));
        if (note.pitchBend) {
            put(*node, NoteSlots::PitchBend, treeOf(*note.pitchBend));
        }
        put(*node, NoteSlots::Label, note.label);
        put(*node, NoteSlots::Direct, note.direct);
        put(*node, NoteSlots::Patch, note.patch);
        put(*node, NoteSlots::Regions, note.regions);
        put(*node, NoteSlots::RegionEnds, note.regionEnds);
        put(*node, NoteSlots::UserData, userDataTree(note.userData));
        return node;
    }

    template <>
    Note edit::fromTree<Note>(const ss::Node *node) {
        const auto &record = recordOf<NoteNode>(node, NoteType);
        Note note;
        note.lyric = get(record, NoteSlots::Lyric);
        note.length = get(record, NoteSlots::Length);
        note.noteNum = get(record, NoteSlots::NoteNum);
        note.intensity = get(record, NoteSlots::Intensity);
        note.modulation = get(record, NoteSlots::Modulation);
        note.velocity = get(record, NoteSlots::Velocity);
        note.preUtterance = get(record, NoteSlots::PreUtterance);
        note.voiceOverlap = get(record, NoteSlots::VoiceOverlap);
        note.startPoint = get(record, NoteSlots::StartPoint);
        note.tempo = get(record, NoteSlots::Tempo);
        note.flags = get(record, NoteSlots::Flags);
        note.envelope = get(record, NoteSlots::Envelope);
        note.vibrato = get(record, NoteSlots::Vibrato);
        note.portamento = listOf<PortamentoPoint>(record.child(NoteSlots::Portamento.index));
        if (const auto pitchBend = record.child(NoteSlots::PitchBend.index)) {
            note.pitchBend = edit::fromTree<PitchBend>(pitchBend);
        }
        note.label = get(record, NoteSlots::Label);
        note.direct = get(record, NoteSlots::Direct);
        note.patch = get(record, NoteSlots::Patch);
        note.regions = get(record, NoteSlots::Regions);
        note.regionEnds = get(record, NoteSlots::RegionEnds);
        note.userData = userDataOf(record.child(NoteSlots::UserData.index));
        return note;
    }

    std::unique_ptr<ss::Node> treeOf(const PortamentoPoint &point) {
        auto node = std::make_unique<PortamentoPointNode>(PortamentoPointType);
        put(*node, PortamentoSlots::X, point.x);
        put(*node, PortamentoSlots::Y, point.y);
        put(*node, PortamentoSlots::Type, point.type);
        return node;
    }

    template <>
    PortamentoPoint edit::fromTree<PortamentoPoint>(const ss::Node *node) {
        const auto &record = recordOf<PortamentoPointNode>(node, PortamentoPointType);
        PortamentoPoint point;
        point.x = get(record, PortamentoSlots::X);
        point.y = get(record, PortamentoSlots::Y);
        point.type = get(record, PortamentoSlots::Type);
        return point;
    }

    std::unique_ptr<ss::Node> treeOf(const PitchBend &pitchBend) {
        auto values = std::make_unique<PitchValuesNode>(PitchValuesType);
        values->append(
            ss::ArrayView<double>(pitchBend.values.constData(), size_t(pitchBend.values.size())));

        auto node = std::make_unique<PitchBendNode>(PitchBendType);
        put(*node, PitchBendSlots::Start, pitchBend.start);
        put(*node, PitchBendSlots::Values, std::move(values));
        return node;
    }

    template <>
    PitchBend edit::fromTree<PitchBend>(const ss::Node *node) {
        const auto &record = recordOf<PitchBendNode>(node, PitchBendType);
        PitchBend pitchBend;
        pitchBend.start = get(record, PitchBendSlots::Start);
        const auto values =
            recordOf<PitchValuesNode>(record.child(PitchBendSlots::Values.index), PitchValuesType)
                .values();
        pitchBend.values = QList<double>(values.cbegin(), values.cend());
        return pitchBend;
    }

    void registerProjectTypes(ss::QCodec &codec) {
        codec.registerNodeType(ProjectType,
                               [] { return std::make_unique<ProjectNode>(ProjectType); });
        codec.registerNodeType(SettingsType,
                               [] { return std::make_unique<SettingsNode>(SettingsType); });
        codec.registerNodeType(TrackType, [] { return std::make_unique<TrackNode>(TrackType); });
        codec.registerNodeType(NoteType, [] { return std::make_unique<NoteNode>(NoteType); });
        codec.registerNodeType(PortamentoPointType, [] {
            return std::make_unique<PortamentoPointNode>(PortamentoPointType);
        });
        codec.registerNodeType(PitchBendType,
                               [] { return std::make_unique<PitchBendNode>(PitchBendType); });
        codec.registerNodeType(PitchValuesType,
                               [] { return std::make_unique<PitchValuesNode>(PitchValuesType); });

        qRegisterMetaType<Envelope>();
        qRegisterMetaType<Vibrato>();
    }

}
