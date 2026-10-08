#include "ProjectRefs.h"

#include <hellokit/EditBase/private/NodeAccess_p.h>

#include "ProjectTree_p.h"

namespace hello::kit {

    template <>
    struct edit::NodeOf<ProjectRef> : edit::NodeTraits<ProjectNode, ProjectType> {};
    template <>
    struct edit::NodeOf<SettingsRef> : edit::NodeTraits<SettingsNode, SettingsType> {};
    template <>
    struct edit::NodeOf<TrackListRef> : edit::ListTraits<TrackType> {};
    template <>
    struct edit::NodeOf<TrackRef> : edit::NodeTraits<TrackNode, TrackType> {};
    template <>
    struct edit::NodeOf<NoteListRef> : edit::ListTraits<NoteType> {};
    template <>
    struct edit::NodeOf<NoteRef> : edit::NodeTraits<NoteNode, NoteType> {};
    template <>
    struct edit::NodeOf<PortamentoListRef> : edit::ListTraits<PortamentoPointType> {};
    template <>
    struct edit::NodeOf<PortamentoPointRef>
        : edit::NodeTraits<PortamentoPointNode, PortamentoPointType> {};
    template <>
    struct edit::NodeOf<PitchBendRef> : edit::NodeTraits<PitchBendNode, PitchBendType> {};
    template <>
    struct edit::NodeOf<UserDataRef> : edit::MappingTraits {};
    template <>
    struct edit::NodeOf<UnknownFieldsRef> : edit::MappingTraits {};

    // ProjectRef

    ProjectRef::ProjectRef(ProjectSession *session) : ProjectNodeRef(session, session->root()) {
    }

    SettingsRef ProjectRef::settings() const {
        return edit::NodeAccess::child<SettingsRef>(*this, ProjectSlots::Settings);
    }

    TrackListRef ProjectRef::tracks() const {
        return edit::NodeAccess::child<TrackListRef>(*this, ProjectSlots::Tracks);
    }

    UnknownFieldsRef ProjectRef::unknownFields() const {
        return edit::NodeAccess::child<UnknownFieldsRef>(*this, ProjectSlots::UnknownFields);
    }

    Project ProjectRef::toProject() const {
        return edit::NodeAccess::toValue<Project>(*this);
    }

    // SettingsRef

