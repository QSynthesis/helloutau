#ifndef HELLOUTAU_EDITOR_DIALOGS_REMOVEFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_REMOVEFREQUENCYMAPSDIALOG_H

#include <filesystem>

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QPushButton;

namespace hello::daw {

    class FileCheckList;

    /// A dialog in which the user checks the audio files whose frequency maps are deleted with
    /// their analysis caches. The files are listed with the presence of their maps in the format
    /// of the resampler of the settings, and of each analysis cache of the format in a column of
    /// its own.
    ///
    /// The selected files are checked initially, and no file if none is selected. A tri-state box
    /// checks or unchecks every file, and a button checks exactly the files with a map. The
    /// delete button is enabled if a file is checked. The dialog changes no file.
    class HELLOUTAU_EDITOR_EXPORT RemoveFrequencyMapsDialog : public QDialog {
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
        /// format named \a formatName, whose analysis caches are named \a cacheNames. The paths of
        /// the files are shown relative to \a root, or whole if they have no path relative to
        /// \a root, as on another drive.
        RemoveFrequencyMapsDialog(QList<File> files, const std::filesystem::path &root,
                                  const QString &formatName, const QStringList &cacheNames,
                                  QWidget *parent = nullptr);
        ~RemoveFrequencyMapsDialog();

        /// Returns the checked files, absolute paths in the order of the files.
        QList<std::filesystem::path> checkedFiles() const;

        FileCheckList *fileList() const;
        /// Returns the button that checks exactly the files with a map
        QPushButton *checkMapsButton() const;
        /// Returns the button that accepts the dialog
        QPushButton *deleteButton() const;

    private:
        QList<File> m_files;

        FileCheckList *m_list;
        QPushButton *m_checkMaps;
        QLabel *m_summary;
        QPushButton *m_delete;

        // Describes the deletion of the checked files and enables the delete button if any is
        // checked
        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_REMOVEFREQUENCYMAPSDIALOG_H
