#ifndef HELLOUTAU_EDITOR_DIALOGS_REPAIRFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_REPAIRFREQUENCYMAPSDIALOG_H

#include <filesystem>
#include <map>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QListWidget;
class QPushButton;

namespace hello::daw {

    /// Lists the files of the maps and the analysis caches of a frequency map format in the
    /// folders of a voice bank, one column for the files that hold maps and one column for each
    /// cache, so that the user checks the files to repair. The action on a checked file is shown
    /// above each column and beside each damaged file. A damaged file is checked at first. A file
    /// that holds the maps of several audio files can be checked only if damaged, and is listed as
    /// missing if it does not exist. See the repair in docs/FrequencyMaps.md.
    class HELLOUTAU_EDITOR_EXPORT RepairFrequencyMapsDialog : public QDialog {
        Q_OBJECT
    public:
        /// The files of each folder, by the folder
        using Files = std::map<std::filesystem::path, QList<kit::FrequencyFormats::MapFile>>;

        /// Lists \a files of \a format, each named relative to \a root.
        RepairFrequencyMapsDialog(const kit::FrequencyFormat &format,
                                  const std::filesystem::path &root, Files files,
                                  QWidget *parent = nullptr);
        ~RepairFrequencyMapsDialog();

        /// Returns the checked files by folder, without the folders that have none.
        Files checkedFiles() const;

        /// Returns the number of columns: the files that hold maps, then one column for each
        /// analysis cache of the format.
        int columnCount() const;

        /// Returns the list of the column \a column, an item for each file in the order of the
        /// folders and of the files of each folder.
        QListWidget *list(int column) const;

        QLabel *summary() const;
        QPushButton *repairButton() const;

    private:
        Files m_files;
        // The list of each column, and the folder and the index in the folder of each item
        QList<QListWidget *> m_lists;
        QList<QList<std::pair<std::filesystem::path, qsizetype>>> m_items;
        QLabel *m_summary;
        QPushButton *m_repair;

        void updateSummary();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_REPAIRFREQUENCYMAPSDIALOG_H
