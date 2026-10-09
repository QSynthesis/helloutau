#ifndef HELLOUTAU_EDITOR_DIALOGS_REGIONDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_REGIONDIALOG_H

#include <optional>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/Document/Project.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QPushButton;
class QTreeWidget;

namespace hello::daw {

    /// Lists the regions of a track from top to bottom, each with its name and its notes, and
    /// emits goToRequested() and removeRequested() for the current region. The owner performs
    /// the edit and passes the remaining regions to setRegions().
    class HELLOUTAU_EDITOR_EXPORT RegionDialog : public QDialog {
        Q_OBJECT
    public:
        explicit RegionDialog(QWidget *parent = nullptr);
        ~RegionDialog();

        QList<kit::Region> regions() const;

        /// Sets the listed regions. The current region remains current if it is among them.
        /// Otherwise the region in the same row becomes current, or the last region if the list
        /// has become shorter, so that a removal moves to the next region.
        void setRegions(const QList<kit::Region> &regions);

        /// Returns the current region, or \c std::nullopt if none is current.
        std::optional<kit::Region> currentRegion() const;

        /// Makes \a region current if it is listed.
        void setCurrentRegion(const kit::Region &region);

        QTreeWidget *list() const;
        QPushButton *goToButton() const;
        QPushButton *removeButton() const;

    Q_SIGNALS:
        /// The notes of \a region are to be selected and shown: "Go To" was clicked or the
        /// region was double-clicked.
        void goToRequested(const hello::kit::Region &region);

        /// \a region is to be removed: "Remove" was clicked.
        void removeRequested(const hello::kit::Region &region);

    private:
        QList<kit::Region> m_regions;
        QTreeWidget *m_list;
        QPushButton *m_goTo;
        QPushButton *m_remove;

        void updateButtons();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_REGIONDIALOG_H
