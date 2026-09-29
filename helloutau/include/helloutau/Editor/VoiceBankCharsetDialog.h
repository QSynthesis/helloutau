#ifndef HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H
#define HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H

#include <filesystem>

#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/VoiceBankSource.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;

namespace hello::daw {

    /// Asks the user in which encodings to read the folders of a voice bank that nothing on disk
    /// determines, all of them at once.
    ///
    /// The candidates are those of \c TextCodec::ustCandidates(), since a voice bank and a UST
    /// pose the same problem. The folders are listed, each with its encoding, checked to be read;
    /// choosing an encoding sets it for the folders selected, all of them at first. The encoding
    /// that reads all folders best is selected at first, see TextCodec::ranked(), and a folder
    /// that it cannot read without invalid bytes takes the one that reads it best. Each text file
    /// of the current folder is shown in its encoding on a tab of its own, whose title counts
    /// the byte sequences invalid in it, and the candidates that the folder does not read in
    /// are grey. A folder left unchecked, or all of them when the dialog is declined, is left
    /// out, and its samples are then found by file name only. See the section on choosing the
    /// encoding in docs/Editing.md.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankCharsetDialog : public QDialog,
                                                           public kit::VoiceBankCharsetSelector {
        Q_OBJECT
    public:
        explicit VoiceBankCharsetDialog(QWidget *parent = nullptr);
        ~VoiceBankCharsetDialog();

        /// Sets the voice bank whose folders are asked about, which the dialog names.
        void setRoot(const std::filesystem::path &root);

        /// Shows the dialog for \a directory and waits for the answer.
        std::optional<QString> selectCharset(const kit::VoiceBankDirectorySource &directory,
                                             kit::DiagnosticList &diagnostics) override;

        /// Shows the dialog for \a directories and waits for the answers.
        QList<std::optional<QString>>
            selectCharsets(const QList<const kit::VoiceBankDirectorySource *> &directories,
                           kit::DiagnosticList &diagnostics) override;

        /// Prepares the dialog for \a directory without showing it.
        void setDirectory(const kit::VoiceBankDirectorySource &directory);

        /// Prepares the dialog for \a directories without showing it.
        void setDirectories(const QList<const kit::VoiceBankDirectorySource *> &directories);

        /// The encoding of the current folder.
        QString selectedCharset() const;

        /// Sets the encoding of the folders selected, or of the current folder if none is.
        void setSelectedCharset(const QString &charset);

        /// The current folder, whose files are shown, and the folders selected.
        int currentFolder() const;
        void setCurrentFolder(int index);
        void selectFolders(const QList<int> &indices);

        /// The encoding of folder \a index, and whether it is to be read.
        QString charsetOf(int index) const;
        bool isIncluded(int index) const;
        void setIncluded(int index, bool included);

        /// The line of folder \a index in the list.
        QString folderText(int index) const;

        /// Whether candidate \a charset is grey, reading the current folder with invalid bytes.
        bool isGrey(const QString &charset) const;

        /// The titles of the tabs, one per text file.
        QStringList previewTitles() const;

        /// The text shown on tab \a index.
        QString previewText(int index) const;

    private:
        std::filesystem::path m_root;
        QList<const kit::VoiceBankDirectorySource *> m_directories;
        QStringList m_choices;
        QLabel *m_message;
        QListWidget *m_folders;
        QListWidget *m_charsets;
        QTabWidget *m_previews;
        QPushButton *m_skip;

        // Shows the encoding of the current folder, and its files in it.
        void showCurrent();
        void updateFolders();
        void updatePreview();
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H
