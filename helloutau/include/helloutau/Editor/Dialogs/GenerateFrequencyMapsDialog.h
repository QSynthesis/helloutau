#ifndef HELLOUTAU_EDITOR_DIALOGS_GENERATEFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_GENERATEFREQUENCYMAPSDIALOG_H

#include <filesystem>

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDialogButtonBox;
class QLabel;
class QPushButton;

namespace hello::daw {

    class FileCheckList;

    /// A dialog in which the user checks the audio files whose frequency maps the resampler of
    /// the settings generates. The files are listed with the presence of their maps in the format
    /// of that resampler, and of each analysis cache of the format in a column of its own. A
    /// checked file with a map has its map generated again, or only its analysis caches if the
    /// settings specify so. A checked file without a map has its map generated.
    ///
    /// The selected files are checked initially, or the files without a map if none is selected.
    /// A tri-state box checks or unchecks every file, and a button checks exactly the files
    /// without a map. The dialog changes no file.
    class HELLOUTAU_EDITOR_EXPORT GenerateFrequencyMapsDialog : public QDialog {
        Q_OBJECT
    public:
        /// An audio file of the voice bank.
        struct File {
            /// The absolute path
            std::filesystem::path path;
            /// Whether the file has a map in the format
            bool hasMap = false;
            /// Whether the file has each analysis cache of the format, in the order of the
            /// cache names of the dialog
            QList<bool> caches;
            /// Whether the file is selected in the window
            bool selected = false;
        };

        /// Creates the dialog for \a files of the voice bank at \a root, with the maps in the
        /// format named \a formatName, whose analysis caches are named \a cacheNames, such as
        /// LLSM. \a cachesOnly specifies that a file with a map has only its analysis caches
        /// generated again. The paths of the files are shown relative to \a root, or whole if
        /// they have no path relative to \a root, as on another drive.
        GenerateFrequencyMapsDialog(QList<File> files, const std::filesystem::path &root,
                                    const QString &formatName, const QStringList &cacheNames,
                                    bool cachesOnly, QWidget *parent = nullptr);
        ~GenerateFrequencyMapsDialog();

        /// Returns the checked files, absolute paths in the order of the files.
        QList<std::filesystem::path> checkedFiles() const;

        FileCheckList *fileList() const;
        /// Returns the button that checks exactly the files without a map
        QPushButton *checkMissingButton() const;
        QDialogButtonBox *buttonBox() const;

    private:
        QList<File> m_files;
        bool m_cachesOnly;

        FileCheckList *m_list;
        QPushButton *m_checkMissing;
        QLabel *m_summary;
        QDialogButtonBox *m_buttons;

        // Describes the generation of the checked files and enables the OK button if any is
        // checked
        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_GENERATEFREQUENCYMAPSDIALOG_H
