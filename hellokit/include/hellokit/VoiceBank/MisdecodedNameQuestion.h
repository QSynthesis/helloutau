#ifndef HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_H
#define HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/VoiceBankConfig.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// The question about the misdecoded file names of one directory, which the user answers in
    /// an encoding dialog on opening. See the section on misdecoded file names in
    /// docs/Editing.md.
    ///
    /// The question about a directory whose record has lists covers the names that the record
    /// lists in neither list. A directory whose record has no lists is not asked about. The
    /// question about a directory without a record covers all of its names that are not ASCII if
    /// a name that restores is not named by an entry of the \c oto.ini as it is.
    class HELLOKIT_VOICEBANK_EXPORT MisdecodedNameQuestion {
    public:
        /// The basis of the encodings and of the proposed states.
        enum Basis {
            Record,  ///< The record of the directory
            Entries, ///< The missing entries of the \c oto.ini that restored names match
            Found,   ///< The entries found under their names, which propose the names as they are
            None,    ///< No entry, the encodings preset to the encoding of the text files
        };

        /// One name to determine.
        struct Choice {
            /// The name on disk.
            QString name;
            /// The name restored by the encodings of the question.
            QString restored;
            /// Whether the name is proposed as misdecoded.
            bool misdecoded = false;
            /// Whether the name is a subdirectory rather than an audio file.
            bool directory = false;
        };

        /// Returns the question about \a source , whose text files are in \a charset , or
        /// \c std::nullopt if nothing is to be asked: the record has no lists, no name to
        /// determine restores, or without a record, every name that restores is named by an
        /// entry as it is. A name that does not restore is used as it is until a question about
        /// another name records it.
        ///
        /// The inference tries every pair of distinct encodings of TextCodec::ustCandidates() and
        /// takes the pair under which the most entries of the \c oto.ini whose audio file is
        /// missing find it under a restored name. Among equals, the pair whose original encoding
        /// is \a charset comes first, then the pair that misdecodes in the system encoding. If no
        /// pair matches an entry, the pair is \a charset and the system encoding, and the names
        /// are proposed as they are if at least 75 percent of the entries find their files under
        /// their names and every name to determine is representable in \a charset .
        static std::optional<MisdecodedNameQuestion> of(const VoiceBankDirectorySource &source,
                                                        const QString &charset);

        /// Returns the question about \a source , a directory without a record, with the
        /// encodings \a original and \a misdecodedAs that the user chose over the proposed ones.
        static MisdecodedNameQuestion of(const VoiceBankDirectorySource &source,
                                         const QString &charset, const QString &original,
                                         const QString &misdecodedAs);

        /// The directory, relative to the root of the voice bank.
        inline const std::filesystem::path &directory() const {
            return m_directory;
        }

        /// The encoding in which the names were created.
        inline const QString &original() const {
            return m_record.original;
        }

        /// The encoding in which the bytes of the names were interpreted.
        inline const QString &misdecodedAs() const {
            return m_record.misdecodedAs;
        }

        /// The basis of the encodings and of the proposed states.
        inline Basis basis() const {
            return m_basis;
        }

        /// Returns whether the directory has a record, whose encodings the question takes.
        inline bool isRecorded() const {
            return m_basis == Record;
        }

        /// The number of the distinct file names of the entries of the \c oto.ini whose audio
        /// file exists under its name on disk.
        inline int foundEntries() const {
            return m_found;
        }

        /// The number of the distinct file names of the entries of the \c oto.ini whose audio
        /// file does not exist under its name on disk.
        inline int missingEntries() const {
            return m_missing;
        }

        /// The number of the missing entries whose audio file exists under a name that restores
        /// to the name of the entry.
        inline int matchedEntries() const {
            return m_matched;
        }

        /// The number of choices that no entry names, neither as they are nor restored, whose
        /// state the entries therefore do not indicate.
        inline int unlistedNames() const {
            return m_unlisted;
        }

        /// The names to determine, in the order of the listing: the audio files, then the
        /// subdirectories. A name that does not restore is not among them.
        inline const QList<Choice> &choices() const {
            return m_choices;
        }

        /// Returns the record of the directory with the names \a misdecoded determined as
        /// misdecoded and every other name of choices() as correct, together with the names of
        /// the record, and with every name that does not restore as correct. The record has no
        /// encodings if no name is misdecoded, and no lists if no correct name restores.
        VoiceBankConfig::FileNameEncoding answer(const QStringList &misdecoded) const;

    private:
        MisdecodedNameQuestion() = default;

        /// Lists the names of \a source to determine under the encodings of \a m_record ,
        /// proposing as misdecoded each name that no entry of \a entries names, and counts the
        /// entries of \a missing that the names restore to.
        void collect(const VoiceBankDirectorySource &source, const QStringList &entries,
                     const QStringList &missing);

        std::filesystem::path m_directory;
        VoiceBankConfig::FileNameEncoding m_record;
        Basis m_basis = None;
        int m_found = 0;
        int m_missing = 0;
        int m_matched = 0;
        int m_unlisted = 0;
        QList<Choice> m_choices;
        /// The number of choices that no entry names as they are.
        int m_unnamed = 0;
        /// The names to determine that do not restore, which an answer records as correct.
        QStringList m_unrestorable;
    };

}

#endif // HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_H