    QString SettingsRef::name() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Name);
    }

    void SettingsRef::setName(const QString &name) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Name, name);
    }

    double SettingsRef::tempo() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Tempo);
    }

    void SettingsRef::setTempo(double tempo) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Tempo, tempo);
    }

    QString SettingsRef::flags() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Flags);
    }

    void SettingsRef::setFlags(const QString &flags) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Flags, flags);
    }

    QString SettingsRef::outputFile() const {
        return edit::NodeAccess::value(*this, SettingsSlots::OutputFile);
    }

    void SettingsRef::setOutputFile(const QString &outputFile) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::OutputFile, outputFile);
    }

    QString SettingsRef::cacheDir() const {
        return edit::NodeAccess::value(*this, SettingsSlots::CacheDir);
    }

    void SettingsRef::setCacheDir(const QString &cacheDir) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::CacheDir, cacheDir);
    }

    QString SettingsRef::wavtool() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Wavtool);
    }

    void SettingsRef::setWavtool(const QString &wavtool) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Wavtool, wavtool);
    }

    QString SettingsRef::resampler() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Resampler);
    }

    void SettingsRef::setResampler(const QString &resampler) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Resampler, resampler);
    }

    bool SettingsRef::mode2() const {
        return edit::NodeAccess::value(*this, SettingsSlots::Mode2);
    }

    void SettingsRef::setMode2(bool mode2) const {
        edit::NodeAccess::setValue(*this, SettingsSlots::Mode2, mode2);
    }

    ProjectSettings SettingsRef::toProjectSettings() const {
        return edit::NodeAccess::toValue<ProjectSettings>(*this);
    }

    // TrackListRef

    int TrackListRef::size() const {
        return edit::NodeAccess::size(*this);
    }

    TrackRef TrackListRef::at(int index) const {
        return edit::NodeAccess::at<TrackRef>(*this, index);
    }

    void TrackListRef::insert(int index, const QList<Track> &tracks) const {
        edit::NodeAccess::insert(*this, index, tracks);
    }

    void TrackListRef::remove(int index, int count) const {
        edit::NodeAccess::remove(*this, index, count);
    }

    void TrackListRef::move(int index, int count, int destination) const {
        edit::NodeAccess::move(*this, index, count, destination);
    }

    // TrackRef

    QString TrackRef::name() const {
        return edit::NodeAccess::value(*this, TrackSlots::Name);
    }

    void TrackRef::setName(const QString &name) const {
        edit::NodeAccess::setValue(*this, TrackSlots::Name, name);
    }

    QString TrackRef::voiceDir() const {
        return edit::NodeAccess::value(*this, TrackSlots::VoiceDir);
    }

    void TrackRef::setVoiceDir(const QString &voiceDir) const {
        edit::NodeAccess::setValue(*this, TrackSlots::VoiceDir, voiceDir);
    }

    NoteListRef TrackRef::notes() const {
        return edit::NodeAccess::child<NoteListRef>(*this, TrackSlots::Notes);
    }

    Track TrackRef::toTrack() const {
        return edit::NodeAccess::toValue<Track>(*this);
    }

    // NoteListRef

    int NoteListRef::size() const {
        return edit::NodeAccess::size(*this);
    }

    NoteRef NoteListRef::at(int index) const {
        return edit::NodeAccess::at<NoteRef>(*this, index);
    }

    void NoteListRef::insert(int index, const QList<Note> &notes) const {
        edit::NodeAccess::insert(*this, index, notes);
    }

    void NoteListRef::remove(int index, int count) const {
        edit::NodeAccess::remove(*this, index, count);
    }

    void NoteListRef::move(int index, int count, int destination) const {
        edit::NodeAccess::move(*this, index, count, destination);
    }

    QList<Region> NoteListRef::regions() const {
        QList<QStringList> starts;
        QList<QStringList> ends;
        for (int i = 0; i < size(); ++i) {
            const auto note = at(i);
            starts.push_back(note.regions());
            ends.push_back(note.regionEnds());
        }
        return Region::of(starts, ends);
    }

    // NoteRef

    QString NoteRef::lyric() const {
        return edit::NodeAccess::value(*this, NoteSlots::Lyric);
    }

    void NoteRef::setLyric(const QString &lyric) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Lyric, lyric);
    }

    int NoteRef::length() const {
        return edit::NodeAccess::value(*this, NoteSlots::Length);
    }

    void NoteRef::setLength(int length) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Length, length);
    }

    int NoteRef::noteNum() const {
        return edit::NodeAccess::value(*this, NoteSlots::NoteNum);
    }

    void NoteRef::setNoteNum(int noteNum) const {
        edit::NodeAccess::setValue(*this, NoteSlots::NoteNum, noteNum);
    }

    std::optional<double> NoteRef::intensity() const {
        return edit::NodeAccess::value(*this, NoteSlots::Intensity);
    }

    void NoteRef::setIntensity(std::optional<double> intensity) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Intensity, intensity);
    }

    std::optional<double> NoteRef::modulation() const {
        return edit::NodeAccess::value(*this, NoteSlots::Modulation);
    }

    void NoteRef::setModulation(std::optional<double> modulation) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Modulation, modulation);
    }

    std::optional<double> NoteRef::velocity() const {
        return edit::NodeAccess::value(*this, NoteSlots::Velocity);
    }

    void NoteRef::setVelocity(std::optional<double> velocity) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Velocity, velocity);
    }

    std::optional<double> NoteRef::preUtterance() const {
        return edit::NodeAccess::value(*this, NoteSlots::PreUtterance);
    }

    void NoteRef::setPreUtterance(std::optional<double> preUtterance) const {
        edit::NodeAccess::setValue(*this, NoteSlots::PreUtterance, preUtterance);
    }

    std::optional<double> NoteRef::voiceOverlap() const {
        return edit::NodeAccess::value(*this, NoteSlots::VoiceOverlap);
    }

    void NoteRef::setVoiceOverlap(std::optional<double> voiceOverlap) const {
        edit::NodeAccess::setValue(*this, NoteSlots::VoiceOverlap, voiceOverlap);
    }

    std::optional<double> NoteRef::startPoint() const {
        return edit::NodeAccess::value(*this, NoteSlots::StartPoint);
    }

    void NoteRef::setStartPoint(std::optional<double> startPoint) const {
        edit::NodeAccess::setValue(*this, NoteSlots::StartPoint, startPoint);
    }

    std::optional<double> NoteRef::tempo() const {
        return edit::NodeAccess::value(*this, NoteSlots::Tempo);
    }

    void NoteRef::setTempo(std::optional<double> tempo) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Tempo, tempo);
    }

    QString NoteRef::flags() const {
        return edit::NodeAccess::value(*this, NoteSlots::Flags);
    }

    void NoteRef::setFlags(const QString &flags) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Flags, flags);
    }

    std::optional<Envelope> NoteRef::envelope() const {
        return edit::NodeAccess::value(*this, NoteSlots::Envelope);
    }

    void NoteRef::setEnvelope(const std::optional<Envelope> &envelope) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Envelope, envelope);
    }

    std::optional<Vibrato> NoteRef::vibrato() const {
        return edit::NodeAccess::value(*this, NoteSlots::Vibrato);
    }

    void NoteRef::setVibrato(const std::optional<Vibrato> &vibrato) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Vibrato, vibrato);
    }

    PortamentoListRef NoteRef::portamento() const {
        return edit::NodeAccess::child<PortamentoListRef>(*this, NoteSlots::Portamento);
    }

    PitchBendRef NoteRef::pitchBend() const {
        return edit::NodeAccess::child<PitchBendRef>(*this, NoteSlots::PitchBend);
    }

    void NoteRef::setPitchBend(const std::optional<PitchBend> &pitchBend) const {
        edit::NodeAccess::setChild(*this, NoteSlots::PitchBend, pitchBend);
    }

    QString NoteRef::label() const {
        return edit::NodeAccess::value(*this, NoteSlots::Label);
    }

    void NoteRef::setLabel(const QString &label) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Label, label);
    }

    QString NoteRef::direct() const {
        return edit::NodeAccess::value(*this, NoteSlots::Direct);
    }

    void NoteRef::setDirect(const QString &direct) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Direct, direct);
    }

    QString NoteRef::patch() const {
        return edit::NodeAccess::value(*this, NoteSlots::Patch);
    }

    void NoteRef::setPatch(const QString &patch) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Patch, patch);
    }

    QStringList NoteRef::regions() const {
        return edit::NodeAccess::value(*this, NoteSlots::Regions);
    }

    void NoteRef::setRegions(const QStringList &regions) const {
        edit::NodeAccess::setValue(*this, NoteSlots::Regions, regions);
    }

    QStringList NoteRef::regionEnds() const {
        return edit::NodeAccess::value(*this, NoteSlots::RegionEnds);
    }

    void NoteRef::setRegionEnds(const QStringList &regionEnds) const {
        edit::NodeAccess::setValue(*this, NoteSlots::RegionEnds, regionEnds);
    }

    UserDataRef NoteRef::userData() const {
        return edit::NodeAccess::child<UserDataRef>(*this, NoteSlots::UserData);
    }

    Note NoteRef::toNote() const {
        return edit::NodeAccess::toValue<Note>(*this);
    }

    // PortamentoListRef

    int PortamentoListRef::size() const {
        return edit::NodeAccess::size(*this);
    }

    PortamentoPointRef PortamentoListRef::at(int index) const {
        return edit::NodeAccess::at<PortamentoPointRef>(*this, index);
    }

    void PortamentoListRef::insert(int index, const QList<PortamentoPoint> &points) const {
        edit::NodeAccess::insert(*this, index, points);
    }

    void PortamentoListRef::remove(int index, int count) const {
        edit::NodeAccess::remove(*this, index, count);
    }

    void PortamentoListRef::move(int index, int count, int destination) const {
        edit::NodeAccess::move(*this, index, count, destination);
    }

    // PortamentoPointRef

    double PortamentoPointRef::x() const {
        return edit::NodeAccess::value(*this, PortamentoSlots::X);
    }

    void PortamentoPointRef::setX(double x) const {
        edit::NodeAccess::setValue(*this, PortamentoSlots::X, x);
    }

    double PortamentoPointRef::y() const {
        return edit::NodeAccess::value(*this, PortamentoSlots::Y);
    }

    void PortamentoPointRef::setY(double y) const {
        edit::NodeAccess::setValue(*this, PortamentoSlots::Y, y);
    }

    PortamentoPoint::Type PortamentoPointRef::type() const {
        return edit::NodeAccess::value(*this, PortamentoSlots::Type);
    }

    void PortamentoPointRef::setType(PortamentoPoint::Type type) const {
        edit::NodeAccess::setValue(*this, PortamentoSlots::Type, type);
    }

    PortamentoPoint PortamentoPointRef::toPortamentoPoint() const {
        return edit::NodeAccess::toValue<PortamentoPoint>(*this);
    }

    // PitchBendRef

    std::optional<double> PitchBendRef::start() const {
        return edit::NodeAccess::value(*this, PitchBendSlots::Start);
    }

    void PitchBendRef::setStart(std::optional<double> start) const {
        edit::NodeAccess::setValue(*this, PitchBendSlots::Start, start);
    }

    QList<double> PitchBendRef::values() const {
        return edit::NodeAccess::arrayValues<double>(*this, PitchBendSlots::Values,
                                                     PitchValuesType);
    }

    int PitchBendRef::valuesSize() const {
        return edit::NodeAccess::arraySize<double>(*this, PitchBendSlots::Values, PitchValuesType);
    }

    void PitchBendRef::replaceValues(int index, const QList<double> &values) const {
        edit::NodeAccess::replaceArray(*this, PitchBendSlots::Values, PitchValuesType, index,
                                       values);
    }

    void PitchBendRef::insertValues(int index, const QList<double> &values) const {
        edit::NodeAccess::insertArray(*this, PitchBendSlots::Values, PitchValuesType, index,
                                      values);
    }

    void PitchBendRef::removeValues(int index, int count) const {
        edit::NodeAccess::removeArray<double>(*this, PitchBendSlots::Values, PitchValuesType, index,
                                              count);
    }

    PitchBend PitchBendRef::toPitchBend() const {
        return edit::NodeAccess::toValue<PitchBend>(*this);
    }

    // UserDataRef

    QStringList UserDataRef::keys() const {
        return edit::NodeAccess::keys(*this);
    }

    bool UserDataRef::contains(const QString &key) const {
        return edit::NodeAccess::contains(*this, key);
    }

    QString UserDataRef::value(const QString &key) const {
        return edit::NodeAccess::entry<QString>(*this, key);
    }

    void UserDataRef::setValue(const QString &key, const QString &value) const {
        edit::NodeAccess::setEntry(*this, key, value);
    }

    void UserDataRef::remove(const QString &key) const {
        edit::NodeAccess::removeEntry(*this, key);
    }

    // UnknownFieldsRef

    QStringList UnknownFieldsRef::keys() const {
        return edit::NodeAccess::keys(*this);
    }

    bool UnknownFieldsRef::contains(const QString &key) const {
        return edit::NodeAccess::contains(*this, key);
    }

    QJsonValue UnknownFieldsRef::value(const QString &key) const {
        return edit::NodeAccess::entry<QJsonValue>(*this, key);
    }

    void UnknownFieldsRef::setValue(const QString &key, const QJsonValue &value) const {
        edit::NodeAccess::setEntry(*this, key, value);
    }

    void UnknownFieldsRef::remove(const QString &key) const {
        edit::NodeAccess::removeEntry(*this, key);
    }

}
