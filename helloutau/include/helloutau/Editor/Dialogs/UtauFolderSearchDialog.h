#ifndef HELLOUTAU_EDITOR_DIALOGS_UTAUFOLDERSEARCHDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_UTAUFOLDERSEARCHDIALOG_H

#include <filesystem>
#include <optional>

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/UtauFolderSearch.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QPushButton;
class QThread;
class QTreeWidget;

namespace hello::daw {

    /// A dialog that lists the UTAU folders that UtauFolderSearch finds, from which the user
    /// chooses the UTAU folder of the settings. See docs/UtauSearch.md.
    ///
    /// start() runs the search on a thread of its own, and the candidates are added as they are
    /// found. The dialog stops waiting after timeout(), after which a candidate is no longer
    /// added. The thread ends and deletes itself once the search finishes, which the dialog
    /// interrupts when it stops waiting or is destroyed.
    class HELLOUTAU_EDITOR_EXPORT UtauFolderSearchDialog : public QDialog {
        Q_OBJECT
    public:
        explicit UtauFolderSearchDialog(QWidget *parent = nullptr);
        ~UtauFolderSearchDialog();

        /// The time in milliseconds after which the dialog stops waiting, 1000 by default
        int timeout() const;
        void setTimeout(int milliseconds);

        /// Starts the search.
        void start();

        /// Returns whether the dialog waits for the search.
        bool isSearching() const;

        /// Adds \a candidate to the list, as the search does. The first candidate is selected.
        void addCandidate(const UtauFolderSearch::Candidate &candidate);

        /// Returns the folder of the selected candidate, or \c std::nullopt if none is.
        std::optional<std::filesystem::path> selectedFolder() const;

        /// The rows of the list, each with its folder, version and source separated by tabs
        QStringList rows() const;

        /// The line that reports the state of the search
        QString statusText() const;

    private:
        QTreeWidget *m_list;
        QLabel *m_status;
        QPushButton *m_use;
        QPointer<QThread> m_thread;
        int m_timeout = 1000;
        bool m_searching = false;

        // Stops waiting for the search, which is asked to end.
        void stopWaiting();
        void updateStatus();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_UTAUFOLDERSEARCHDIALOG_H
