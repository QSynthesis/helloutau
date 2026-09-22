#ifndef HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H
#define HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H

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

    /// The files of a voice bank directory that this library reads and writes back.
    enum class VoiceBankFile {
        Oto,
        PrefixMap,
        Character,
        Readme,
        Config,
    };

    /// The name a file is created under, in lower case. One that is already there keeps its
    /// own, see VoiceBankFileRecord::name .
    HELLOKIT_VOICEBANK_EXPORT const char *voiceBankFileName(VoiceBankFile file);

    /// Which file \a foldedName is, given in lower case, or nothing for any other name.
    HELLOKIT_VOICEBANK_EXPORT std::optional<VoiceBankFile>
        voiceBankFileNamed(std::string_view foldedName);

    /// One of those files as it was when it was read.
    struct VoiceBankFileRecord {
        /// Its name as it was found, which is the name it is written back under. A bank from
        /// Windows may spell it \c OTO.INI , and where case matters that is another file.
        std::filesystem::path name;

        /// SHA-1 of the bytes that were read, which is how a save tells whether something else
        /// has written the file since.
        QByteArray digest;
    };

    /// One directory of a voice bank as it was found, with nothing decoded.
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankDirectorySource {
        /// Where it is, relative to the bank root, and empty for the root itself.
        std::filesystem::path path;

        /// What \c hello-config.json in this directory says, absent where there is none.
        std::optional<VoiceBankConfig> config;

        /// \name The UTAU files found here
        ///
        /// Absent where the directory has none.
        ///
        /// \warning Every string in them is raw bytes in whatever encoding the author's machine
        ///          was using. Put anything taken from here through \c TextCodec before treating
        ///          it as text.
        /// @{
        std::optional<utau::OtoIni> oto;
        std::optional<utau::PrefixMap> prefixMap;
        std::optional<utau::CharacterTxt> character;
        QByteArray readme;
        /// @}

        /// The audio files here, by name, in the order the directory listed them.
        std::vector<std::filesystem::path> audioFiles;

        /// Every file above that was there and could be read. One that is missing here was not
        /// there, or could not be read, and a save must not replace it in either case.
        std::map<VoiceBankFile, VoiceBankFileRecord> files;

        /// Whether anything here has to be decoded before it can be read.
        bool needsCharset() const;

        /// Text out of this directory for a chooser to show under each candidate encoding.
        ///
        /// Aliases, because a wrong encoding shows up in them at a glance. Decoding them here
        /// would answer the question before it was put.
        ///
        /// \warning These look into this object and do not outlive it.
        QList<QByteArrayView> rawAliases() const;
    };

    /// Asked which encoding a directory's UTAU files are in, where nothing on disk says.
    ///
    /// Implemented wherever there is a user interface, which is not here. Nothing guesses: a UST
    /// and a voice bank are the same problem, and the rule is the same. See docs/note.md.
    ///
    /// \warning Nothing here says which thread it runs on, and the implementor has to settle
    ///          that.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankCharsetSelector {
    public:
        virtual ~VoiceBankCharsetSelector();

        /// \return the encoding, or nothing to leave this directory out of the bank
        /// \note Returning nothing with an \c Error recorded means the question could not be
        ///       put. Returning nothing without one means the user declined. A caller cannot
        ///       tell them apart any other way.
        virtual std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                                     DiagnosticList &diagnostics) = 0;
    };

    /// Answers every directory with the same encoding.
    ///
    /// What the command line tools and the tests use. A test that reached a dialog would hang
    /// rather than fail, so there has to be an answer available without a user.
    class HELLOKIT_VOICEBANK_EXPORT FixedCharsetSelector : public VoiceBankCharsetSelector {
    public:
        explicit FixedCharsetSelector(QString charset);
        ~FixedCharsetSelector() override;

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &diagnostics) override;

    private:
        QString m_charset;
    };

    /// How far a scan goes before it stops and says so.
    ///
    /// A voice bank is a folder a user picked, so its shape is not this program's to trust.
    /// Without a limit a deep or a looping tree turns opening a folder into an operation that
    /// does not end.
    struct VoiceBankLimits {
        int maxDepth = 8;
        int maxDirectories = 4096;
    };

    /// A voice bank's directories, read but not decoded.
    ///
    /// Reading and decoding are two steps for the same reason they are for a \c .ust : a chooser
    /// has to show the user what each candidate encoding makes of the file, and that cannot
    /// happen if the encoding was already needed to read it.
    ///
    /// \note open() writes nothing. Remembering an encoding means writing a
    ///       \c hello-config.json into the user's voice bank, and a scan is not the place to
    ///       decide that: a bank may sit on a read-only disk, and a user who only looked at a
    ///       folder did not ask for a file to appear in it. VoiceBank::save() writes it along
    ///       with the first file it writes into a directory, and on its own once
    ///       VoiceBank::rememberCharset() has been called for the directory.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankSource {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankSource)
    public:
        /// Walks \a root and reads what it finds.
        static std::optional<VoiceBankSource> open(const std::filesystem::path &root,
                                                   DiagnosticList &diagnostics,
                                                   const VoiceBankLimits &limits = {});

        const std::filesystem::path &root() const {
            return m_root;
        }

        /// The directories that hold anything, the root first.
        const QList<VoiceBankDirectorySource> &directories() const {
            return m_directories;
        }

        /// Reads one directory of the bank at \a root again, and nothing under it.
        ///
        /// For a directory whose files are to be read afresh, in another encoding or because
        /// they changed on disk, without walking the whole bank to get at it.
        ///
        /// \param relative the directory, relative to \a root , and empty for the root itself
        static std::optional<VoiceBankDirectorySource>
            readDirectory(const std::filesystem::path &root, const std::filesystem::path &relative,
                          DiagnosticList &diagnostics);

        /// The directories that have something to decode and no encoding recorded.
        ///
        /// \warning These point into this object and do not outlive it.
        QList<const VoiceBankDirectorySource *> unsettled() const;

    private:
        VoiceBankSource() = default;

        std::filesystem::path m_root;
        QList<VoiceBankDirectorySource> m_directories;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKSOURCE_H
