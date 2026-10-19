#ifndef HELLOUTAU_WIDGETS_HEIGHTFORWIDTHFILTER_H
#define HELLOUTAU_WIDGETS_HEIGHTFORWIDTHFILTER_H

#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// Keeps the minimum height of a window at the height that the layout of the window requires
    /// for the width of the window, so that a word-wrapped label is not clipped.
    ///
    /// Qt derives the minimum height of a window from the minimum sizes of the items, and the
    /// minimum height of a word-wrapped label is the height of one line. A window resized to its
    /// minimum height, as by <tt>resize(width, 0)</tt>, therefore clips a label that wraps. A
    /// label that is shown, or whose text becomes longer, after the window is shown is clipped
    /// in the same way. The filter updates the minimum height before the window is first shown,
    /// when the layout changes and when the width changes. The window grows to the minimum height
    /// and does not shrink.
    // TODO: Replace the filter with a solution within the layout system of Qt, if one exists,
    // because an event filter that sets the minimum height works around the layout.
    class HELLOUTAU_WIDGETS_EXPORT HeightForWidthFilter : public QObject {
    public:
        /// Installs the filter on \a window, which owns the filter.
        explicit HeightForWidthFilter(QWidget *window);
        ~HeightForWidthFilter();

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
    };

}

#endif // HELLOUTAU_WIDGETS_HEIGHTFORWIDTHFILTER_H
