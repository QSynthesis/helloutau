#ifndef HELLOUTAU_EDITOR_DIALOGS_PROCESSAUDIOFILESDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_PROCESSAUDIOFILESDIALOG_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/AudioFile/AudioFileProcessor.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTreeWidget;

namespace hello::daw {

    class FileCheckList;

    /// A dialog in which the user chooses the processing of audio files and the files to
    /// process, see kit::AudioFileProcessor. The files are listed on the left, and the options
    /// on the right, beyond a splitter. The selected files are checked initially. If none is
    /// selected, the files that the options change are checked, again whenever an option changes.
    /// A tri-state box checks or unchecks every file, and a button checks exactly the files that
    /// the options change.
    ///
    /// The dialog also offers the removal of the frequency maps and the analysis caches of the
    /// processed files, checked initially, and a backup of the original files into a folder,
    /// checked initially. The dialog changes no file.
    class HELLOUTAU_EDITOR_EXPORT ProcessAudioFilesDialog : public QDialog {
        Q_OBJECT
    public:
        /// An audio file and its properties.
        struct File {
            /// The absolute path
            std::filesystem::path path;
            kit::AudioFileProcessor::Inspection inspection;
            /// Whether the file is selected in the window
            bool selected = false;
        };

        /// Creates the dialog for \a files of the voice bank at \a root, whose paths are shown
        /// relative to \a root, with \a backup as the initial backup folder.
        ProcessAudioFilesDialog(QList<File> files, const std::filesystem::path &root,
                                const std::filesystem::path &backup, QWidget *parent = nullptr);
        ~ProcessAudioFilesDialog();

        kit::AudioFileProcessor::Options options() const;
        void setOptions(const kit::AudioFileProcessor::Options &options);

        /// Returns the checked files, absolute paths in the order of the files.
        QList<std::filesystem::path> checkedFiles() const;

        /// Returns whether the frequency maps and the analysis caches of the processed files
        /// are to be removed in every format.
        bool removesFrequencyMaps() const;

        /// Returns the folder to which the original files are copied, or \c std::nullopt if
        /// the backup is unchecked.
        std::optional<std::filesystem::path> backupFolder() const;

        QTreeWidget *fileList() const;
        /// The button that checks exactly the files that the options change
        QPushButton *checkFilesButton() const;
        /// The splitter between the files and the options
        QSplitter *splitter() const;
        QLineEdit *backupFolderEdit() const;
        QDialogButtonBox *buttonBox() const;

    private:
        QList<File> m_files;
        std::filesystem::path m_root;
        bool m_anySelected = false;

        QCheckBox *m_resampling;
        QComboBox *m_sampleRate;
        QCheckBox *m_mono;
        QComboBox *m_channelMix;
        QCheckBox *m_sixteenBits;
        QCheckBox *m_dithered;
        QCheckBox *m_removingMetadata;
        QCheckBox *m_plainHeader;
        QCheckBox *m_removingDcOffset;
        QCheckBox *m_scalingPeak;
        QDoubleSpinBox *m_peakLevel;
        FileCheckList *m_list;
        QSplitter *m_splitter;
        QPushButton *m_checkFiles;
        QCheckBox *m_removingMaps;
        QCheckBox *m_backingUp;
        QLineEdit *m_backupFolder;
        QLabel *m_summary;
        QDialogButtonBox *m_buttons;
        bool m_applying = false;

        // Describes the changes of the files, and checks the files that the options change if no
        // file is selected
        void refreshFiles();

        // Shows the number of checked files and enables the OK button if any is checked and the
        // backup has a folder
        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_PROCESSAUDIOFILESDIALOG_H
