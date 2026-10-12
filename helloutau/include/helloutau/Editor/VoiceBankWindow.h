#ifndef HELLOUTAU_EDITOR_VOICEBANKWINDOW_H
#define HELLOUTAU_EDITOR_VOICEBANKWINDOW_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtWidgets/QMainWindow>

#include <hellokit/VoiceBank/VoiceBankConfiguration.h>

#include <helloutau/Editor/Dialogs/VoiceBankTranscodingDialog.h>
#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLineEdit;
class QTableView;
class QTreeWidget;

namespace QAK {
    class WidgetActionContext;
}

namespace hello::kit {
    class FrequencyMapGenerator;
    class VoiceBankDocument;
}

namespace hello::daw {

    class Editor;
    class OtoWaveformView;
    class VoiceBankEntryModel;
    class VoiceBankInfoPanel;

    /// The window of a voice bank, a document of its own beside the projects: its folders in a
    /// tree, and the oto entries of the folder chosen there in a table. Created by Editor. See
    /// docs/VoiceBankEditor.md.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankWindow : public QMainWindow {
        Q_OBJECT
    public:
        VoiceBankWindow(Editor *editor, std::unique_ptr<kit::VoiceBankDocument> document);
        ~VoiceBankWindow();

        kit::VoiceBankDocument *document() const;

        /// The actions of the window by the ids of the action extensions, those of plugins
        /// included; see ActionContribution.
        QAK::WidgetActionContext *actionContext() const;

        /// The folders: the item of all of them, then the root with its subfolders. Choosing
        /// one shows its entries.
        QTreeWidget *directoryTree() const;

        /// The entries, filtered by searchBox().
        QTableView *entryTable() const;
        VoiceBankEntryModel *entryModel() const;

        /// Filters the entries by file name and alias, as the text is typed.
        QLineEdit *searchBox() const;

        /// The box of the format of the frequency map in the sample tool bar, or null while
        /// the tool bar has none. A rebuilt tool bar has a new box.
        QComboBox *frequencyFormatBox() const;

        /// The waveform of the entry of the current row, where its values are dragged, and set
        /// at the pointer by the keys 1 to 5.
        OtoWaveformView *waveformView() const;

        /// The information of the voice bank, in a right pane that the View menu shows and hides.
        VoiceBankInfoPanel *infoPanel() const;

        /// The rows of entryModel() selected in the table, in the order of the model.
        QList<int> selectedRows() const;

        /// The row of entryModel() current in the table, or -1.
        int currentRow() const;

        /// Makes \a row of entryModel() current and the only row selected, clearing the search
        /// if it hides the row.
        void setCurrentRow(int row);

        /// Shows the entry that sings \a lyric at \a noteNum, found as the synthesis finds it
        /// (kit::VoiceBank::find()) in the voice bank as edited: chooses its folder in the tree
        /// and selects its row, clearing the search. Returns whether one was found.
        bool showEntryFor(int noteNum, const QString &lyric);

        /// Compares the voice bank with the disk now. The window also compares them when a file
        /// changes, when it is activated and every minute (Editor::watchesDisk()). See the disk
        /// changes in docs/VoiceBankEditor.md.
        /// - For a changed text file or a removed folder, a message box offers to read it again,
        ///   as one undo step. If the user declines, the change is listed in changeBar() instead,
        ///   and offered again only after the file changes again.
        /// - An added or removed audio file is taken at once, as no undo step.
        /// - A new folder, and a root that no longer exists, are listed in changeBar().
        void checkDisk();

        /// The bar above the table that lists the changes on disk not read, with the buttons
        /// that read them; hidden without any.
        QWidget *changeBar() const;

        /// Reads every folder again, whatever the stamps of its files, as one undo step.
        bool reloadAll();

        /// \name Commands
        /// Each shows its errors to the user and returns whether it completed.
        /// @{
        bool save();

        /// Saves the voice bank into another folder, which must not exist or be empty, and
        /// edits it there from now on.
        bool saveAs();

        /// Shows a file dialog for an audio file of the folder of the current row, or of the
        /// folder chosen in the tree, and inserts an entry for the file with zero values, whose
        /// alias is then edited in the table. The alias is empty, which counts as the stem of the
        /// file name, unless another entry of the file has that alias. In that case the alias is
        /// the stem with the first free number.
        bool insertEntry();

        /// Inserts a copy of each selected entry beside it, its alias followed by the first
        /// number that no entry of the file has, as one undo step.
        bool duplicateEntries();

        /// Opens VoiceAliasRuleDialog for the selected entries and inserts a copy of each with the
        /// alias that the rule derives, as one undo step. Returns false if the dialog is
        /// cancelled or the insertion fails.
        bool duplicateWithRule();

