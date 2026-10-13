#ifndef HELLOUTAU_EDITOR_DIALOGS_VOICEBANKDECODINGDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_VOICEBANKDECODINGDIALOG_H

#include <filesystem>
#include <optional>

#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/MisdecodedNameQuestion.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;

namespace hello::daw {

    /// A dialog in which the user chooses, at once, the encodings of the folders of a voice bank
    /// whose encoding no file on disk determines, and determines the misdecoded file names of
    /// its folders.
    ///
    /// The candidates are those of \c TextCodec::ustCandidates(), since a voice bank and a UST
    /// pose the same problem. The folders are listed, each with its encoding and checked to be
    /// read. Choosing an encoding sets it for the selected folders, initially all of them. The
    /// encoding with the fewest invalid byte sequences in all folders is initially selected, see
    /// TextCodec::ranked(). A folder with invalid byte sequences in that encoding instead
    /// receives the encoding with the fewest invalid byte sequences in that folder. Each text file
    /// of the current folder is shown in its encoding on a tab of its own, whose title states
    /// the number of invalid byte sequences, and the candidates in which the folder has invalid
    /// byte sequences are gray. An unchecked folder, or every folder if the dialog is declined,
    /// is left out, and its samples are then found by file name only. See the section on choosing
    /// the encoding in docs/Editing.md.
    ///
    /// A folder with file names to determine has a further tab, which lists the names of its
    /// kit::MisdecodedNameQuestion under the encoding of the folder, each checked if it is
    /// misdecoded. The question follows the encoding of the folder. A folder whose encoding a file
    /// on disk determines is listed with that encoding, which cannot be changed, if it has file
    /// names to determine. See the section on misdecoded file names in docs/Editing.md.
    ///
    /// \sa setCurrentCharsets() for reading the folders of an open voice bank again.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankDecodingDialog : public QDialog,
                                                            public kit::VoiceBankCharsetSelector {
        Q_OBJECT
    public:
        explicit VoiceBankDecodingDialog(QWidget *parent = nullptr);
        ~VoiceBankDecodingDialog();

        /// Sets the voice bank whose folders are asked about, which the dialog names.
        void setRoot(const std::filesystem::path &root);

        /// Shows the dialog for \a directory and waits for the answer.
        std::optional<QString> selectCharset(const kit::VoiceBankDirectorySource &directory,
                                             kit::DiagnosticList &diagnostics) override;

        /// Shows the dialog for \a directories and waits for the answers.
        QList<std::optional<QString>>
            selectCharsets(const QList<const kit::VoiceBankDirectorySource *> &directories,
                           kit::DiagnosticList &diagnostics) override;

        /// Shows the dialog for \a questions and waits for the answers.
        QList<Answer> selectEncodings(const QList<Question> &questions,
                                      kit::DiagnosticList &diagnostics) override;

        /// Prepares the dialog for \a directory without showing it.
        void setDirectory(const kit::VoiceBankDirectorySource &directory);

        /// Prepares the dialog for \a directories without showing it.
        void setDirectories(const QList<const kit::VoiceBankDirectorySource *> &directories);

        /// Prepares the dialog for \a questions without showing it.
        void setQuestions(const QList<Question> &questions);

        /// Returns the answers to the questions of setQuestions() if the dialog is accepted.
        QList<Answer> answers() const;

        /// Prepares the dialog for reading the folders of setDirectories() again, which were read
        /// in \a charsets . The initial encoding of each folder is its encoding in \a charsets ,
        /// no folder can be left out, and changedFolders() returns the folders whose encoding the
        /// user changes.
        void setCurrentCharsets(const QStringList &charsets);

        /// The folders whose encoding differs from the encoding given to setCurrentCharsets().
        QList<int> changedFolders() const;

        /// The encoding of the current folder.
        QString selectedCharset() const;

        /// Sets the encoding of the folders selected, or of the current folder if none is. A
        /// folder whose encoding a file on disk determines keeps it.
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

        /// Returns whether candidate \a charset is gray, reading the current folder with invalid
        /// bytes.
        bool isGrey(const QString &charset) const;

        /// The titles of the tabs: a tab per text file, and a tab for the file names if the
        /// current folder has file names to determine.
        QStringList previewTitles() const;

        /// The text shown on tab \a index.
        QString previewText(int index) const;

        /// \name The file names of a folder
        /// @{

        /// The question about the file names of folder \a index under its encoding, or
        /// \c std::nullopt if it has no file names to determine.
        const std::optional<kit::MisdecodedNameQuestion> &misdecodedNameQuestion(int index) const;

        /// Chooses the encodings of the file names of folder \a index , a folder without a
        /// record, over the proposed ones.
        void setFileNameEncodings(int index, const QString &original, const QString &misdecodedAs);

        /// The names of folder \a index checked as misdecoded.
        QStringList misdecodedNames(int index) const;
        void setMisdecoded(int index, const QString &name, bool misdecoded);

        /// Returns whether the names of folder \a index, a folder without a record, are used as
        /// they are, which records the record without encodings. Initially whether the question
        /// proposes the names as they are, see kit::MisdecodedNameQuestion::Found.
        bool namesAsTheyAre(int index) const;
        void setNamesAsTheyAre(int index, bool asTheyAre);

        /// @}

    private:
        /// The file names of one folder.
        struct FileNames {
            std::optional<kit::MisdecodedNameQuestion> question;
            /// The encodings chosen by the user, empty for the proposed ones.
            QString original;
            QString misdecodedAs;
            QStringList misdecoded;
            bool asTheyAre = false;
        };

        std::filesystem::path m_root;
        QList<const kit::VoiceBankDirectorySource *> m_directories;
        QStringList m_choices;
        // The encoding that a file on disk determines for each folder, or none if chosen
        QList<std::optional<QString>> m_settled;
        QList<FileNames> m_fileNames;
        // The encodings of setCurrentCharsets(), empty for choosing the encodings at opening
        QStringList m_current;
        QLabel *m_message;
        QListWidget *m_folders;
        QListWidget *m_charsets;
        QTabWidget *m_previews;
        QPushButton *m_skip;

        // The page of the file names of the current folder
        QWidget *m_names;
        QLabel *m_namesBasis;
        QComboBox *m_original;
        QComboBox *m_misdecodedAs;
        QCheckBox *m_asTheyAre;
        QTreeWidget *m_nameList;

        // Shows the encoding of the current folder, and its files in it.
        void showCurrent();
        void updateFolders();
        void updatePreview();
        void updateMessage();

        // Asks the question of folder index again under its encoding, with the default choices.
        void askFileNames(int index);
        // Shows the file names of the current folder on their page.
        void showFileNames();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_VOICEBANKDECODINGDIALOG_H
