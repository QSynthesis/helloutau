#include "ProjectRefs.h"

#include "NodeAccess_p.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    template <>
    struct NodeOf<ProjectRef> : NodeTraits<ProjectNode, ProjectType> {};
    template <>
    struct NodeOf<SettingsRef> : NodeTraits<SettingsNode, SettingsType> {};
    template <>
    struct NodeOf<TrackListRef> : ListTraits<TrackType> {};
    template <>
    struct NodeOf<TrackRef> : NodeTraits<TrackNode, TrackType> {};
    template <>
    struct NodeOf<NoteListRef> : ListTraits<NoteType> {};
    template <>
    struct NodeOf<NoteRef> : NodeTraits<NoteNode, NoteType> {};
    template <>
    struct NodeOf<PortamentoListRef> : ListTraits<PortamentoPointType> {};
    template <>
    struct NodeOf<PortamentoPointRef> : NodeTraits<PortamentoPointNode, PortamentoPointType> {};
    template <>
    struct NodeOf<PitchBendRef> : NodeTraits<PitchBendNode, PitchBendType> {};
    template <>
    struct NodeOf<UserDataRef> : MappingTraits {};
    template <>
    struct NodeOf<UnknownFieldsRef> : MappingTraits {};

    // ProjectRef

    ProjectRef::ProjectRef(ProjectSession *session) : ProjectNodeRef(session, session->root()) {
    }

    SettingsRef ProjectRef::settings() const {
        return NodeAccess::child<SettingsRef>(*this, ProjectSlots::Settings);
    }

    TrackListRef ProjectRef::tracks() const {
        return NodeAccess::child<TrackListRef>(*this, ProjectSlots::Tracks);
    }

    UnknownFieldsRef ProjectRef::unknownFields() const {
        return NodeAccess::child<UnknownFieldsRef>(*this, ProjectSlots::UnknownFields);
    }

    Project ProjectRef::toProject() const {
        return NodeAccess::toValue<Project>(*this);
    }

    // SettingsRef

    QString SettingsRef::name() const {
        return NodeAccess::value(*this, SettingsSlots::Name);
    }

    void SettingsRef::setName(const QString &name) const {
        NodeAccess::setValue(*this, SettingsSlots::Name, name);
    }

    double SettingsRef::tempo() const {
        return NodeAccess::value(*this, SettingsSlots::Tempo);
    }

    void SettingsRef::setTempo(double tempo) const {
        NodeAccess::setValue(*this, SettingsSlots::Tempo, tempo);
    }

    QString SettingsRef::flags() const {
        return NodeAccess::value(*this, SettingsSlots::Flags);
    }

    void SettingsRef::setFlags(const QString &flags) const {
        NodeAccess::setValue(*this, SettingsSlots::Flags, flags);
    }

    QString SettingsRef::outputFile() const {
        return NodeAccess::value(*this, SettingsSlots::OutputFile);
    }

    void SettingsRef::setOutputFile(const QString &outputFile) const {
        NodeAccess::setValue(*this, SettingsSlots::OutputFile, outputFile);
    }

    QString SettingsRef::cacheDir() const {
        return NodeAccess::value(*this, SettingsSlots::CacheDir);
    }

    void SettingsRef::setCacheDir(const QString &cacheDir) const {
        NodeAccess::setValue(*this, SettingsSlots::CacheDir, cacheDir);
    }

    QString SettingsRef::wavtool() const {
        return NodeAccess::value(*this, SettingsSlots::Wavtool);
    }

    void SettingsRef::setWavtool(const QString &wavtool) const {
        NodeAccess::setValue(*this, SettingsSlots::Wavtool, wavtool);
    }

    QString SettingsRef::resampler() const {
        return NodeAccess::value(*this, SettingsSlots::Resampler);
    }

    void SettingsRef::setResampler(const QString &resampler) const {
        NodeAccess::setValue(*this, SettingsSlots::Resampler, resampler);
    }

    bool SettingsRef::mode2() const {
        return NodeAccess::value(*this, SettingsSlots::Mode2);
    }

    void SettingsRef::setMode2(bool mode2) const {
        NodeAccess::setValue(*this, SettingsSlots::Mode2, mode2);
    }

    ProjectSettings SettingsRef::toProjectSettings() const {
        return NodeAccess::toValue<ProjectSettings>(*this);
    }

    // TrackListRef

    int TrackListRef::size() const {
        return NodeAccess::size(*this);
    }

    TrackRef TrackListRef::at(int index) const {
        return NodeAccess::at<TrackRef>(*this, index);
    }

    void TrackListRef::insert(int index, const QList<Track> &tracks) const {
        NodeAccess::insert(*this, index, tracks);
    }

    void TrackListRef::remove(int index, int count) const {
        NodeAccess::remove(*this, index, count);
    }

    void TrackListRef::move(int index, int count, int destination) const {
        NodeAccess::move(*this, index, count, destination);
    }

    // TrackRef

    QString TrackRef::name() const {
        return NodeAccess::value(*this, TrackSlots::Name);
    }

    void TrackRef::setName(const QString &name) const {
        NodeAccess::setValue(*this, TrackSlots::Name, name);
    }

    QString TrackRef::voiceDir() const {
        return NodeAccess::value(*this, TrackSlots::VoiceDir);
    }

    void TrackRef::setVoiceDir(const QString &voiceDir) const {
        NodeAccess::setValue(*this, TrackSlots::VoiceDir, voiceDir);
    }

    NoteListRef TrackRef::notes() const {
        return NodeAccess::child<NoteListRef>(*this, TrackSlots::Notes);
    }

    Track TrackRef::toTrack() const {
        return NodeAccess::toValue<Track>(*this);
    }

    // NoteListRef

    int NoteListRef::size() const {
        return NodeAccess::size(*this);
    }

    NoteRef NoteListRef::at(int index) const {
        return NodeAccess::at<NoteRef>(*this, index);
    }

    void NoteListRef::insert(int index, const QList<Note> &notes) const {
        NodeAccess::insert(*this, index, notes);
    }

    void NoteListRef::remove(int index, int count) const {
        NodeAccess::remove(*this, index, count);
    }

    void NoteListRef::move(int index, int count, int destination) const {
        NodeAccess::move(*this, index, count, destination);
    }

    // NoteRef

    QString NoteRef::lyric() const {
        return NodeAccess::value(*this, NoteSlots::Lyric);
    }

    void NoteRef::setLyric(const QString &lyric) const {
        NodeAccess::setValue(*this, NoteSlots::Lyric, lyric);
    }

    int NoteRef::length() const {
        return NodeAccess::value(*this, NoteSlots::Length);
    }

    void NoteRef::setLength(int length) const {
        NodeAccess::setValue(*this, NoteSlots::Length, length);
    }

    int NoteRef::noteNum() const {
        return NodeAccess::value(*this, NoteSlots::NoteNum);
    }

    void NoteRef::setNoteNum(int noteNum) const {
        NodeAccess::setValue(*this, NoteSlots::NoteNum, noteNum);
    }

    std::optional<double> NoteRef::intensity() const {
        return NodeAccess::value(*this, NoteSlots::Intensity);
    }

    void NoteRef::setIntensity(std::optional<double> intensity) const {
        NodeAccess::setValue(*this, NoteSlots::Intensity, intensity);
    }

    std::optional<double> NoteRef::modulation() const {
        return NodeAccess::value(*this, NoteSlots::Modulation);
    }

    void NoteRef::setModulation(std::optional<double> modulation) const {
        NodeAccess::setValue(*this, NoteSlots::Modulation, modulation);
    }

    std::optional<double> NoteRef::velocity() const {
        return NodeAccess::value(*this, NoteSlots::Velocity);
    }

    void NoteRef::setVelocity(std::optional<double> velocity) const {
        NodeAccess::setValue(*this, NoteSlots::Velocity, velocity);
    }

    std::optional<double> NoteRef::preUtterance() const {
        return NodeAccess::value(*this, NoteSlots::PreUtterance);
    }

    void NoteRef::setPreUtterance(std::optional<double> preUtterance) const {
        NodeAccess::setValue(*this, NoteSlots::PreUtterance, preUtterance);
    }

    std::optional<double> NoteRef::voiceOverlap() const {
        return NodeAccess::value(*this, NoteSlots::VoiceOverlap);
    }

    void NoteRef::setVoiceOverlap(std::optional<double> voiceOverlap) const {
        NodeAccess::setValue(*this, NoteSlots::VoiceOverlap, voiceOverlap);
    }

    std::optional<double> NoteRef::startPoint() const {
        return NodeAccess::value(*this, NoteSlots::StartPoint);
    }

    void NoteRef::setStartPoint(std::optional<double> startPoint) const {
        NodeAccess::setValue(*this, NoteSlots::StartPoint, startPoint);
    }

    std::optional<double> NoteRef::tempo() const {
        return NodeAccess::value(*this, NoteSlots::Tempo);
    }

    void NoteRef::setTempo(std::optional<double> tempo) const {
        NodeAccess::setValue(*this, NoteSlots::Tempo, tempo);
    }

    QString NoteRef::flags() const {
        return NodeAccess::value(*this, NoteSlots::Flags);
    }

    void NoteRef::setFlags(const QString &flags) const {
        NodeAccess::setValue(*this, NoteSlots::Flags, flags);
    }

    std::optional<Envelope> NoteRef::envelope() const {
        return NodeAccess::value(*this, NoteSlots::Envelope);
    }

    void NoteRef::setEnvelope(const std::optional<Envelope> &envelope) const {
        NodeAccess::setValue(*this, NoteSlots::Envelope, envelope);
    }

    std::optional<Vibrato> NoteRef::vibrato() const {
        return NodeAccess::value(*this, NoteSlots::Vibrato);
    }

    void NoteRef::setVibrato(const std::optional<Vibrato> &vibrato) const {
        NodeAccess::setValue(*this, NoteSlots::Vibrato, vibrato);
    }

    PortamentoListRef NoteRef::portamento() const {
        return NodeAccess::child<PortamentoListRef>(*this, NoteSlots::Portamento);
    }

    PitchBendRef NoteRef::pitchBend() const {
        return NodeAccess::child<PitchBendRef>(*this, NoteSlots::PitchBend);
    }

    void NoteRef::setPitchBend(const std::optional<PitchBend> &pitchBend) const {
        NodeAccess::setChild(*this, NoteSlots::PitchBend, pitchBend);
    }

    QString NoteRef::label() const {
        return NodeAccess::value(*this, NoteSlots::Label);
    }

    void NoteRef::setLabel(const QString &label) const {
        NodeAccess::setValue(*this, NoteSlots::Label, label);
    }

    QString NoteRef::direct() const {
        return NodeAccess::value(*this, NoteSlots::Direct);
    }

    void NoteRef::setDirect(const QString &direct) const {
        NodeAccess::setValue(*this, NoteSlots::Direct, direct);
    }

    QString NoteRef::patch() const {
        return NodeAccess::value(*this, NoteSlots::Patch);
    }

    void NoteRef::setPatch(const QString &patch) const {
        NodeAccess::setValue(*this, NoteSlots::Patch, patch);
    }

    QString NoteRef::region() const {
        return NodeAccess::value(*this, NoteSlots::Region);
    }

    void NoteRef::setRegion(const QString &region) const {
        NodeAccess::setValue(*this, NoteSlots::Region, region);
    }

    QString NoteRef::regionEnd() const {
        return NodeAccess::value(*this, NoteSlots::RegionEnd);
    }

    void NoteRef::setRegionEnd(const QString &regionEnd) const {
        NodeAccess::setValue(*this, NoteSlots::RegionEnd, regionEnd);
    }

    UserDataRef NoteRef::userData() const {
        return NodeAccess::child<UserDataRef>(*this, NoteSlots::UserData);
    }

    Note NoteRef::toNote() const {
        return NodeAccess::toValue<Note>(*this);
    }

    // PortamentoListRef

    int PortamentoListRef::size() const {
        return NodeAccess::size(*this);
    }

    PortamentoPointRef PortamentoListRef::at(int index) const {
        return NodeAccess::at<PortamentoPointRef>(*this, index);
    }

    void PortamentoListRef::insert(int index, const QList<PortamentoPoint> &points) const {
        NodeAccess::insert(*this, index, points);
    }

    void PortamentoListRef::remove(int index, int count) const {
        NodeAccess::remove(*this, index, count);
    }

    void PortamentoListRef::move(int index, int count, int destination) const {
        NodeAccess::move(*this, index, count, destination);
    }

    // PortamentoPointRef

    double PortamentoPointRef::x() const {
        return NodeAccess::value(*this, PortamentoSlots::X);
    }

    void PortamentoPointRef::setX(double x) const {
        NodeAccess::setValue(*this, PortamentoSlots::X, x);
    }

    double PortamentoPointRef::y() const {
        return NodeAccess::value(*this, PortamentoSlots::Y);
    }

    void PortamentoPointRef::setY(double y) const {
        NodeAccess::setValue(*this, PortamentoSlots::Y, y);
    }

    PortamentoPoint::Type PortamentoPointRef::type() const {
        return NodeAccess::value(*this, PortamentoSlots::Type);
    }

    void PortamentoPointRef::setType(PortamentoPoint::Type type) const {
        NodeAccess::setValue(*this, PortamentoSlots::Type, type);
    }

    PortamentoPoint PortamentoPointRef::toPortamentoPoint() const {
        return NodeAccess::toValue<PortamentoPoint>(*this);
    }

    // PitchBendRef

    std::optional<double> PitchBendRef::start() const {
        return NodeAccess::value(*this, PitchBendSlots::Start);
    }

    void PitchBendRef::setStart(std::optional<double> start) const {
        NodeAccess::setValue(*this, PitchBendSlots::Start, start);
    }

    QList<double> PitchBendRef::values() const {
        return NodeAccess::arrayValues<double>(*this, PitchBendSlots::Values, PitchValuesType);
    }

    int PitchBendRef::valuesSize() const {
        return NodeAccess::arraySize<double>(*this, PitchBendSlots::Values, PitchValuesType);
    }

    void PitchBendRef::replaceValues(int index, const QList<double> &values) const {
        NodeAccess::replaceArray(*this, PitchBendSlots::Values, PitchValuesType, index, values);
    }

    void PitchBendRef::insertValues(int index, const QList<double> &values) const {
        NodeAccess::insertArray(*this, PitchBendSlots::Values, PitchValuesType, index, values);
    }

    void PitchBendRef::removeValues(int index, int count) const {
        NodeAccess::removeArray<double>(*this, PitchBendSlots::Values, PitchValuesType, index,
                                        count);
    }

    PitchBend PitchBendRef::toPitchBend() const {
        return NodeAccess::toValue<PitchBend>(*this);
    }

    // UserDataRef

    QStringList UserDataRef::keys() const {
        return NodeAccess::keys(*this);
    }

    bool UserDataRef::contains(const QString &key) const {
        return NodeAccess::contains(*this, key);
    }

    QString UserDataRef::value(const QString &key) const {
        return NodeAccess::entry<QString>(*this, key);
    }

    void UserDataRef::setValue(const QString &key, const QString &value) const {
        NodeAccess::setEntry(*this, key, value);
    }

    void UserDataRef::remove(const QString &key) const {
        NodeAccess::removeEntry(*this, key);
    }

    // UnknownFieldsRef

    QStringList UnknownFieldsRef::keys() const {
        return NodeAccess::keys(*this);
    }

    bool UnknownFieldsRef::contains(const QString &key) const {
        return NodeAccess::contains(*this, key);
    }

    QJsonValue UnknownFieldsRef::value(const QString &key) const {
        return NodeAccess::entry<QJsonValue>(*this, key);
    }

    void UnknownFieldsRef::setValue(const QString &key, const QJsonValue &value) const {
        NodeAccess::setEntry(*this, key, value);
    }

    void UnknownFieldsRef::remove(const QString &key) const {
        NodeAccess::removeEntry(*this, key);
    }

}
