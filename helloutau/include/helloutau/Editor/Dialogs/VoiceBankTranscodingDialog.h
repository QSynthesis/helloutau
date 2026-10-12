#ifndef HELLOUTAU_EDITOR_DIALOGS_VOICEBANKTRANSCODINGDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_VOICEBANKTRANSCODINGDIALOG_H

#include <filesystem>
#include <optional>
#include <utility>

#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/MisdecodedNameConversion.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QTreeWidget;

namespace hello::daw {

    /// A dialog in which the user converts the encodings of a voice bank: the encoding in which
    /// the text files of each folder are saved, and the misdecoded file names of each folder. See
    /// the section on the conversion in docs/Editing.md.
    ///
    /// The folders are listed on the left. For the current folder, the right side shows the
    /// encoding of its text files, which is converted only if checked, and the names that its
    /// kit::MisdecodedNameConversion lists under the two encodings of the folder, each checked if
    /// proposed as misdecoded. The method applies to the checked names of every folder.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankTranscodingDialog : public QDialog {
        Q_OBJECT
    public:
        /// The method of the file names.
        enum Method {
            KeepNames,        ///< The file names are not converted
            ReadAsMisdecoded, ///< The checked names are recorded as misdecoded, the others correct
            Rename,           ///< The checked names are renamed on disk at once
        };

        /// One folder of the voice bank.
        struct Folder {
            /// The names of the folder and the state on which their conversion depends
            kit::MisdecodedNameConversion::Listing listing;
            /// The name under which the window shows the folder
            QString name;
            /// Whether the folder was not read for want of an encoding. Its text files cannot be
            /// converted, and its names can be renamed but not recorded.
            bool excluded = false;
        };

        /// The conversion that the user chose.
        struct Conversion {
            /// The folders whose text files are saved in another encoding, with the encodings
            QList<std::pair<std::filesystem::path, QString>> charsets;
            Method method = KeepNames;
            /// The folders whose names are converted, each with its checked names, empty if
            /// \a method is KeepNames
            QList<std::pair<kit::MisdecodedNameConversion, QStringList>> names;
        };

        explicit VoiceBankTranscodingDialog(QWidget *parent = nullptr);
        ~VoiceBankTranscodingDialog();

        /// Prepares the dialog for \a folders of the voice bank at \a root .
        void setFolders(const std::filesystem::path &root, const QList<Folder> &folders);

        int folderCount() const;

        /// The line of folder \a index in the list.
        QString folderText(int index) const;

        /// The current folder, whose text files and names are shown.
        int currentFolder() const;
        void setCurrentFolder(int index);

        /// The encoding in which the text files of folder \a index are saved, empty if they are
        /// not converted. Setting the encoding in which they are written clears it.
        QString textCharset(int index) const;
        void setTextCharset(int index, const QString &charset);

        /// \name The file names of a folder
        /// @{

        /// The conversion of the names of folder \a index under its current encodings.
        const kit::MisdecodedNameConversion &nameConversion(int index) const;

        /// Chooses the encodings of the names of folder \a index , which checks the names anew.
        void setFileNameEncodings(int index, const QString &original, const QString &misdecodedAs);

        /// The names of folder \a index checked as misdecoded.
        QStringList checkedNames(int index) const;
        void setChecked(int index, const QString &name, bool checked);

        /// @}

        Method method() const;
        void setMethod(Method method);

        /// Returns the conversion chosen. A folder that is excluded, or that has no name to
        /// convert, contributes no names to a method other than Rename.
        Conversion conversion() const;

    private:
        // The choices of one folder
        struct State {
            Folder folder;
            QString textCharset;
            std::optional<kit::MisdecodedNameConversion> names;
            QStringList checked;
        };

        std::filesystem::path m_root;
        QList<State> m_states;
        QLabel *m_message;
        QListWidget *m_folders;

        QGroupBox *m_text;
        QLabel *m_textNote;
        QCheckBox *m_convertText;
        QComboBox *m_textCharset;

        QGroupBox *m_names;
        QLabel *m_namesNote;
        QComboBox *m_original;
        QComboBox *m_misdecodedAs;
        QTreeWidget *m_nameList;

        QComboBox *m_method;
        QLabel *m_methodNote;

        // Checks the names of folder index as its conversion proposes.
        void checkProposed(int index);
        void updateFolders();
        // Shows the text files and the names of the current folder.
        void showCurrent();
        void updateMethod();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_VOICEBANKTRANSCODINGDIALOG_H
