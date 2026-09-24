#ifndef HELLOKIT_EDIT_PROJECTREFS_H
#define HELLOKIT_EDIT_PROJECTREFS_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Note.h>

#include <hellokit/Edit/NodeRef.h>
#include <hellokit/Edit/ProjectSchema.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    // The handles of the nodes of a project tree. Each member function calls one function of
    // EditSession with the slot of ProjectSchema.h. See NodeRef.

    /// The base of the handles of a project tree, which refer to a ProjectSession.
    class ProjectNodeRef : public NodeRef {
    public:
        inline ProjectNodeRef() = default;

        inline ProjectNodeRef(ProjectSession *session, NodeId id) : NodeRef(session, id) {
        }

        inline ProjectSession *session() const {
            return static_cast<ProjectSession *>(m_session);
        }

    protected:
        template <class Ref>
        inline Ref child(ChildSlot slot) const {
            return Ref(session(), childId(slot));
        }

        template <class Ref>
        inline Ref item(int index) const {
            return Ref(session(), itemId(index));
        }
    };

    class SettingsRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline QString name() const {
            return get(SettingsSlots::Name);
        }

        inline void setName(const QString &name) const {
            set(SettingsSlots::Name, name);
        }

        inline double tempo() const {
            return get(SettingsSlots::Tempo);
        }

        inline void setTempo(double tempo) const {
            set(SettingsSlots::Tempo, tempo);
        }

        inline QString flags() const {
            return get(SettingsSlots::Flags);
        }

        inline void setFlags(const QString &flags) const {
            set(SettingsSlots::Flags, flags);
        }

        inline QString outputFile() const {
            return get(SettingsSlots::OutputFile);
        }

        inline void setOutputFile(const QString &outputFile) const {
            set(SettingsSlots::OutputFile, outputFile);
        }

        inline QString cacheDir() const {
            return get(SettingsSlots::CacheDir);
        }

        inline void setCacheDir(const QString &cacheDir) const {
            set(SettingsSlots::CacheDir, cacheDir);
        }

        /// \warning Untrusted, see \c ProjectSettings::wavtool.
        inline QString wavtool() const {
            return get(SettingsSlots::Wavtool);
        }

        inline void setWavtool(const QString &wavtool) const {
            set(SettingsSlots::Wavtool, wavtool);
        }

        /// \warning Untrusted, see \c ProjectSettings::resampler.
        inline QString resampler() const {
            return get(SettingsSlots::Resampler);
        }

        inline void setResampler(const QString &resampler) const {
            set(SettingsSlots::Resampler, resampler);
        }

        inline bool mode2() const {
            return get(SettingsSlots::Mode2);
        }

        inline void setMode2(bool mode2) const {
            set(SettingsSlots::Mode2, mode2);
        }
    };

    class PortamentoPointRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline double x() const {
            return get(PortamentoSlots::X);
        }

        inline void setX(double x) const {
            set(PortamentoSlots::X, x);
        }

        inline double y() const {
            return get(PortamentoSlots::Y);
        }

        inline void setY(double y) const {
            set(PortamentoSlots::Y, y);
        }

        inline PortamentoPoint::Type type() const {
            return get(PortamentoSlots::Type);
        }

        inline void setType(PortamentoPoint::Type type) const {
            set(PortamentoSlots::Type, type);
        }

        /// Returns a copy of the point.
        inline PortamentoPoint toPoint() const {
            return {x(), y(), type()};
        }
    };

    class PortamentoListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline int size() const {
            return count();
        }

        inline PortamentoPointRef at(int index) const {
            return item<PortamentoPointRef>(index);
        }

        /// Inserts copies of \a points before \a index.
        inline void insert(int index, const QList<PortamentoPoint> &points) const {
            session()->insert(m_id, index, points);
        }

        inline void remove(int index, int count) const {
            m_session->remove(m_id, index, count);
        }

        inline void move(int index, int count, int destination) const {
            m_session->move(m_id, index, count, destination);
        }
    };

    class PitchBendRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline std::optional<double> start() const {
            return get(PitchBendSlots::Start);
        }

        inline void setStart(std::optional<double> start) const {
            set(PitchBendSlots::Start, start);
        }

        inline int size() const {
            return m_session ? m_session->size(valuesId()) : 0;
        }

        inline QList<double> values() const {
            return m_session ? m_session->values(valuesId()) : QList<double>();
        }

        /// Overwrites the values starting at \a index, extending the curve if \a values reaches
        /// beyond its end.
        inline void replaceValues(int index, const QList<double> &values) const {
            m_session->replaceValues(valuesId(), index, values);
        }

        inline void insertValues(int index, const QList<double> &values) const {
            m_session->insertValues(valuesId(), index, values);
        }

        inline void removeValues(int index, int count) const {
            m_session->removeValues(valuesId(), index, count);
        }

    private:
        inline NodeId valuesId() const {
            return m_session->child(m_id, PitchBendSlots::Values);
        }
    };

    /// See \c Note::userData.
    class UserDataRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        /// Returns the keys in ascending order.
        inline QStringList keys() const {
            return m_session ? m_session->keys(m_id) : QStringList();
        }

        inline bool contains(const QString &key) const {
            return m_session && m_session->entry(m_id, key).isValid();
        }

        inline QString value(const QString &key) const {
            return m_session ? m_session->entry(m_id, key).toString() : QString();
        }

        inline void setValue(const QString &key, const QString &value) const {
            m_session->setEntry(m_id, key, value);
        }

        inline void remove(const QString &key) const {
            m_session->setEntry(m_id, key, QVariant());
        }
    };

    class NoteRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline QString lyric() const {
            return get(NoteSlots::Lyric);
        }

        inline void setLyric(const QString &lyric) const {
            set(NoteSlots::Lyric, lyric);
        }

        inline int length() const {
            return get(NoteSlots::Length);
        }

        inline void setLength(int length) const {
            set(NoteSlots::Length, length);
        }

        inline int noteNum() const {
            return get(NoteSlots::NoteNum);
        }

        inline void setNoteNum(int noteNum) const {
            set(NoteSlots::NoteNum, noteNum);
        }

        inline std::optional<double> intensity() const {
            return get(NoteSlots::Intensity);
        }

        inline void setIntensity(std::optional<double> intensity) const {
            set(NoteSlots::Intensity, intensity);
        }

        inline std::optional<double> modulation() const {
            return get(NoteSlots::Modulation);
        }

        inline void setModulation(std::optional<double> modulation) const {
            set(NoteSlots::Modulation, modulation);
        }

        inline std::optional<double> velocity() const {
            return get(NoteSlots::Velocity);
        }

        inline void setVelocity(std::optional<double> velocity) const {
            set(NoteSlots::Velocity, velocity);
        }

        inline std::optional<double> preUtterance() const {
            return get(NoteSlots::PreUtterance);
        }

        inline void setPreUtterance(std::optional<double> preUtterance) const {
            set(NoteSlots::PreUtterance, preUtterance);
        }

        inline std::optional<double> voiceOverlap() const {
            return get(NoteSlots::VoiceOverlap);
        }

        inline void setVoiceOverlap(std::optional<double> voiceOverlap) const {
            set(NoteSlots::VoiceOverlap, voiceOverlap);
        }

        inline std::optional<double> startPoint() const {
            return get(NoteSlots::StartPoint);
        }

        inline void setStartPoint(std::optional<double> startPoint) const {
            set(NoteSlots::StartPoint, startPoint);
        }

        inline std::optional<double> tempo() const {
            return get(NoteSlots::Tempo);
        }

        inline void setTempo(std::optional<double> tempo) const {
            set(NoteSlots::Tempo, tempo);
        }

        inline QString flags() const {
            return get(NoteSlots::Flags);
        }

        inline void setFlags(const QString &flags) const {
            set(NoteSlots::Flags, flags);
        }

        inline std::optional<Envelope> envelope() const {
            return get(NoteSlots::Envelope);
        }

        /// Replaces the envelope as a whole.
        inline void setEnvelope(const std::optional<Envelope> &envelope) const {
            set(NoteSlots::Envelope, envelope);
        }

        inline std::optional<Vibrato> vibrato() const {
            return get(NoteSlots::Vibrato);
        }

        /// Replaces the vibrato as a whole.
        inline void setVibrato(const std::optional<Vibrato> &vibrato) const {
            set(NoteSlots::Vibrato, vibrato);
        }

        inline PortamentoListRef portamento() const {
            return child<PortamentoListRef>(NoteSlots::Portamento);
        }

        /// Returns the Mode1 pitch curve, or an invalid handle if the note has none.
        inline PitchBendRef pitchBend() const {
            return child<PitchBendRef>(NoteSlots::PitchBend);
        }

        /// Replaces the Mode1 pitch curve, or removes it if \a pitchBend is empty.
        inline void setPitchBend(const std::optional<PitchBend> &pitchBend) const {
            session()->setPitchBend(m_id, pitchBend);
        }

        inline QString label() const {
            return get(NoteSlots::Label);
        }

        inline void setLabel(const QString &label) const {
            set(NoteSlots::Label, label);
        }

        inline QString direct() const {
            return get(NoteSlots::Direct);
        }

        inline void setDirect(const QString &direct) const {
            set(NoteSlots::Direct, direct);
        }

        /// \warning Untrusted, see \c Note::patch.
        inline QString patch() const {
            return get(NoteSlots::Patch);
        }

        inline void setPatch(const QString &patch) const {
            set(NoteSlots::Patch, patch);
        }

        inline QString region() const {
            return get(NoteSlots::Region);
        }

        inline void setRegion(const QString &region) const {
            set(NoteSlots::Region, region);
        }

        inline QString regionEnd() const {
            return get(NoteSlots::RegionEnd);
        }

        inline void setRegionEnd(const QString &regionEnd) const {
            set(NoteSlots::RegionEnd, regionEnd);
        }

        inline UserDataRef userData() const {
            return child<UserDataRef>(NoteSlots::UserData);
        }

        /// Returns a copy of the note, or a default note if the handle is invalid.
        inline Note toNote() const {
            return m_session ? session()->note(m_id) : Note();
        }
    };

    class NoteListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline int size() const {
            return count();
        }

        inline NoteRef at(int index) const {
            return item<NoteRef>(index);
        }

        /// Inserts copies of \a notes before \a index.
        inline void insert(int index, const QList<Note> &notes) const {
            session()->insert(m_id, index, notes);
        }

        inline void remove(int index, int count) const {
            m_session->remove(m_id, index, count);
        }

        /// Moves \a count notes starting at \a index so that the first of them is at
        /// \a destination afterwards. The notes keep their identifiers.
        inline void move(int index, int count, int destination) const {
            m_session->move(m_id, index, count, destination);
        }
    };

    class TrackRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline QString name() const {
            return get(TrackSlots::Name);
        }

        inline void setName(const QString &name) const {
            set(TrackSlots::Name, name);
        }

        inline QString voiceDir() const {
            return get(TrackSlots::VoiceDir);
        }

        inline void setVoiceDir(const QString &voiceDir) const {
            set(TrackSlots::VoiceDir, voiceDir);
        }

        inline NoteListRef notes() const {
            return child<NoteListRef>(TrackSlots::Notes);
        }
    };

    /// The handle of the root of a session, from which the other handles are obtained.
    class ProjectRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        inline explicit ProjectRef(ProjectSession *session)
            : ProjectNodeRef(session, session->root()) {
        }

        inline SettingsRef settings() const {
            return child<SettingsRef>(ProjectSlots::Settings);
        }

        inline int trackCount() const {
            return m_session->size(tracksId());
        }

        inline TrackRef track(int index) const {
            return TrackRef(session(), m_session->at(tracksId(), index));
        }

    private:
        inline NodeId tracksId() const {
            return m_session->child(m_id, ProjectSlots::Tracks);
        }
    };

}

#endif // HELLOKIT_EDIT_PROJECTREFS_H
