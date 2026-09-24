#include "ProjectTree_p.h"

#include <QtCore/QJsonValue>

namespace hello::kit {

    namespace {

        template <class T>
        void put(ss::StructNodeBase &node, Slot<T> slot, const typename Slot<T>::ValueType &value) {
            node.setAt(slot.index, SlotValue<T>::toVariant(value));
        }

        void put(ss::StructNodeBase &node, ChildSlot slot, std::unique_ptr<ss::Node> child) {
            node.setAt(slot.index, std::move(child));
        }

        template <class T>
        T get(const ss::StructNodeBase &node, Slot<T> slot) {
            return SlotValue<T>::fromVariant(node.variant(slot.index));
        }

        // Returns the child in slot, or nullptr if the slot is empty. The child must be of type.
        template <class NodeType>
        const NodeType *childOf(const ss::StructNodeBase &node, ChildSlot slot, int type) {
            const auto child = node.child(slot.index);
            Q_ASSERT(!child || child->type() == type);
            return static_cast<const NodeType *>(child);
        }

        std::unique_ptr<ss::Node> settingsTree(const ProjectSettings &settings) {
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

        std::unique_ptr<ss::Node> pointTree(const PortamentoPoint &point) {
            auto node = std::make_unique<PortamentoPointNode>(PortamentoPointType);
            put(*node, PortamentoSlots::X, point.x);
            put(*node, PortamentoSlots::Y, point.y);
            put(*node, PortamentoSlots::Type, point.type);
            return node;
        }

        std::unique_ptr<ss::Node> portamentoTree(const QList<PortamentoPoint> &portamento) {
            auto node = std::make_unique<ss::VectorNode>();
            for (const auto &point : portamento) {
                node->append(pointTree(point));
            }
            return node;
        }

        std::unique_ptr<ss::Node> pitchBendTree(const PitchBend &pitchBend) {
            auto values = std::make_unique<PitchValuesNode>(PitchValuesType);
            values->append(ss::ArrayView<double>(pitchBend.values.constData(),
                                                 size_t(pitchBend.values.size())));

            auto node = std::make_unique<PitchBendNode>(PitchBendType);
            put(*node, PitchBendSlots::Start, pitchBend.start);
            put(*node, PitchBendSlots::Values, std::move(values));
            return node;
        }

        std::unique_ptr<ss::Node> userDataTree(const QMap<QString, QString> &userData) {
            auto node = std::make_unique<ss::MappingNode>();
            for (auto it = userData.cbegin(); it != userData.cend(); ++it) {
                node->setProperty(it.key(), QVariant(it.value()));
            }
            return node;
        }

        std::unique_ptr<ss::Node> noteTree(const Note &note) {
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
            put(*node, NoteSlots::Portamento, portamentoTree(note.portamento));
            if (note.pitchBend) {
                put(*node, NoteSlots::PitchBend, pitchBendTree(*note.pitchBend));
            }
            put(*node, NoteSlots::Label, note.label);
            put(*node, NoteSlots::Direct, note.direct);
            put(*node, NoteSlots::Patch, note.patch);
            put(*node, NoteSlots::Region, note.region);
            put(*node, NoteSlots::RegionEnd, note.regionEnd);
            put(*node, NoteSlots::UserData, userDataTree(note.userData));
            return node;
        }

        std::unique_ptr<ss::Node> trackTree(const Track &track) {
            auto notes = std::make_unique<ss::VectorNode>();
            for (const auto &note : track.notes) {
                notes->append(noteTree(note));
            }

            auto node = std::make_unique<TrackNode>(TrackType);
            put(*node, TrackSlots::Name, track.name);
            put(*node, TrackSlots::VoiceDir, track.voiceDir);
            put(*node, TrackSlots::Notes, std::move(notes));
            return node;
        }

        std::unique_ptr<ss::Node> unknownFieldsTree(const QJsonObject &fields) {
            auto node = std::make_unique<ss::MappingNode>();
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
                // value() returns a QJsonValueConstRef, which QVariant would store as that type.
                node->setProperty(it.key(), QVariant::fromValue(QJsonValue(it.value())));
            }
            return node;
        }

        ProjectSettings settingsOf(const SettingsNode &node) {
            ProjectSettings settings;
            settings.name = get(node, SettingsSlots::Name);
            settings.tempo = get(node, SettingsSlots::Tempo);
            settings.flags = get(node, SettingsSlots::Flags);
            settings.outputFile = get(node, SettingsSlots::OutputFile);
            settings.cacheDir = get(node, SettingsSlots::CacheDir);
            settings.wavtool = get(node, SettingsSlots::Wavtool);
            settings.resampler = get(node, SettingsSlots::Resampler);
            settings.mode2 = get(node, SettingsSlots::Mode2);
            return settings;
        }

        PortamentoPoint pointOf(const PortamentoPointNode &node) {
            PortamentoPoint point;
            point.x = get(node, PortamentoSlots::X);
            point.y = get(node, PortamentoSlots::Y);
            point.type = get(node, PortamentoSlots::Type);
            return point;
        }

        QList<PortamentoPoint> portamentoOf(const ss::VectorNode &node) {
            QList<PortamentoPoint> portamento;
            portamento.reserve(node.size());
            for (int i = 0; i < node.size(); ++i) {
                Q_ASSERT(node.at(i)->type() == PortamentoPointType);
                portamento.push_back(
                    pointOf(static_cast<const PortamentoPointNode &>(*node.at(i))));
            }
            return portamento;
        }

