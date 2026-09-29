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

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKWINDOW_H
