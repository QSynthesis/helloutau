#ifndef HELLOKIT_EDIT_VOICEBANKREFS_H
#define HELLOKIT_EDIT_VOICEBANKREFS_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/EditBase/NodeRef.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/VoiceBankSchema.h>
#include <hellokit/Edit/VoiceBankSession.h>

namespace hello::kit {

    // The handles of the nodes of a voice bank tree, see NodeRef, with the conventions of
    // ProjectRefs.h. A read-only field has a getter only, and the list of directories has no
    // insert(), remove() or move(). The internal spellings of an entry have no function.

    /// The base of the handles of a voice bank tree, which refer to a VoiceBankSession.
    class HELLOKIT_EDIT_EXPORT VoiceBankNodeRef : public edit::NodeRef {
    public:
        inline VoiceBankNodeRef() = default;

        inline VoiceBankNodeRef(VoiceBankSession *session, edit::NodeId id)
            : edit::NodeRef(session, id) {
        }

        inline VoiceBankSession *session() const {
            return static_cast<VoiceBankSession *>(m_session);
        }
    };

    class HELLOKIT_EDIT_EXPORT OtoEntryRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        QString fileName() const;
        void setFileName(const QString &fileName) const;

        QString alias() const;
        void setAlias(const QString &alias) const;

        double offset() const;
        void setOffset(double offset) const;

        double consonant() const;
        void setConsonant(double consonant) const;

        double cutoff() const;
        void setCutoff(double cutoff) const;

        double preUtterance() const;
        void setPreUtterance(double preUtterance) const;

        double voiceOverlap() const;
        void setVoiceOverlap(double voiceOverlap) const;

        VoiceOtoEntry toVoiceOtoEntry() const;
    };

    class HELLOKIT_EDIT_EXPORT OtoEntryListRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        int size() const;
        OtoEntryRef at(int index) const;
        void insert(int index, const QList<VoiceOtoEntry> &entries) const;
        void remove(int index, int count) const;
        void move(int index, int count, int destination) const;
    };

    /// A directory of the voice bank. Every field but the entries is read-only.
    ///
    /// \sa VoiceBankDirectory
    class HELLOKIT_EDIT_EXPORT VoiceDirectoryRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        std::filesystem::path path() const;
        QString charset() const;

        OtoEntryListRef otoEntries() const;

    private:
        /// \sa VoiceBankEdits::convertCharset()
        void setCharset(const QString &charset) const;

        friend class VoiceBankEdits;
    };

    /// The directories, which only reading from disk adds and removes.
    class HELLOKIT_EDIT_EXPORT VoiceDirectoryListRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        int size() const;
        VoiceDirectoryRef at(int index) const;
    };

    /// The \c prefix.map of the voice bank. The keys are note numbers.
    ///
    /// \sa VoiceBankDirectory::prefixMap
    class HELLOKIT_EDIT_EXPORT PrefixMapRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        QList<int> keys() const;
        bool contains(int noteNum) const;
        VoicePrefix value(int noteNum) const;
        void setValue(int noteNum, const VoicePrefix &prefix) const;
        void remove(int noteNum) const;
    };

    class HELLOKIT_EDIT_EXPORT VoiceCharacterRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        QString name() const;
        void setName(const QString &name) const;

        QString image() const;
        void setImage(const QString &image) const;

        QString sample() const;
        void setSample(const QString &sample) const;

        QString author() const;
        void setAuthor(const QString &author) const;

        QString web() const;
        void setWeb(const QString &web) const;

        QStringList extraLines() const;
        void setExtraLines(const QStringList &extraLines) const;

        VoiceCharacter toVoiceCharacter() const;
    };

    /// The handle of the root of a session, from which the other handles are obtained.
    class HELLOKIT_EDIT_EXPORT VoiceBankRef : public VoiceBankNodeRef {
    public:
        using VoiceBankNodeRef::VoiceBankNodeRef;

        explicit VoiceBankRef(VoiceBankSession *session);

        /// Returns an invalid handle if the voice bank has no \c character.txt .
        VoiceCharacterRef character() const;
        void setCharacter(const std::optional<VoiceCharacter> &character) const;

        /// Returns an invalid handle if the voice bank has no \c prefix.map .
        PrefixMapRef prefixMap() const;
        void setPrefixMap(const std::optional<QMap<int, VoicePrefix>> &prefixMap) const;

        QString readme() const;
        void setReadme(const QString &readme) const;

        VoiceDirectoryListRef directories() const;

        /// Returns the voice bank.
        ///
        /// \sa VoiceBankSession::snapshot()
        VoiceBank toVoiceBank() const;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKREFS_H
