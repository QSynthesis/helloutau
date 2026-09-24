#ifndef HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H
#define HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QList>
#include <QtCore/QString>

#include <stdutau/charactertxt.h>
#include <stdutau/otoini.h>
#include <stdutau/prefixmap.h>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/VoiceBankConfig.h>

namespace hello::kit {

    /// The state of one file of VoiceBankDirectorySource::File when it was read.
    struct VoiceBankFileRecord {
        /// The name as found on disk, under which the file is saved. A voice bank created on
        /// Windows may use \c OTO.INI , which is a different file on a case-sensitive file
        /// system.
        std::filesystem::path name;

        /// The SHA-1 digest of the bytes read, used by save() to detect whether another program
        /// has written the file since.
        QByteArray digest;
    };

    /// A snapshot of a directory, inexpensive enough to take on every check.
    ///
    /// Contains every name relevant to a voice bank: subdirectories, audio files, and the files
    /// of VoiceBankDirectorySource::File with their size and modification time. Audio files are
    /// recorded by name only, because their contents concern the render cache, not the voice bank.
    /// All other files are excluded, such as the \c .frq files a resampler writes beside a sample,
    /// so that rendering is not mistaken for an edit.
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankDirectoryStamp {
        struct Entry {
            std::filesystem::path name;
            bool directory = false;
            std::uintmax_t size = 0;
            std::filesystem::file_time_type time{};

            inline bool operator==(const Entry &RHS) const {
                return name == RHS.name && directory == RHS.directory && size == RHS.size &&
                       time == RHS.time;
            }
            inline bool operator!=(const Entry &RHS) const {
                return !(*this == RHS);
            }
        };

        /// Sorted by name.
        std::vector<Entry> entries;

        /// The time at which the snapshot was taken, which determines whether a modification
        /// time can be trusted. See isRacy() .
        std::filesystem::file_time_type takenAt{};

        /// Stamps are equal if all entries are equal. \c takenAt is not compared.
        inline bool operator==(const VoiceBankDirectoryStamp &RHS) const {
            return entries == RHS.entries;
        }
        inline bool operator!=(const VoiceBankDirectoryStamp &RHS) const {
            return !(*this == RHS);
        }

        /// Returns whether \a entry was modified so shortly before the snapshot that a later
        /// write could leave the same size and modification time, which a stamp cannot
        /// distinguish.
        ///
        /// A file system stores times at a fixed granularity, two seconds on FAT, and two writes
        /// within one interval are indistinguishable. A racy entry must therefore be compared by
        /// content. Git applies the same rule under the same name.
        bool isRacy(const Entry &entry) const;

        /// Takes the stamp of \a directory , which is the root of the voice bank if \a root is
        /// \c true . See VoiceBankDirectorySource::fileNamed() for the files recorded.
        ///
        /// \return the stamp, or \c std::nullopt if \a directory is not a directory
        static std::optional<VoiceBankDirectoryStamp> take(const std::filesystem::path &directory,
                                                           bool root);
    };

    /// One directory of a voice bank as found on disk, not decoded.
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankDirectorySource {
        /// The files of a voice bank directory that this library reads and saves.
        enum File {
            Oto,
            PrefixMap,
            Character,
            Readme,
            Config,
        };

        /// The lowercase name under which \a file is created. An existing file keeps its name.
        /// See VoiceBankFileRecord::name .
        static const char *fileName(File file);

        /// Returns the file identified by the lowercase name \a foldedName in the root directory
        /// if \a root is \c true or in a subdirectory otherwise, or \c std::nullopt for any other
        /// name.
        ///
        /// \c character.txt , \c prefix.map and \c readme.txt belong to the root only, because
        /// UTAU reads them from the voice bank directory alone. In a subdirectory they belong to
        /// the voice bank that the subdirectory forms if selected by itself, and are neither read
        /// nor written.
        static std::optional<File> fileNamed(std::string_view foldedName, bool root);

        /// The location relative to the voice bank root. Empty for the root itself.
        std::filesystem::path path;

        /// The contents of \c hello-config.json in this directory, if present.
        std::optional<VoiceBankConfig> config;

        /// \name UTAU files in this directory
        ///
        /// Absent if the directory does not contain the file.
        ///
        /// \warning Every string is raw bytes in the encoding of the author's machine. Decode
        ///          these through \c TextCodec before treating them as text.
        /// @{
        std::optional<utau::OtoIni> oto;
        std::optional<utau::PrefixMap> prefixMap;
        std::optional<utau::CharacterTxt> character;
        QByteArray readme;
        /// @}

        /// The audio files in this directory by name, in directory listing order.
        std::vector<std::filesystem::path> audioFiles;

        /// Every file above that exists and was readable. A file absent from this map either
        /// did not exist or could not be read, and save() must not replace it in either case.
        std::map<File, VoiceBankFileRecord> files;

        /// A snapshot of the directory, taken before any file in it was read. A change made
        /// during reading is thereby detected as a change at the next check rather than
        /// accepted as the state read.
        VoiceBankDirectoryStamp stamp;

        /// Returns whether any content requires an encoding to be read. An \c oto.ini that
        /// declares an available encoding for itself does not. See utau::OtoIni::charset .
        bool needsCharset() const;

        /// Sample text from this directory, for an encoding selector to display under each
        /// candidate encoding.
        ///
        /// Aliases are used because a wrong encoding is immediately visible in them. The text is
        /// not decoded here, because decoding would presuppose the answer.
        ///
        /// \warning The views refer into this object and must not outlive it.
        QList<QByteArrayView> rawAliases() const;
    };