        /// Opens VoiceAliasRuleDialog for the selected entries and replaces their aliases with
        /// those that the rule derives, as one undo step. Returns false if the dialog is cancelled
        /// or the change fails.
        bool renameAliases();

        /// Includes the selected audio files without an entry, each with an empty alias and
        /// zero values, as one undo step.
        bool includeAudio();

        /// Removes the selected entries, as one undo step.
        bool removeEntries();

        /// Makes \a charset the encoding in which the files of the folder \a directory are
        /// saved, as one undo step: the text stays and the bytes change. See the section on
        /// setting an encoding in docs/Editing.md.
        bool convertCharset(const std::filesystem::path &directory, const QString &charset);

        /// Reads the folder \a directory again in \a charset, as one undo step: the bytes stay
        /// and the text changes. A folder that was not read, for want of an encoding, is read.
        bool rereadCharset(const std::filesystem::path &directory, const QString &charset);

        /// Opens VoiceBankDecodingDialog for the folders in the tree and reads each folder whose
        /// encoding the user changes again in the new encoding, as one undo step. Folders without
        /// text files and folders whose \c oto.ini declares UTF-8 are not listed, and a message
        /// box replaces the dialog if no folder remains. Returns false if no folder remains, the
        /// dialog is cancelled, no encoding is changed or a folder is not read.
        bool rereadCharsets();

        /// \overload for the folders and encodings of \a charsets , without the dialog.
        bool rereadCharsets(const QList<std::pair<std::filesystem::path, QString>> &charsets);

        /// Returns the number of names of the folders in the tree that their records of
        /// misdecoded file names list in neither list and that restore, which the bar of the
        /// window reports. See the section on misdecoded file names in docs/Editing.md.
        int fileNamesToDetermine() const;

        /// Opens VoiceBankDecodingDialog for the file names to determine under the records of the
        /// tree, and sets the records answered, as one undo step. A message box replaces the
        /// dialog if no name is to be determined. Returns false if no name is to be determined,
        /// the dialog is declined or a record is not set.
        bool determineFileNames();

        /// \overload for the folders and records of \a records , without the dialog.
        bool determineFileNames(
            const QList<std::pair<std::filesystem::path,
                                  kit::VoiceBankConfiguration::FileNameEncoding>> &records);

        /// Opens VoiceBankTranscodingDialog for the folders of the voice bank, those of the tree
        /// and those not read, and makes the conversion chosen. A renaming is first confirmed in
        /// a message box. A message box replaces the dialog if no folder has text files or a name
        /// that is not ASCII. Returns false if no folder remains, the dialog is cancelled, the
        /// renaming is declined or the conversion fails.
        bool convertEncodings();

        /// \overload for \a conversion , without the dialog and the confirmation.
        ///
        /// The text files and the names read as misdecoded are converted as one undo step.
        /// Renaming saves the voice bank, renames the names on disk at once and replaces the
        /// window with a new window of the voice bank read again (Editor::reopenVoiceBank()). See
        /// the section on the conversion in docs/Editing.md.
        bool convertEncodings(const VoiceBankTranscodingDialog::Conversion &conversion);

        /// The audio files whose frequency maps generateFrequencyMaps() generates.
        enum FrequencyMapScope {
            MissingFrequencyMaps,  ///< Every audio file without a map
            SelectedFrequencyMaps, ///< The audio files of the selected rows, generated again
            AllFrequencyMaps,      ///< Every audio file, generated again
        };

        /// Generates the frequency maps of the audio files of \a scope with the resampler of the
        /// settings, in the format of that resampler, with a progress dialog that cancels the
        /// generation, with the threads of FrequencyMapSettings::threadCount(). Generating a map
        /// again removes it first with its analysis caches, or removes only the caches if
        /// FrequencyMapSettings::regeneration() specifies so and the format has caches, which
        /// AllFrequencyMaps confirms in a message box. The audio files of the folders not read
        /// are included. See the generation in docs/FrequencyMaps.md.
        ///
        /// \return whether every map was generated
        bool generateFrequencyMaps(FrequencyMapScope scope);

        /// Removes the frequency maps of every audio file of the voice bank in the format of the
        /// resampler of the settings, after a confirmation. No undo restores them.
        bool removeFrequencyMaps();

        /// The generator of generateFrequencyMaps(), whose process a test replaces.
        kit::FrequencyMapGenerator &frequencyMapGenerator();

        /// Lists the audio files of the folders that contain metadata, see kit::WaveMetadata, in
        /// a message box in which the user confirms writing them again without it. The files are
        /// written at once, and no undo reverts the writing.
        ///
        /// \return the number of files written, or \c std::nullopt if the user declined or no
        ///         file carries metadata
        std::optional<int> removeAudioMetadata();
        /// @}

    protected:
        void closeEvent(QCloseEvent *event) override;
        void changeEvent(QEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKWINDOW_H
