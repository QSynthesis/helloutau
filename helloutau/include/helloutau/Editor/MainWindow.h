#ifndef HELLOUTAU_EDITOR_MAINWINDOW_H
#define HELLOUTAU_EDITOR_MAINWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class ProjectDocument;
}

namespace hello::daw {

    class Editor;

    /// The editor window, which edits one project. Created by Editor.
    class HELLOUTAU_EDITOR_EXPORT MainWindow : public QMainWindow {
        Q_OBJECT
    public:
        MainWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document);
        ~MainWindow();

        kit::ProjectDocument *document() const;

        /// Replaces the project of the window with \a document.
        void setDocument(std::unique_ptr<kit::ProjectDocument> document);

        /// Returns whether the window shows a new project that has not been modified, which a
        /// project opened from a file may replace.
        bool isUnused() const;

        /// \name Commands
        /// The commands of the menus. Each shows its errors to the user and returns whether it
        /// completed.
        /// @{
        bool save();
        bool saveAs();
        bool exportUst();
        /// @}

    protected:
        void closeEvent(QCloseEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_MAINWINDOW_H
