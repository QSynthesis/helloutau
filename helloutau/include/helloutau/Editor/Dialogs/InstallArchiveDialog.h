#ifndef HELLOUTAU_EDITOR_DIALOGS_INSTALLARCHIVEDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_INSTALLARCHIVEDIALOG_H

#include <filesystem>
#include <optional>

#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/InstallArchive.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QButtonGroup;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;

namespace hello::daw {

    class AppSettings;

    /// A dialog that installs a voice bank or a plugin from an installation archive of UTAU, a
    /// \c .uar or a \c .zip with an \c install.txt at its root. See docs/ArchiveInstall.md.
    ///
    /// The dialog shows the kind and the name of the content, the description of
    /// \c install.txt and the files of the content. If a name of the archive is neither ASCII nor
    /// marked as UTF-8, the user chooses the encoding of the names, and how the names are written
    /// to disk if the ANSI code page of the system differs from that encoding. For a voice bank,
    /// the user chooses the encoding of its text files, which the installation records. The
    /// encodings that read the names or the text files best are initially selected, see
    /// TextCodec::ranked(), and those in which they have invalid byte sequences are gray.
    ///
    /// install() runs the whole installation: the dialog, the question about a folder that
    /// exists, and the progress.
    class HELLOUTAU_EDITOR_EXPORT InstallArchiveDialog : public QDialog {
        Q_OBJECT
    public:
        /// The installed voice bank or plugin
        struct Installation {
            kit::InstallArchive::Kind kind = kit::InstallArchive::VoiceBank;
            /// The name from \c character.txt or \c plugin.txt, or the folder if it names none
            QString name;
            std::filesystem::path folder;
        };

        /// \a archive must be open (kit::InstallArchive::open()) and outlive the dialog. The
        /// dialog decodes the archive in the selected encoding of the names, so that the
        /// archive is ready for kit::InstallArchive::install() once the dialog is accepted.
        explicit InstallArchiveDialog(kit::InstallArchive &archive, QWidget *parent = nullptr);
        ~InstallArchiveDialog();

        /// Returns whether \a path is handled as an installation archive rather than opened as
        /// a project: a \c .uar, or a \c .zip with an \c install.txt at its root.
        static bool isInstallArchive(const std::filesystem::path &path);

        /// Returns the folder into which a voice bank or a plugin is installed, the \c voice or
        /// \c plugins directory of AppSettings::HelloUtauRoot.
        static std::filesystem::path destinationOf(kit::InstallArchive::Kind kind,
                                                   const AppSettings &settings);

        /// Asks the user for an installation archive with a file dialog over \a parent, of
        /// \a kind if given, which names the dialog.
        ///
        /// \return the archive, or \c std::nullopt if the user cancels
        static std::optional<std::filesystem::path>
            chooseArchive(QWidget *parent,
                          std::optional<kit::InstallArchive::Kind> kind = std::nullopt);

        /// Installs the archive \a path over \a parent: shows the dialog, asks whether to merge
        /// into a folder that exists or to install into a new folder, and shows the progress.
        /// Errors are shown to the user. If \a only is given, an archive of the other kind is
        /// refused, as the install buttons of the settings pages require.
        ///
        /// \return the installed content, or \c std::nullopt if the archive cannot be installed,
        ///         the installation failed, or the user canceled
        static std::optional<Installation>
            install(const std::filesystem::path &path, const AppSettings &settings, QWidget *parent,
                    std::optional<kit::InstallArchive::Kind> only = std::nullopt);

        /// Tells the user over \a parent that \a installation is installed.
        static void reportInstalled(const Installation &installation, QWidget *parent);

        /// Returns whether the archive is installable in the selected encoding of the names.
        /// errorText() returns the reason otherwise.
        bool isInstallable() const;
        QString errorText() const;

        /// The text that names the kind and the name of the content, and the description
        QString summaryText() const;

        /// Returns the name from \c character.txt or \c plugin.txt, or the folder of
        /// \c install.txt if the file names none.
        QString contentName() const;

        /// \name The names of the archive
        /// @{

        /// The encoding of the names, or an empty string if the archive needs none
        QString nameEncoding() const;
        void setNameEncoding(const QString &encoding);

        /// Returns whether candidate \a encoding is gray, which indicates invalid byte sequences
        /// in the names or in \c install.txt decoded in \a encoding.
        bool isNameEncodingGray(const QString &encoding) const;

        /// Returns whether the user chooses how the names are written: the archive needs an
        /// encoding of the names, the ANSI code page of the system differs from it, and the
        /// names can be written as misdecoded or some names cannot be represented in that code
        /// page, which the dialog then explains.
        bool isNameModeShown() const;

        /// Returns whether the names can be written as misdecoded, see
        /// kit::InstallArchive::canWriteMisdecoded().
        bool isMisdecodedAvailable() const;

        kit::InstallArchive::NameMode nameMode() const;
        void setNameMode(kit::InstallArchive::NameMode mode);

        /// The paths of the files of the content in the tree, in the order of the archive
        QStringList contentPaths() const;

        /// @}

        /// \name The text files of a voice bank
        /// @{

        /// The encoding of the text files of a voice bank, which hello-config.json records, or an
        /// empty string for a plugin
        QString charset() const;
        void setCharset(const QString &charset);

        /// Returns whether candidate \a charset is gray, which indicates invalid byte sequences
        /// in a previewed text file decoded in \a charset.
        bool isCharsetGray(const QString &charset) const;

        /// The titles of the tabs: the files of the content, and a tab per previewed text file
        QStringList previewTitles() const;

        /// The text shown on tab \a index, empty for the tab of the files
        QString previewText(int index) const;

        /// @}

    private:
        struct TextFile {
            QString path;
            QByteArray bytes;
        };

        kit::InstallArchive &m_archive;
        bool m_installable = false;
        // Whether the user chose the encoding of the text files, which is then kept
        bool m_charsetChosen = false;
        QList<TextFile> m_textFiles;

        QLabel *m_summary;
        QGroupBox *m_names;
        QComboBox *m_nameEncoding;
        QWidget *m_nameMode;
        QButtonGroup *m_nameModes;
        QRadioButton *m_decoded;
        QRadioButton *m_misdecoded;
        QLabel *m_nameNote;
        QGroupBox *m_text;
        QComboBox *m_charset;
        QTabWidget *m_previews;
        QTreeWidget *m_files;
        QLabel *m_error;
        QPushButton *m_install;

        // Decodes the archive in the selected encoding of the names and shows the result.
        void decodeNames();
        // Reads the text files of a voice bank from the decoded archive.
        void readTextFiles();
        void updateNameMode();
        void updateCharsets();
        void updatePreviews();
        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_INSTALLARCHIVEDIALOG_H
