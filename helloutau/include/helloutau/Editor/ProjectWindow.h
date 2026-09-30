#ifndef HELLOUTAU_EDITOR_PROJECTWINDOW_H
#define HELLOUTAU_EDITOR_PROJECTWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class WidgetActionContext;
}

namespace hello::kit {
    class ProjectDocument;
}

namespace hello::daw {

    class Editor;
    class PianoRoll;

    /// The window of a project, which edits it. Created by Editor.
    class HELLOUTAU_EDITOR_EXPORT ProjectWindow : public QMainWindow {
        Q_OBJECT
    public:
        ProjectWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document);
        ~ProjectWindow();

        kit::ProjectDocument *document() const;

        /// The actions of the window by the ids of the action extensions, those of plugins
        /// included; see ActionContribution.
        QAK::WidgetActionContext *actionContext() const;

        /// The piano roll of the project, which holds the selection. It is replaced with the
        /// document; see documentChanged().
        PianoRoll *pianoRoll() const;

        /// Replaces the project of the window with \a document, and its piano roll.
        void setDocument(std::unique_ptr<kit::ProjectDocument> document);

        /// Returns whether the window shows a new project that has not been modified, which a
        /// project opened from a file may replace.
        bool isUnused() const;

        /// Reads the voice bank of the project from the UTAU folder of the settings, asking the
        /// user for the encoding of each folder that does not state it, and shows any problem.
        /// The piano roll then marks the notes that have no sample.
        ///
        /// \return whether a voice bank was read
        bool loadVoiceBank();

        /// Plays in the playback mode of the settings, with their engines, after they changed:
        /// what plays in the other mode stops, and in the realtime mode the track is rendered
        /// in the background.
        void applySettings();

        /// \name Commands
        /// The commands of the menus. Each shows its errors to the user and returns whether it
        /// completed.
        /// @{
        bool save();
        bool saveAs();
        bool exportUst();
        /// @}

    Q_SIGNALS:
        /// Emitted after setDocument() replaced the document and the piano roll.
        void documentChanged();

    protected:
        void closeEvent(QCloseEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PROJECTWINDOW_H
