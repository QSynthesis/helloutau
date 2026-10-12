#ifndef HELLOKIT_VOICEBANK_MISDECODEDNAMECONVERSION_H
#define HELLOKIT_VOICEBANK_MISDECODEDNAMECONVERSION_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/MisdecodedNameQuestion.h>
#include <hellokit/VoiceBank/VoiceBankConfiguration.h>

namespace hello::kit {

    /// The conversion of the misdecoded file names of one directory, which the user previews and
    /// checks in the dialog that converts the encodings of a voice bank. See the section on the
    /// conversion in docs/Editing.md.
    ///
    /// The conversion lists every name that is not ASCII and that restores under its two
    /// encodings, whatever the record of the directory determines. The lists of the record
    /// determine the proposed states under any encodings, because the lists record which names
    /// are misdecoded and the encodings record how the names restore.
    class HELLOKIT_VOICEBANK_EXPORT MisdecodedNameConversion {
    public:
        /// The names of one directory and the state on which a conversion depends.
        struct Listing {
            /// The directory, relative to the root of the voice bank
            std::filesystem::path directory;
            /// The encoding of the text files of the directory, empty if none was read
            QString charset;
            /// The names on disk of the audio files, in the order of the listing
            QStringList audioFiles;
            /// The names on disk of the subdirectories, in the order of the listing
            QStringList subdirectories;
            /// The file names of the entries of the \c oto.ini
            QStringList entries;
            /// The record of the directory, or \c std::nullopt if the user has not decided
            std::optional<VoiceBankConfiguration::FileNameEncoding> record;
        };

        using Choice = MisdecodedNameQuestion::Choice;

        /// Returns the conversion of \a listing under the encodings of its record. If the record
        /// has no encodings or is absent, the encodings are inferred from the entries as
        /// MisdecodedNameQuestion::of() infers them. If no pair matches an entry, the names are
        /// misdecoded in the system encoding, from the candidate that TextCodec::ranked() ranks
        /// first for their bytes.
        static MisdecodedNameConversion of(const Listing &listing);

        /// Returns the conversion of \a listing under the encodings \a original and
        /// \a misdecodedAs that the user chose.
        static MisdecodedNameConversion of(const Listing &listing, const QString &original,
                                           const QString &misdecodedAs);

        /// The directory, relative to the root of the voice bank.
        inline const std::filesystem::path &directory() const {
            return m_directory;
        }

        /// The encoding in which the names were created.
        inline const QString &original() const {
            return m_original;
        }

        /// The encoding in which the bytes of the names were interpreted.
        inline const QString &misdecodedAs() const {
            return m_misdecodedAs;
        }

        /// Returns whether the directory has a record, which determines the proposed states.
        inline bool isRecorded() const {
            return m_record.has_value();
        }

        /// The names that restore, in the order of the listing: the audio files, then the
        /// subdirectories. A name is proposed as misdecoded as the record determines it. A name
        /// that the record does not determine is proposed as misdecoded unless an entry of the
        /// \c oto.ini names it as it is.
        inline const QList<Choice> &choices() const {
            return m_choices;
        }

        /// The names on disk that are not ASCII and do not restore, which the conversion neither
        /// renames nor records as misdecoded.
        inline const QStringList &unrestorable() const {
            return m_unrestorable;
        }

        /// Returns the record of the directory with the names \a misdecoded as misdecoded and
        /// every other name that is not ASCII as correct, in the form in which it is written.
        ///
        /// Intended for a conversion that reads the names as misdecoded and renames nothing.
        VoiceBankConfiguration::FileNameEncoding record(const QStringList &misdecoded) const;

        /// Returns the record that restores the names a conversion renames, and whose states the
        /// names that are not renamed keep: the record of the directory under the encodings of
        /// the conversion. If the record has no encodings or is absent, the lists are empty, under
        /// which such a name is correct.
        ///
        /// \sa VoiceBankSession::NameConversion
        VoiceBankConfiguration::FileNameEncoding keptRecord() const;

    private:
        MisdecodedNameConversion() = default;

        // Lists the names of the listing under the encodings of the conversion.
        void collect(const Listing &listing);

        std::filesystem::path m_directory;
        QString m_original;
        QString m_misdecodedAs;
        std::optional<VoiceBankConfiguration::FileNameEncoding> m_record;
        QList<Choice> m_choices;
        QStringList m_unrestorable;
    };

}

#endif // HELLOKIT_VOICEBANK_MISDECODEDNAMECONVERSION_H
