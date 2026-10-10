#ifndef HELLOUTAU_WIDGETS_FINDMATCHES_H
#define HELLOUTAU_WIDGETS_FINDMATCHES_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QRectF>
#include <QtGui/QColor>
#include <QtGui/QFontMetricsF>
#include <QtWidgets/QStyleOptionViewItem>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QPainter;

namespace hello::daw {

    /// The parts of find and replace that do not depend on how text is searched: moving among
    /// the matches of a list of items, and highlighting the matches in the text of an item. A
    /// position is an index in the order of the searched items. See FindBar.
    class HELLOUTAU_WIDGETS_EXPORT FindMatches {
    public:
        /// A match in a text, from \c start, \c length characters long
        struct Range {
            qsizetype start = 0;
            qsizetype length = 0;
        };

        /// Returns the index in \a positions, in ascending order, of the match after \a from if
        /// \a forward is true and before it otherwise. The search continues from the other end
        /// past the last or the first match. With \a inclusive, a match at \a from itself is
        /// returned. A negative \a from precedes every position. Returns \c std::nullopt if
        /// \a positions is empty.
        static std::optional<qsizetype> adjacentMatch(const QList<int> &positions, int from,
                                                      bool forward, bool inclusive);

        /// Returns the translucent orange in which VS Code highlights the matches in the editor
        /// in both its light and dark themes.
        static QColor matchColor();

        /// Returns the rectangles of the nonempty \a ranges in \a text, drawn on one line in
        /// \a rect with \a alignment and the font of \a metrics.
        static QList<QRectF> matchRects(const QFontMetricsF &metrics, const QRectF &rect,
                                        Qt::Alignment alignment, const QString &text,
                                        const QList<Range> &ranges);

        /// Fills the rectangles of \a ranges in the text of the item of \a option with \a color,
        /// after the style has drawn the item. The rectangles follow the text layout of
        /// QCommonStyle, which the Windows 11 style and the Fusion style also use. If the text is
        /// elided at the right, the ranges after the ellipsis are not drawn. Nothing is drawn for
        /// other elisions or for wrapped text that does not fit on one line.
        static void drawItemMatches(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QList<Range> &ranges, const QColor &color);
    };

}

#endif // HELLOUTAU_WIDGETS_FINDMATCHES_H
