#ifndef HELLOKIT_VOICEBANK_INSTALLARCHIVE_H
#define HELLOKIT_VOICEBANK_INSTALLARCHIVE_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/ZipExtractor.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// An installation archive of UTAU, a \c .uar or \c .zip with an \c install.txt at its root,
    /// which installs a voice bank or a plugin. See docs/ArchiveInstall.md.
    ///
    /// The names of the entries and the values of \c install.txt are untrusted input. Every
    /// entry passes the checks of ZipExtractor, and \c folder and \c contentsdir must each be one
    /// safe path component, or the archive is rejected as a whole.
    ///
    /// The steps are open(), setNameEncoding() and install(). The archive is read again by each
    /// step, so it must not change in between.
    class HELLOKIT_VOICEBANK_EXPORT InstallArchive {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::InstallArchive)
    public:
        enum Kind {
            VoiceBank, ///< \c type=voiceset
            Plugin,    ///< \c type=editplugin
        };

        /// The names that install() writes to the file system
        enum NameMode {
            Decoded,    ///< The names decoded in the encoding of setNameEncoding()
            Misdecoded, ///< The names as the ANSI code page of the system decodes them, as UTAU
        };

        static constexpr char manifestName[] = "install.txt";
        static constexpr qint64 manifestMaximumSize = 64 * 1024;

        InstallArchive();
        ~InstallArchive();

        /// Reads the entries of \a archive and its \c install.txt.
        ///
        /// \return whether the archive is readable and has an \c install.txt at its root.
        ///         errorString() returns the reason otherwise.
        bool open(const std::filesystem::path &archive);

        /// Returns whether a name of an entry or the \c install.txt is neither ASCII nor marked as
        /// UTF-8, so that the user must specify the encoding in which the archive was written.
        bool needsNameEncoding() const;

        /// Returns whether every name and the \c install.txt are valid in \a encoding.
        bool canDecode(const QString &encoding) const;

        /// Returns the bytes that the encoding of setNameEncoding() decodes: the names of the
        /// entries that are not marked as UTF-8, and the \c install.txt. A dialog ranks the
        /// candidates by them with TextCodec::ranked().
        QList<QByteArray> rawTexts() const;

        /// Decodes the names and the \c install.txt in \a encoding, which is ignored unless
        /// needsNameEncoding(), parses \c install.txt and checks the archive.
        ///
        /// \return whether the archive is installable: every entry passes the checks of
        ///         ZipExtractor, \c type is \c voiceset or \c editplugin, \c folder and
        ///         \c contentsdir are safe path components, and the root of the content holds a
        ///         \c character.txt for a voice bank or a \c plugin.txt for a plugin, from which
        ///         UTAU reads the name. errorString() returns the reason otherwise.
        bool setNameEncoding(const QString &encoding);

        /// The encoding of setNameEncoding(), or an empty string if none is needed
        QString nameEncoding() const;

        Kind kind() const;

        /// The name of the folder that install() creates, \c folder of \c install.txt
        QString folder() const;

        /// The folder of the archive whose content is installed, \c contentsdir of
        /// \c install.txt, or an empty string if it is omitted, which denotes the root of the
        /// archive without \c install.txt. UTAU installs the root in that case, although the
        /// specification of the plugins states that \c folder is taken.
        QString contentsDirectory() const;

        /// The one-line description of \c install.txt, plain text
        QString description() const;

        /// Returns the decoded paths of the files in contentsDirectory(), relative to it, with
        /// \c / as the separator, in the order of the archive.
        QStringList contents() const;

        /// Returns the bytes of the file at \a path of contents(), or \c std::nullopt if it is
        /// absent, unreadable or larger than \a maximumSize bytes.
        std::optional<QByteArray> readContent(const QString &path, qint64 maximumSize);

        /// Returns whether install() can write the names as Misdecoded: the ANSI code page of the
        /// system differs from nameEncoding(), and every name that is neither ASCII nor marked
        /// as UTF-8 is valid in that code page, so that a misdecoded name can be restored.
        bool canWriteMisdecoded() const;

        /// Returns the decoded names of contents() that the ANSI code page of the system cannot
        /// represent, which UTAU on this computer cannot open if the names are written as
        /// Decoded.
        QStringList unrepresentableNames() const;

        /// Returns \a folder if \a root has no entry of that name, otherwise the name followed by
        /// the first number from 0 that is free, as UTAU names the folder of a reinstallation.
        static QString freeFolderName(const std::filesystem::path &root, const QString &folder);

        /// Installs the content of contentsDirectory() into the folder \a folderName of \a root,
        /// such as freeFolderName() returns. The content is extracted into a temporary folder in
        /// \a root first. If the folder exists, the content is merged into it as UTAU does: a file
        /// of the same name is replaced and the other files are kept. Otherwise the temporary
        /// folder is renamed. A failure leaves the existing folder unchanged.
        ///
        /// For a voice bank, the hello-config.json of each folder of the content that holds an
        /// \c oto.ini records \a charset, unless empty, and the names as \a mode writes them, so
        /// that the voice bank opens without questions.
        ///
        /// \return whether the installation succeeded. errorString() returns the reason
        ///         otherwise.
        bool install(const std::filesystem::path &root, const QString &folderName, NameMode mode,
                     const QString &charset, const ZipExtractor::ProgressHandler &progress = {});

        /// Returns the reason why the last step failed.
        QString errorString() const;

    private:
        struct Entry {
            ZipExtractor::RawEntry raw;
            QString name;
        };

        bool fail(const QString &message);

        std::filesystem::path m_archive;
        QList<Entry> m_entries;
        QByteArray m_manifest;
        QString m_nameEncoding;
        Kind m_kind = VoiceBank;
        QString m_folder;
        QString m_contentsDirectory;
        QString m_description;
        QString m_errorString;

        Q_DISABLE_COPY(InstallArchive)
    };

}

#endif // HELLOKIT_VOICEBANK_INSTALLARCHIVE_H
