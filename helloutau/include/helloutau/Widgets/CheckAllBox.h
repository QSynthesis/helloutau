#ifndef HELLOUTAU_WIDGETS_CHECKALLBOX_H
#define HELLOUTAU_WIDGETS_CHECKALLBOX_H

#include <QtCore/QPointer>
#include <QtWidgets/QCheckBox>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QAbstractItemView;

namespace hello::daw {

    /// A tri-state check box that checks or unchecks every checkable item of a list at once, for
    /// a dialog that lists many files or folders to check. The state of the box indicates whether
    /// all, none or some of the checkable items in a column of the top-level rows of a view are
    /// checked. A partially checked item, such as the item of a folder with only some of its
    /// files checked, makes the box partially checked. A click checks every checkable item, or
    /// unchecks every checkable item if all are checked. An item that is not checkable or not
    /// enabled is left alone. The box is disabled while the view is disabled or has no checkable
    /// item.
    ///
    /// Each item is changed through the model of the view, so that the view emits the signal of
    /// each changed item. The model of the view must not be replaced.
    class HELLOUTAU_WIDGETS_EXPORT CheckAllBox : public QCheckBox {
        Q_OBJECT
    public:
        /// Follows the items in \a column of \a view, with the text "All".
        explicit CheckAllBox(QAbstractItemView *view, int column = 0, QWidget *parent = nullptr);
        ~CheckAllBox();

    protected:
        void nextCheckState() override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        QPointer<QAbstractItemView> m_view;
        int m_column;
        // Set while a click changes the items, so that each change does not count the items
        bool m_applying = false;

        void refresh();
    };

}

#endif // HELLOUTAU_WIDGETS_CHECKALLBOX_H
