#ifndef HELLOUTAU_EDITOR_PROJECTWINDOW_H
#define HELLOUTAU_EDITOR_PROJECTWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QDragEnterEvent;
class QDropEvent;

namespace QAK {
    class WidgetActionContext;
}

namespace hello::kit {
    class ProjectDocument;
}

namespace hello::daw {

    class Editor;
    class PianoRoll;

    /// Window that edits a project. Instances are created by Editor.
    class HELLOUTAU_EDITOR_EXPORT ProjectWindow : public QMainWindow {
        Q_OBJECT
    public:
        ProjectWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document);
        ~ProjectWindow();

        /// Returns the editor of the window, through which a plugin reads the settings.
        Editor *editor() const;

        kit::ProjectDocument *document() const;

        /// Refreshes the title after another project window changes the names that need showing.
        void refreshTitle();

        /// Returns the action context of the window, keyed by the IDs of the action extensions,
        /// including the extensions of plugins. See ActionContribution.
        QAK::WidgetActionContext *actionContext() const;

        /// Returns the piano roll of the project, which holds the selection. The piano roll is
        /// replaced together with the document, see documentChanged().
        PianoRoll *pianoRoll() const;

        /// Returns the box of the quantization in the tool bar, or null while the tool bar has
        /// none. A rebuilt tool bar has a new box.
        QComboBox *quantizationBox() const;

        /// Replaces the project of the window with \a document, together with its piano roll.
        void setDocument(std::unique_ptr<kit::ProjectDocument> document);

        /// Returns whether the window shows an unmodified new project, which a project opened
        /// from a file may replace.
        bool isUnused() const;

        /// Reads the voice bank of the project from the UTAU folder of the settings, prompting
        /// the user for the encoding of each folder that does not record its encoding, and shows
        /// any problem. The piano roll then marks the notes that have no sample.
        ///
        /// \return whether a voice bank was read
        bool loadVoiceBank();

        /// Opens Project Properties when a configured voice folder or engine path is missing.
        void showPropertiesIfPathsAreInvalid();

        /// Applies the playback mode and the engines of the settings after a change. Playback in
        /// the other mode stops. In the realtime mode, the track is rendered in the background.
        void applySettings();

        /// \name Commands
        /// Commands of the menus. Each command shows its errors to the user and returns whether
        /// it completed.
        /// @{
        bool save();
        bool saveAs();
        /// Asks whether modified changes may be discarded or saved before replacing the project.
        bool maybeSave();
        bool exportUst();
        /// @}

    Q_SIGNALS:
        /// Emitted after setDocument() has replaced the document and the piano roll.
        void documentChanged();

    protected:
        void closeEvent(QCloseEvent *event) override;

        /// The files dropped on the window open as by Open of the File menu, see
        /// Editor::openFile(): in this window if it shows an unmodified new project, and in
        /// windows of their own otherwise. A file that a window shows already activates that
        /// window.
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PROJECTWINDOW_H
