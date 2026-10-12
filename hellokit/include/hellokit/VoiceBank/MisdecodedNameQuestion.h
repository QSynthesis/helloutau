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
    /// The question about a directory with a record of misdecoded file names covers the names
    /// that the record lists in neither list. The question about a directory without a record
    /// covers all of its names if the inference from its \c oto.ini proposes a pair of encodings.
    class HELLOKIT_VOICEBANK_EXPORT MisdecodedNameQuestion {
    public:
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
        /// \c std::nullopt if nothing is to be asked: no name to determine restores, or no record
        /// exists and the inference proposes nothing. A name that does not restore is used as it
        /// is until a question about another name records it.
        ///
        /// The inference tries every pair of distinct encodings of TextCodec::ustCandidates() and
        /// takes the pair under which the most entries of the \c oto.ini whose audio file is
        /// missing find it under a restored name. Among equals, the pair that misdecodes in the
        /// system encoding comes first.
        static std::optional<MisdecodedNameQuestion> of(const VoiceBankDirectorySource &source,
                                                        const QString &charset);

        /// Returns the question about \a source , a directory without a record, with the
        /// encodings \a original and \a misdecodedAs that the user chose over the inferred ones.
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

        /// Returns whether the directory has a record, whose encodings the question takes.
        inline bool isRecorded() const {
            return m_recorded;
        }

        /// The number of entries of the \c oto.ini whose audio file does not exist under its name
        /// on disk.
        inline int missingEntries() const {
            return m_missing;
        }

        /// The number of the missing entries whose audio file exists under a name that restores
        /// to the name of the entry.
        inline int matchedEntries() const {
            return m_matched;
        }

        /// The names to determine, in the order of the listing: the audio files, then the
        /// subdirectories. A name that does not restore is not among them.
        inline const QList<Choice> &choices() const {
            return m_choices;
        }

        /// Returns the record of the directory with the names \a misdecoded determined as
        /// misdecoded and every other name of choices() as correct, together with the names of
        /// the record, and with every name that does not restore as correct.
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
        bool m_recorded = false;
        int m_missing = 0;
        int m_matched = 0;
        QList<Choice> m_choices;
        /// The names to determine that do not restore, which an answer records as correct.
        QStringList m_unrestorable;
    };

}

#endif // HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_H
