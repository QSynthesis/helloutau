#include "ProjectRefs.h"

#include <vector>

#include <substate/ArrayNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include "EditSession_p.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        // The node class and the node type of the node of each handle, see ProjectTree_p.h.
        template <class Ref>
        struct NodeOf;

        template <class NodeClass, int nodeType>
        struct NodeTraits {
            using Type = NodeClass;
            static constexpr int type = nodeType;
        };

        template <>
        struct NodeOf<ProjectRef> : NodeTraits<ProjectNode, ProjectType> {};
        template <>
        struct NodeOf<SettingsRef> : NodeTraits<SettingsNode, SettingsType> {};
        template <>
        struct NodeOf<TrackRef> : NodeTraits<TrackNode, TrackType> {};
        template <>
        struct NodeOf<NoteListRef> : NodeTraits<ss::VectorNode, ss::Node::Vector> {};
        template <>
        struct NodeOf<NoteRef> : NodeTraits<NoteNode, NoteType> {};
        template <>
        struct NodeOf<PortamentoListRef> : NodeTraits<ss::VectorNode, ss::Node::Vector> {};
        template <>
        struct NodeOf<PortamentoPointRef> : NodeTraits<PortamentoPointNode, PortamentoPointType> {};
        template <>
        struct NodeOf<PitchBendRef> : NodeTraits<PitchBendNode, PitchBendType> {};
        template <>
        struct NodeOf<UserDataRef> : NodeTraits<ss::MappingNode, ss::Node::Mapping> {};

        template <class Ref>
        typename NodeOf<Ref>::Type *nodeOf(const Ref &ref) {
            return EditSessionPrivate::find<typename NodeOf<Ref>::Type>(ref.session(), ref.id(),
                                                                        NodeOf<Ref>::type);
        }

        template <class Ref>
        typename NodeOf<Ref>::Type *editableOf(const Ref &ref) {
            return EditSessionPrivate::findEditable<typename NodeOf<Ref>::Type>(
                ref.session(), ref.id(), NodeOf<Ref>::type);
        }

        template <class Ref, class T>
        T get(const Ref &record, Slot<T> slot) {
            const auto node = nodeOf(record);
            return SlotValue<T>::fromVariant(node ? node->variant(slot.index) : QVariant());
        }

        template <class Ref, class T>
        void set(const Ref &record, Slot<T> slot, const typename Slot<T>::ValueType &value) {
            if (const auto node = editableOf(record)) {
                node->setAt(slot.index, SlotValue<T>::toVariant(value));
            }
        }

        NodeId childId(const ss::StructNodeBase *record, ChildSlot slot) {
            const auto child = record ? record->child(slot.index) : nullptr;
            return child ? child->id() : 0;
        }

        template <class ChildRef, class Ref>
        ChildRef child(const Ref &record, ChildSlot slot) {
            return ChildRef(record.session(), childId(nodeOf(record), slot));
        }

        int itemCount(const ss::VectorNode *list) {
            return list ? list->size() : 0;
        }

        NodeId itemId(const ss::VectorNode *list, int index) {
            if (!list) {
                return 0;
            }
            Q_ASSERT(index >= 0 && index < list->size());
            return list->at(index)->id();
        }

        // Inserts the trees of values into a list whose items belong to a record of type owner.
        // The owner is determined by the handle, therefore it is compared only by an assertion.
        template <class Ref, class Value, class Convert>
        void insertItems(const Ref &list, int owner, int index, const QList<Value> &values,
                         Convert convert) {
            Q_UNUSED(owner)
            const auto node = editableOf(list);
            if (!node || values.isEmpty()) {
                return;
            }
            Q_ASSERT(node->parent()->type() == owner);
            std::vector<std::unique_ptr<ss::Node>> items;
            items.reserve(size_t(values.size()));
            for (const auto &value : values) {
                items.push_back(convert(value));
            }
            node->insert(index, std::move(items));
        }

        template <class Ref>
        void removeItems(const Ref &list, int index, int count) {
            if (const auto node = editableOf(list)) {
                node->remove(index, count);
            }
        }

        template <class Ref>
        void moveItems(const Ref &list, int index, int count, int destination) {
            if (const auto node = editableOf(list)) {
                node->move(index, count, destination);
            }
        }

        PitchValuesNode *pitchValuesOf(const PitchBendRef &pitchBend, bool editable) {
            const auto id = childId(nodeOf(pitchBend), PitchBendSlots::Values);
            return editable ? EditSessionPrivate::findEditable<PitchValuesNode>(pitchBend.session(),
                                                                                id, PitchValuesType)
                            : EditSessionPrivate::find<PitchValuesNode>(pitchBend.session(), id,
                                                                        PitchValuesType);
        }

        const ss::VectorNode *tracksOf(const ProjectRef &project) {
            return EditSessionPrivate::find<ss::VectorNode>(
                project.session(), childId(nodeOf(project), ProjectSlots::Tracks),
                ss::Node::Vector);
        }

        ss::ArrayView<double> viewOf(const QList<double> &values) {
            return ss::ArrayView<double>(values.constData(), size_t(values.size()));
        }

    }

    QString SettingsRef::name() const {
        return get(*this, SettingsSlots::Name);
    }

    void SettingsRef::setName(const QString &name) const {
        set(*this, SettingsSlots::Name, name);
    }

    double SettingsRef::tempo() const {
        return get(*this, SettingsSlots::Tempo);
    }

    void SettingsRef::setTempo(double tempo) const {
        set(*this, SettingsSlots::Tempo, tempo);
    }

    QString SettingsRef::flags() const {
        return get(*this, SettingsSlots::Flags);
    }

    void SettingsRef::setFlags(const QString &flags) const {
        set(*this, SettingsSlots::Flags, flags);
    }

    QString SettingsRef::outputFile() const {
        return get(*this, SettingsSlots::OutputFile);
    }

    void SettingsRef::setOutputFile(const QString &outputFile) const {
        set(*this, SettingsSlots::OutputFile, outputFile);
    }

    QString SettingsRef::cacheDir() const {
        return get(*this, SettingsSlots::CacheDir);
    }

    void SettingsRef::setCacheDir(const QString &cacheDir) const {
        set(*this, SettingsSlots::CacheDir, cacheDir);
    }

    QString SettingsRef::wavtool() const {
        return get(*this, SettingsSlots::Wavtool);
    }

    void SettingsRef::setWavtool(const QString &wavtool) const {
        set(*this, SettingsSlots::Wavtool, wavtool);
    }

    QString SettingsRef::resampler() const {
        return get(*this, SettingsSlots::Resampler);
    }

    void SettingsRef::setResampler(const QString &resampler) const {
        set(*this, SettingsSlots::Resampler, resampler);
    }

    bool SettingsRef::mode2() const {
        return get(*this, SettingsSlots::Mode2);
    }

    void SettingsRef::setMode2(bool mode2) const {
        set(*this, SettingsSlots::Mode2, mode2);
    }

    double PortamentoPointRef::x() const {
        return get(*this, PortamentoSlots::X);
    }

    void PortamentoPointRef::setX(double x) const {
        set(*this, PortamentoSlots::X, x);
    }

    double PortamentoPointRef::y() const {
        return get(*this, PortamentoSlots::Y);
    }

    void PortamentoPointRef::setY(double y) const {
        set(*this, PortamentoSlots::Y, y);
    }

    PortamentoPoint::Type PortamentoPointRef::type() const {
        return get(*this, PortamentoSlots::Type);
    }

    void PortamentoPointRef::setType(PortamentoPoint::Type type) const {
        set(*this, PortamentoSlots::Type, type);
    }

    PortamentoPoint PortamentoPointRef::toPoint() const {
        return {x(), y(), type()};
    }

    int PortamentoListRef::size() const {
        return itemCount(nodeOf(*this));
    }

    PortamentoPointRef PortamentoListRef::at(int index) const {
        return PortamentoPointRef(session(), itemId(nodeOf(*this), index));
    }

    void PortamentoListRef::insert(int index, const QList<PortamentoPoint> &points) const {
        insertItems(*this, NoteType, index, points, treeOfPortamentoPoint);
    }

    void PortamentoListRef::remove(int index, int count) const {
        removeItems(*this, index, count);
    }

    void PortamentoListRef::move(int index, int count, int destination) const {
        moveItems(*this, index, count, destination);
    }

    std::optional<double> PitchBendRef::start() const {
        return get(*this, PitchBendSlots::Start);
    }

    void PitchBendRef::setStart(std::optional<double> start) const {
        set(*this, PitchBendSlots::Start, start);
    }

    int PitchBendRef::size() const {
        const auto values = pitchValuesOf(*this, false);
        return values ? values->size() : 0;
    }

    QList<double> PitchBendRef::values() const {
        const auto node = pitchValuesOf(*this, false);
        if (!node) {
            return {};
        }
        const auto values = node->values();
        return QList<double>(values.cbegin(), values.cend());
    }

    void PitchBendRef::replaceValues(int index, const QList<double> &values) const {
        if (const auto node = pitchValuesOf(*this, true)) {
            node->replace(index, viewOf(values));
        }
    }

    void PitchBendRef::insertValues(int index, const QList<double> &values) const {
        if (const auto node = pitchValuesOf(*this, true)) {
            node->insert(index, viewOf(values));
        }
    }

    void PitchBendRef::removeValues(int index, int count) const {
        if (const auto node = pitchValuesOf(*this, true)) {
            node->remove(index, count);
        }
    }

    QStringList UserDataRef::keys() const {
        const auto node = nodeOf(*this);
        return node ? node->keys() : QStringList();
    }

    bool UserDataRef::contains(const QString &key) const {
        const auto node = nodeOf(*this);
        return node && node->contains(key);
    }

    QString UserDataRef::value(const QString &key) const {
        const auto node = nodeOf(*this);
        return node ? node->variant(key).toString() : QString();
    }

    void UserDataRef::setValue(const QString &key, const QString &value) const {
        if (const auto node = editableOf(*this)) {
            node->setProperty(key, QVariant(value));
        }
    }

    void UserDataRef::remove(const QString &key) const {
        if (const auto node = editableOf(*this)) {
            node->setProperty(key, ss::Property());
        }
    }

    QString NoteRef::lyric() const {
        return get(*this, NoteSlots::Lyric);
    }

    void NoteRef::setLyric(const QString &lyric) const {
        set(*this, NoteSlots::Lyric, lyric);
    }

    int NoteRef::length() const {
        return get(*this, NoteSlots::Length);
    }

    void NoteRef::setLength(int length) const {
        set(*this, NoteSlots::Length, length);
    }

    int NoteRef::noteNum() const {
        return get(*this, NoteSlots::NoteNum);
    }

    void NoteRef::setNoteNum(int noteNum) const {
        set(*this, NoteSlots::NoteNum, noteNum);
    }

    std::optional<double> NoteRef::intensity() const {
        return get(*this, NoteSlots::Intensity);
    }

    void NoteRef::setIntensity(std::optional<double> intensity) const {
        set(*this, NoteSlots::Intensity, intensity);
    }

    std::optional<double> NoteRef::modulation() const {
        return get(*this, NoteSlots::Modulation);
    }

    void NoteRef::setModulation(std::optional<double> modulation) const {
        set(*this, NoteSlots::Modulation, modulation);
    }

    std::optional<double> NoteRef::velocity() const {
        return get(*this, NoteSlots::Velocity);
    }

    void NoteRef::setVelocity(std::optional<double> velocity) const {
        set(*this, NoteSlots::Velocity, velocity);
    }

    std::optional<double> NoteRef::preUtterance() const {
        return get(*this, NoteSlots::PreUtterance);
    }

    void NoteRef::setPreUtterance(std::optional<double> preUtterance) const {
        set(*this, NoteSlots::PreUtterance, preUtterance);
    }

    std::optional<double> NoteRef::voiceOverlap() const {
        return get(*this, NoteSlots::VoiceOverlap);
    }

    void NoteRef::setVoiceOverlap(std::optional<double> voiceOverlap) const {
        set(*this, NoteSlots::VoiceOverlap, voiceOverlap);
    }

    std::optional<double> NoteRef::startPoint() const {
        return get(*this, NoteSlots::StartPoint);
    }

    void NoteRef::setStartPoint(std::optional<double> startPoint) const {
        set(*this, NoteSlots::StartPoint, startPoint);
    }

    std::optional<double> NoteRef::tempo() const {
        return get(*this, NoteSlots::Tempo);
    }

    void NoteRef::setTempo(std::optional<double> tempo) const {
        set(*this, NoteSlots::Tempo, tempo);
    }

    QString NoteRef::flags() const {
        return get(*this, NoteSlots::Flags);
    }

    void NoteRef::setFlags(const QString &flags) const {
        set(*this, NoteSlots::Flags, flags);
    }

    std::optional<Envelope> NoteRef::envelope() const {
        return get(*this, NoteSlots::Envelope);
    }

    void NoteRef::setEnvelope(const std::optional<Envelope> &envelope) const {
        set(*this, NoteSlots::Envelope, envelope);
    }

    std::optional<Vibrato> NoteRef::vibrato() const {
        return get(*this, NoteSlots::Vibrato);
    }

    void NoteRef::setVibrato(const std::optional<Vibrato> &vibrato) const {
        set(*this, NoteSlots::Vibrato, vibrato);
    }

    PortamentoListRef NoteRef::portamento() const {
        return child<PortamentoListRef>(*this, NoteSlots::Portamento);
    }

    PitchBendRef NoteRef::pitchBend() const {
        return child<PitchBendRef>(*this, NoteSlots::PitchBend);
    }

    void NoteRef::setPitchBend(const std::optional<PitchBend> &pitchBend) const {
        if (const auto node = editableOf(*this)) {
            node->setAt(NoteSlots::PitchBend.index,
                        pitchBend ? ss::Property(treeOfPitchBend(*pitchBend)) : ss::Property());
        }
    }

    QString NoteRef::label() const {
        return get(*this, NoteSlots::Label);
    }

    void NoteRef::setLabel(const QString &label) const {
        set(*this, NoteSlots::Label, label);
    }

    QString NoteRef::direct() const {
        return get(*this, NoteSlots::Direct);
    }

    void NoteRef::setDirect(const QString &direct) const {
        set(*this, NoteSlots::Direct, direct);
    }

    QString NoteRef::patch() const {
        return get(*this, NoteSlots::Patch);
    }

    void NoteRef::setPatch(const QString &patch) const {
        set(*this, NoteSlots::Patch, patch);
    }

    QString NoteRef::region() const {
        return get(*this, NoteSlots::Region);
    }

    void NoteRef::setRegion(const QString &region) const {
        set(*this, NoteSlots::Region, region);
    }

    QString NoteRef::regionEnd() const {
        return get(*this, NoteSlots::RegionEnd);
    }

    void NoteRef::setRegionEnd(const QString &regionEnd) const {
        set(*this, NoteSlots::RegionEnd, regionEnd);
    }

    UserDataRef NoteRef::userData() const {
        return child<UserDataRef>(*this, NoteSlots::UserData);
    }

    Note NoteRef::toNote() const {
        const auto node = nodeOf(*this);
        return node ? noteOfTree(node) : Note();
    }

    int NoteListRef::size() const {
        return itemCount(nodeOf(*this));
    }

    NoteRef NoteListRef::at(int index) const {
        return NoteRef(session(), itemId(nodeOf(*this), index));
    }

    void NoteListRef::insert(int index, const QList<Note> &notes) const {
        insertItems(*this, TrackType, index, notes, treeOfNote);
    }

    void NoteListRef::remove(int index, int count) const {
        removeItems(*this, index, count);
    }

    void NoteListRef::move(int index, int count, int destination) const {
        moveItems(*this, index, count, destination);
    }

    QString TrackRef::name() const {
        return get(*this, TrackSlots::Name);
    }

    void TrackRef::setName(const QString &name) const {
        set(*this, TrackSlots::Name, name);
    }

    QString TrackRef::voiceDir() const {
        return get(*this, TrackSlots::VoiceDir);
    }

    void TrackRef::setVoiceDir(const QString &voiceDir) const {
        set(*this, TrackSlots::VoiceDir, voiceDir);
    }

    NoteListRef TrackRef::notes() const {
        return child<NoteListRef>(*this, TrackSlots::Notes);
    }

    ProjectRef::ProjectRef(ProjectSession *session) : ProjectNodeRef(session, session->root()) {
    }

    SettingsRef ProjectRef::settings() const {
        return child<SettingsRef>(*this, ProjectSlots::Settings);
    }

    int ProjectRef::trackCount() const {
        return itemCount(tracksOf(*this));
    }

    TrackRef ProjectRef::track(int index) const {
        return TrackRef(session(), itemId(tracksOf(*this), index));
    }

}
