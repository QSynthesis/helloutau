#ifndef HELLOKIT_EDIT_PROJECTREFS_H
#define HELLOKIT_EDIT_PROJECTREFS_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Note.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/NodeRef.h>
#include <hellokit/Edit/ProjectSchema.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    // The handles of the nodes of a project tree. Each getter and setter reads or writes the
    // slot of ProjectSchema.h with the same name. See NodeRef.

    /// The base of the handles of a project tree, which refer to a ProjectSession.
    class HELLOKIT_EDIT_EXPORT ProjectNodeRef : public NodeRef {
    public:
        inline ProjectNodeRef() = default;

        inline ProjectNodeRef(ProjectSession *session, NodeId id) : NodeRef(session, id) {
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

        /// \warning Untrusted, see \c ProjectSettings::wavtool.
        QString wavtool() const;
        void setWavtool(const QString &wavtool) const;

        /// \warning Untrusted, see \c ProjectSettings::resampler.
        QString resampler() const;
        void setResampler(const QString &resampler) const;

        bool mode2() const;
        void setMode2(bool mode2) const;
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

        /// Returns a copy of the point.
        PortamentoPoint toPoint() const;
    };

    class HELLOKIT_EDIT_EXPORT PortamentoListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        int size() const;
        PortamentoPointRef at(int index) const;

        /// Inserts copies of \a points before \a index.
        void insert(int index, const QList<PortamentoPoint> &points) const;

        void remove(int index, int count) const;

        /// Moves \a count points starting at \a index so that the first of them is at
        /// \a destination afterwards.
        void move(int index, int count, int destination) const;
    };

    class HELLOKIT_EDIT_EXPORT PitchBendRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        std::optional<double> start() const;
        void setStart(std::optional<double> start) const;

        int size() const;
        QList<double> values() const;

        /// Overwrites the values starting at \a index, extending the curve if \a values reaches
        /// beyond its end.
        void replaceValues(int index, const QList<double> &values) const;

        void insertValues(int index, const QList<double> &values) const;
        void removeValues(int index, int count) const;
    };

    /// See \c Note::userData.
    class HELLOKIT_EDIT_EXPORT UserDataRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        /// Returns the keys in ascending order.
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

        /// Replaces the envelope as a whole.
        void setEnvelope(const std::optional<Envelope> &envelope) const;

        std::optional<Vibrato> vibrato() const;

        /// Replaces the vibrato as a whole.
        void setVibrato(const std::optional<Vibrato> &vibrato) const;

        PortamentoListRef portamento() const;

        /// Returns the Mode1 pitch curve, or an invalid handle if the note has none.
        PitchBendRef pitchBend() const;

        /// Replaces the Mode1 pitch curve, or removes it if \a pitchBend is empty.
        void setPitchBend(const std::optional<PitchBend> &pitchBend) const;

        QString label() const;
        void setLabel(const QString &label) const;

        QString direct() const;
        void setDirect(const QString &direct) const;

        /// \warning Untrusted, see \c Note::patch.
        QString patch() const;
        void setPatch(const QString &patch) const;

        QString region() const;
        void setRegion(const QString &region) const;

        QString regionEnd() const;
        void setRegionEnd(const QString &regionEnd) const;

        UserDataRef userData() const;

        /// Returns a copy of the note, or a default note if the handle is invalid.
        Note toNote() const;
    };

    class HELLOKIT_EDIT_EXPORT NoteListRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        int size() const;
        NoteRef at(int index) const;

        /// Inserts copies of \a notes before \a index.
        void insert(int index, const QList<Note> &notes) const;

        void remove(int index, int count) const;

        /// Moves \a count notes starting at \a index so that the first of them is at
        /// \a destination afterwards. The notes keep their identifiers.
        void move(int index, int count, int destination) const;
    };

    class HELLOKIT_EDIT_EXPORT TrackRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        QString name() const;
        void setName(const QString &name) const;

        QString voiceDir() const;
        void setVoiceDir(const QString &voiceDir) const;

        NoteListRef notes() const;
    };

    /// The handle of the root of a session, from which the other handles are obtained.
    class HELLOKIT_EDIT_EXPORT ProjectRef : public ProjectNodeRef {
    public:
        using ProjectNodeRef::ProjectNodeRef;

        explicit ProjectRef(ProjectSession *session);

        SettingsRef settings() const;

        int trackCount() const;
        TrackRef track(int index) const;
    };

}

#endif // HELLOKIT_EDIT_PROJECTREFS_H
