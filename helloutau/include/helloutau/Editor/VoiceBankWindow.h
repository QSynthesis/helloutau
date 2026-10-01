#ifndef HELLOUTAU_EDITOR_VOICEBANKWINDOW_H
#define HELLOUTAU_EDITOR_VOICEBANKWINDOW_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtWidgets/QMainWindow>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLineEdit;
class QTableView;
class QTreeWidget;

namespace QAK {
    class WidgetActionContext;
}

namespace hello::kit {
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

        /// The box of the format of the frequency table in the sample tool bar, or null while
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

        /// Compares the voice bank with the disk now, as the window does on its own when a file
        /// changes, when it is activated and every minute (Editor::watchesDisk()). See the disk
        /// changes in docs/VoiceBankEditor.md:
        /// - a changed text file or a removed folder asks whether to read it again, as one undo
        ///   step; declined, the same change is listed in changeBar() instead, and asked about
        ///   again only once the file changes again;
        /// - an audio file added or removed is taken at once, as no undo step;
        /// - a new folder, and a root that no longer exists, are listed in changeBar().
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

        /// Asks for an audio file of the folder of the current row, or of the folder chosen in
        /// the tree, and inserts an entry for it with zero values, whose alias the table then
        /// edits. The alias is empty, which counts as the stem of the file name, unless another
        /// entry of the file has that name; then the stem with the first free number.
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

        /// Lists the audio files of the folders that carry metadata, see kit::WaveMetadata, and
        /// asks whether to write them again without it. The files are written at once, which
        /// no undo reverts.
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