        PitchBend pitchBendOf(const PitchBendNode &node) {
            PitchBend pitchBend;
            pitchBend.start = get(node, PitchBendSlots::Start);
            const auto values =
                childOf<PitchValuesNode>(node, PitchBendSlots::Values, PitchValuesType)->values();
            pitchBend.values = QList<double>(values.cbegin(), values.cend());
            return pitchBend;
        }

        QMap<QString, QString> userDataOf(const ss::MappingNode &node) {
            QMap<QString, QString> userData;
            for (const auto &key : node.keys()) {
                userData.insert(key, node.variant(key).toString());
            }
            return userData;
        }

        Note noteOf(const NoteNode &node) {
            Note note;
            note.lyric = get(node, NoteSlots::Lyric);
            note.length = get(node, NoteSlots::Length);
            note.noteNum = get(node, NoteSlots::NoteNum);
            note.intensity = get(node, NoteSlots::Intensity);
            note.modulation = get(node, NoteSlots::Modulation);
            note.velocity = get(node, NoteSlots::Velocity);
            note.preUtterance = get(node, NoteSlots::PreUtterance);
            note.voiceOverlap = get(node, NoteSlots::VoiceOverlap);
            note.startPoint = get(node, NoteSlots::StartPoint);
            note.tempo = get(node, NoteSlots::Tempo);
            note.flags = get(node, NoteSlots::Flags);
            note.envelope = get(node, NoteSlots::Envelope);
            note.vibrato = get(node, NoteSlots::Vibrato);
            note.portamento = portamentoOf(
                *childOf<ss::VectorNode>(node, NoteSlots::Portamento, ss::Node::Vector));
            if (const auto pitchBend =
                    childOf<PitchBendNode>(node, NoteSlots::PitchBend, PitchBendType)) {
                note.pitchBend = pitchBendOf(*pitchBend);
            }
            note.label = get(node, NoteSlots::Label);
            note.direct = get(node, NoteSlots::Direct);
            note.patch = get(node, NoteSlots::Patch);
            note.region = get(node, NoteSlots::Region);
            note.regionEnd = get(node, NoteSlots::RegionEnd);
            note.userData =
                userDataOf(*childOf<ss::MappingNode>(node, NoteSlots::UserData, ss::Node::Mapping));
            return note;
        }

        Track trackOf(const TrackNode &node) {
            Track track;
            track.name = get(node, TrackSlots::Name);
            track.voiceDir = get(node, TrackSlots::VoiceDir);
            const auto &notes = *childOf<ss::VectorNode>(node, TrackSlots::Notes, ss::Node::Vector);
            track.notes.reserve(notes.size());
            for (int i = 0; i < notes.size(); ++i) {
                Q_ASSERT(notes.at(i)->type() == NoteType);
                track.notes.push_back(noteOf(static_cast<const NoteNode &>(*notes.at(i))));
            }
            return track;
        }

        QJsonObject unknownFieldsOf(const ss::MappingNode &node) {
            QJsonObject fields;
            for (const auto &key : node.keys()) {
                fields.insert(key, node.variant(key).value<QJsonValue>());
            }
            return fields;
        }

    }

    std::unique_ptr<ss::Node> treeOf(const Project &project) {
        auto tracks = std::make_unique<ss::VectorNode>();
        for (const auto &track : project.tracks) {
            tracks->append(trackTree(track));
        }

        auto node = std::make_unique<ProjectNode>(ProjectType);
        put(*node, ProjectSlots::Settings, settingsTree(project.settings));
        put(*node, ProjectSlots::Tracks, std::move(tracks));
        put(*node, ProjectSlots::UnknownFields, unknownFieldsTree(project.unknownFields));
        return node;
    }

    Project projectOf(const ss::Node *root) {
        Q_ASSERT(root && root->type() == ProjectType);
        const auto &node = static_cast<const ProjectNode &>(*root);

        Project project;
        project.settings =
            settingsOf(*childOf<SettingsNode>(node, ProjectSlots::Settings, SettingsType));

        const auto &tracks = *childOf<ss::VectorNode>(node, ProjectSlots::Tracks, ss::Node::Vector);
        project.tracks.reserve(tracks.size());
        for (int i = 0; i < tracks.size(); ++i) {
            Q_ASSERT(tracks.at(i)->type() == TrackType);
            project.tracks.push_back(trackOf(static_cast<const TrackNode &>(*tracks.at(i))));
        }

        project.unknownFields = unknownFieldsOf(
            *childOf<ss::MappingNode>(node, ProjectSlots::UnknownFields, ss::Node::Mapping));
        return project;
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

    std::unique_ptr<ss::Node> treeOfNote(const Note &note) {
        return noteTree(note);
    }

    std::unique_ptr<ss::Node> treeOfPortamentoPoint(const PortamentoPoint &point) {
        return pointTree(point);
    }

    std::unique_ptr<ss::Node> treeOfPitchBend(const PitchBend &pitchBend) {
        return pitchBendTree(pitchBend);
    }

    Note noteOfTree(const ss::Node *node) {
        Q_ASSERT(node && node->type() == NoteType);
        return noteOf(static_cast<const NoteNode &>(*node));
    }

    PortamentoPoint portamentoPointOfTree(const ss::Node *node) {
        Q_ASSERT(node && node->type() == PortamentoPointType);
        return pointOf(static_cast<const PortamentoPointNode &>(*node));
    }

}
