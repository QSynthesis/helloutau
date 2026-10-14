#ifndef HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H

#include <filesystem>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSplitter;

namespace hello::kit {
    class FrequencyFormat;
}

namespace hello::daw {

    class FileCheckList;

    /// A dialog in which the user checks the audio files whose frequency maps are converted, and
    /// chooses the format from which the maps are converted and the format into which they are
    /// converted. The files are listed on the left with a column for each format, which shows
    /// whether the file has a map of that format. The formats are chosen on the right, beyond a
    /// splitter. The target formats are the formats that write maps.
    ///
    /// The selected files are checked initially. If none is selected, the files with a map of the
    /// source format are checked again whenever the source format changes. A tri-state box checks
    /// or unchecks every file, and a button checks exactly the files with a map of the source
    /// format. The OK button is enabled if a file is checked and the two formats differ. The
    /// dialog changes no file.
    class HELLOUTAU_EDITOR_EXPORT ConvertFrequencyMapsDialog : public QDialog {
        Q_OBJECT
    public:
        /// An audio file of the voice bank.
        struct File {
            /// The absolute path
            std::filesystem::path path;
            /// Whether the file has a map of each format, in the order of the formats of the
            /// dialog
            QList<bool> maps;
            /// Whether the file is selected in the window
            bool selected = false;
        };

        /// Creates the dialog for \a files of the voice bank at \a root, whose paths are shown
        /// relative to \a root, and for the formats \a formats, with \a source as the initial
        /// source format. The initial target format is the first format of \a formats that writes
        /// maps and differs from \a source.
        ConvertFrequencyMapsDialog(QList<File> files, const std::filesystem::path &root,
                                   const QList<kit::FrequencyFormat *> &formats,
                                   const kit::FrequencyFormat *source, QWidget *parent = nullptr);
        ~ConvertFrequencyMapsDialog();

        /// Returns the source format, or null if \a formats is empty.
        kit::FrequencyFormat *sourceFormat() const;

        /// Returns the target format, or null if no format writes maps.
        kit::FrequencyFormat *targetFormat() const;

        /// Returns the checked files, absolute paths in the order of the files.
        QList<std::filesystem::path> checkedFiles() const;

        FileCheckList *fileList() const;
        /// Returns the button that checks exactly the files with a map of the source format
        QPushButton *checkMapsButton() const;
        QComboBox *sourceBox() const;
        QComboBox *targetBox() const;
        /// Returns the splitter between the files and the formats
        QSplitter *splitter() const;
        QDialogButtonBox *buttonBox() const;

    private:
        QList<File> m_files;
        QList<kit::FrequencyFormat *> m_formats;
        bool m_anySelected = false;

        FileCheckList *m_list;
        QSplitter *m_splitter;
        QPushButton *m_checkMaps;
        QComboBox *m_source;
        QComboBox *m_target;
        QLabel *m_summary;
        QDialogButtonBox *m_buttons;

        // Returns whether the file of row has a map of the source format
        bool hasSourceMap(int row) const;

        // Describes the conversion of the checked files and enables the OK button if a file is
        // checked and the two formats differ
        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H
