#ifndef HELLOUTAU_EDITOR_VOICEBANKWINDOW_H
#define HELLOUTAU_EDITOR_VOICEBANKWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLineEdit;
class QTableView;
class QTreeWidget;

namespace hello::kit {
    class VoiceBankDocument;
}

namespace hello::daw {

    class Editor;
    class VoiceBankEntryModel;

    /// The window of a voice bank, a document of its own beside the projects: its folders in a
    /// tree, and the oto entries of the folder chosen there in a table. Created by Editor. See
    /// docs/VoiceBankEditor.md.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankWindow : public QMainWindow {
        Q_OBJECT
    public:
        VoiceBankWindow(Editor *editor, std::unique_ptr<kit::VoiceBankDocument> document);
        ~VoiceBankWindow();

        kit::VoiceBankDocument *document() const;

        /// The folders: the item of all of them, then the root with its subfolders. Choosing
        /// one shows its entries.
        QTreeWidget *directoryTree() const;

        /// The entries, filtered by searchBox().
        QTableView *entryTable() const;
        VoiceBankEntryModel *entryModel() const;

        /// Filters the entries by file name and alias, as the text is typed.
        QLineEdit *searchBox() const;

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