    /// Selects the encoding of the UTAU files of a directory when nothing on disk records it.
    ///
    /// Implemented by the user interface layer, not by this library. No encoding detection is
    /// performed. A UST and a voice bank pose the same problem and follow the same rule. See
    /// docs/note.md.
    ///
    /// \warning The calling thread is unspecified, and the implementation is responsible for
    ///          thread safety.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankCharsetSelector {
    public:
        virtual ~VoiceBankCharsetSelector();

        /// \return the encoding, or \c std::nullopt to leave the directory out of the voice bank
        /// \note \c std::nullopt with an \c Error in \a diagnostics indicates that the user
        ///       could not be asked. \c std::nullopt without one indicates that the user
        ///       declined. The caller has no other means of distinguishing the two.
        virtual std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                                     DiagnosticList &diagnostics) = 0;
    };

    /// Selects the same encoding for every directory.
    ///
    /// Used by the command-line tools and the tests. A test that opened a dialog would hang
    /// instead of failing, so a selector that requires no user is necessary.
    class HELLOKIT_VOICEBANK_EXPORT FixedCharsetSelector : public VoiceBankCharsetSelector {
    public:
        explicit FixedCharsetSelector(QString charset);
        ~FixedCharsetSelector() override;

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &diagnostics) override;

    private:
        QString m_charset;
    };

    /// Limits at which a scan stops and reports the condition.
    ///
    /// A voice bank is a folder selected by the user, so its structure cannot be trusted.
    /// Without limits, a deep or cyclic tree would make opening a folder a nonterminating
    /// operation.
    struct VoiceBankLimits {
        int maxDepth = 8;
        int maxDirectories = 4096;
    };

    /// The directories of a voice bank, read but not decoded.
    ///
    /// Reading and decoding are separate steps for the same reason as for a \c .ust : an
    /// encoding selector must show the user the result of each candidate encoding, which is
    /// impossible if an encoding is required to read the file in the first place.
    ///
    /// \note open() writes nothing. Recording an encoding requires writing a
    ///       \c hello-config.json into the user's voice bank, and a scan is not the place for
    ///       that decision: the voice bank may reside on a read-only disk, and a user who
    ///       merely inspected a folder did not request a new file in it. VoiceBank::save()
    ///       writes the configuration together with the first file it writes into a directory,
    ///       or by itself once VoiceBank::rememberCharset() has been called for the directory.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankSource {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankSource)
    public:
        /// Traverses \a root and reads its contents.
        static std::optional<VoiceBankSource> open(const std::filesystem::path &root,
                                                   DiagnosticList &diagnostics,
                                                   const VoiceBankLimits &limits = {});

        inline const std::filesystem::path &root() const {
            return m_root;
        }

        /// The non-empty directories, the root first.
        inline const QList<VoiceBankDirectorySource> &directories() const {
            return m_directories;
        }

        /// Rereads one directory of the voice bank at \a root , excluding its subdirectories.
        ///
        /// Intended for a directory whose files must be read anew, in another encoding or after
        /// a change on disk, without traversing the entire voice bank.
        ///
        /// \param relative the directory relative to \a root . Empty for the root itself.
        static std::optional<VoiceBankDirectorySource>
            readDirectory(const std::filesystem::path &root, const std::filesystem::path &relative,
                          DiagnosticList &diagnostics);

        /// Reads directory \a relative of the voice bank at \a root with its entire subtree,
        /// within \a limits measured from \a root .
        ///
        /// Intended for a directory that appeared in a voice bank that is already open.
        ///
        /// \param alreadyRead the number of directories of the voice bank already read, which
        ///        counts toward VoiceBankLimits::maxDirectories
        static QList<VoiceBankDirectorySource>
            readTree(const std::filesystem::path &root, const std::filesystem::path &relative,
                     const VoiceBankLimits &limits, int alreadyRead, DiagnosticList &diagnostics);

        /// The directories that contain text to decode and have no recorded encoding.
        ///
        /// \warning The pointers refer into this object and must not outlive it.
        QList<const VoiceBankDirectorySource *> unsettled() const;

    private:
        VoiceBankSource() = default;

        std::filesystem::path m_root;
        QList<VoiceBankDirectorySource> m_directories;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H
