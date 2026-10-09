#ifndef HELLOKIT_EDIT_PROJECTREFS_H
#define HELLOKIT_EDIT_PROJECTREFS_H

#include <optional>

#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>

#include <hellokit/EditBase/NodeRef.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectSchema.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    // The handles of the nodes of a project tree, see NodeRef. The member functions follow one
    // convention per kind of field of ProjectSchema.h:
    //
    // - value field f: f() and setF()
    // - record field r: r(), and setR() with a std::optional value if the record is optional
    // - list field l: l(), a list handle with size(), at(), insert(), remove() and move()
    // - mapping field m: m(), a mapping handle with keys(), contains(), value(), setValue() and
    //   remove()
    // - array field a: a(), aSize(), replaceA(), insertA() and removeA()
    //
    // Each record handle also provides a copy of its record as toT(), where T is the name of the
    // record type.

    /// The base of the handles of a project tree, which refer to a ProjectSession.
    class HELLOKIT_EDIT_EXPORT ProjectNodeRef : public edit::NodeRef {
    public:
        inline ProjectNodeRef() = default;

        inline ProjectNodeRef(ProjectSession *session, edit::NodeId id)
            : edit::NodeRef(session, id) {
        }

        inline ProjectSession *session() const {
            return static_cast<ProjectSession *>(m_session);
        }
    };

    class HELLOKIT_EDIT_EXPORT SettingsRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QString name() const;
        void setName(const QString &name) const;

        double tempo() const;
        void setTempo(double tempo) const;

        QString flags() const;
        void setFlags(const QString &flags) const;

        QString outputFile() const;
        void setOutputFile(const QString &outputFile) const;

        QString cacheDir() const;
        void setCacheDir(const QString &cacheDir) const;

        /// \warning Untrusted.
        /// \sa ProjectSettings::wavtool
        QString wavtool() const;
        void setWavtool(const QString &wavtool) const;

        /// \warning Untrusted.
        /// \sa ProjectSettings::resampler
        QString resampler() const;
        void setResampler(const QString &resampler) const;

        bool mode2() const;
        void setMode2(bool mode2) const;

        TimeSignature timeSignature() const;
        void setTimeSignature(const TimeSignature &timeSignature) const;

        ProjectSettings toProjectSettings() const;
    };

    class HELLOKIT_EDIT_EXPORT PortamentoPointRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        double x() const;
        void setX(double x) const;

        double y() const;
        void setY(double y) const;

        PortamentoPoint::Type type() const;
        void setType(PortamentoPoint::Type type) const;

        PortamentoPoint toPortamentoPoint() const;
    };

    class HELLOKIT_EDIT_EXPORT PortamentoListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        int size() const;
        PortamentoPointRef at(int index) const;
        void insert(int index, const QList<PortamentoPoint> &points) const;
        void remove(int index, int count) const;
        void move(int index, int count, int destination) const;
    };

    class HELLOKIT_EDIT_EXPORT PitchBendRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        std::optional<double> start() const;
        void setStart(std::optional<double> start) const;

        QList<double> values() const;
        int valuesSize() const;
        void replaceValues(int index, const QList<double> &values) const;
        void insertValues(int index, const QList<double> &values) const;
        void removeValues(int index, int count) const;

        PitchBend toPitchBend() const;
    };

    /// \sa Note::userData
    class HELLOKIT_EDIT_EXPORT UserDataRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QStringList keys() const;
        bool contains(const QString &key) const;
        QString value(const QString &key) const;
        void setValue(const QString &key, const QString &value) const;
        void remove(const QString &key) const;
    };

    class HELLOKIT_EDIT_EXPORT NoteRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QString lyric() const;
        void setLyric(const QString &lyric) const;

        int length() const;
        void setLength(int length) const;

        int noteNum() const;
        void setNoteNum(int noteNum) const;

        std::optional<double> intensity() const;
        void setIntensity(std::optional<double> intensity) const;

        std::optional<double> modulation() const;
        void setModulation(std::optional<double> modulation) const;

        std::optional<double> velocity() const;
        void setVelocity(std::optional<double> velocity) const;

        std::optional<double> preUtterance() const;
        void setPreUtterance(std::optional<double> preUtterance) const;

        std::optional<double> voiceOverlap() const;
        void setVoiceOverlap(std::optional<double> voiceOverlap) const;

        std::optional<double> startPoint() const;
        void setStartPoint(std::optional<double> startPoint) const;

        std::optional<double> tempo() const;
        void setTempo(std::optional<double> tempo) const;

        QString flags() const;
        void setFlags(const QString &flags) const;

        std::optional<Envelope> envelope() const;
        void setEnvelope(const std::optional<Envelope> &envelope) const;

        std::optional<Vibrato> vibrato() const;
        void setVibrato(const std::optional<Vibrato> &vibrato) const;

        PortamentoListRef portamento() const;

        /// Returns an invalid handle if the note has no Mode1 pitch curve.
        PitchBendRef pitchBend() const;
        void setPitchBend(const std::optional<PitchBend> &pitchBend) const;

        QString label() const;
        void setLabel(const QString &label) const;

        QString direct() const;
        void setDirect(const QString &direct) const;

        /// \warning Untrusted.
        /// \sa Note::patch
        QString patch() const;
        void setPatch(const QString &patch) const;

        /// \sa Note::regions
        QStringList regions() const;
        void setRegions(const QStringList &regions) const;

        /// \sa Note::regionEnds
        QStringList regionEnds() const;
        void setRegionEnds(const QStringList &regionEnds) const;

        UserDataRef userData() const;

        Note toNote() const;
    };

    class HELLOKIT_EDIT_EXPORT NoteListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        int size() const;
        NoteRef at(int index) const;
        void insert(int index, const QList<Note> &notes) const;
        void remove(int index, int count) const;
        void move(int index, int count, int destination) const;

        /// Returns the regions of the notes, as Track::regions() does.
        QList<Region> regions() const;
    };

    class HELLOKIT_EDIT_EXPORT TrackRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QString name() const;
        void setName(const QString &name) const;

        QString voiceDir() const;
        void setVoiceDir(const QString &voiceDir) const;

        NoteListRef notes() const;

        Track toTrack() const;
    };

    class HELLOKIT_EDIT_EXPORT TrackListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        int size() const;
        TrackRef at(int index) const;
        void insert(int index, const QList<Track> &tracks) const;
        void remove(int index, int count) const;
        void move(int index, int count, int destination) const;
    };

    /// \sa Project::unknownFields
    class HELLOKIT_EDIT_EXPORT UnknownFieldsRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QStringList keys() const;
        bool contains(const QString &key) const;
        QJsonValue value(const QString &key) const;
        void setValue(const QString &key, const QJsonValue &value) const;
        void remove(const QString &key) const;
    };

    /// The handle of the root of a session, from which the other handles are obtained.
    class HELLOKIT_EDIT_EXPORT ProjectRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        explicit ProjectRef(ProjectSession *session);

        SettingsRef settings() const;
        TrackListRef tracks() const;
        UnknownFieldsRef unknownFields() const;

        Project toProject() const;
    };

}

#endif // HELLOKIT_EDIT_PROJECTREFS_H
