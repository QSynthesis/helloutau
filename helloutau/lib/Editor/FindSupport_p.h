#ifndef HELLOUTAU_EDITOR_FINDSUPPORT_P_H
#define HELLOUTAU_EDITOR_FINDSUPPORT_P_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QRectF>
#include <QtGui/QColor>
#include <QtGui/QFontMetricsF>
#include <QtWidgets/QStyleOptionViewItem>

#include <hellokit/Support/TextSearch.h>

class QPainter;

namespace hello::daw {

    class FindBar;

    /// The parts of find and replace that the project window and the voice bank window share.
    /// A position is an index in the order of the searched items, a note of the track or a row
    /// of the entry table.
    class FindSupport {
    public:
        /// Returns the search that the query and the options of \a bar describe.
        static kit::TextSearch searchOf(const FindBar *bar);

        /// Returns the index in \a matches, positions in ascending order, of the match after
        /// \a from if \a forward is true and before it otherwise. The search continues from the
        /// other end past the last or the first match. With \a inclusive, a match at \a from
        /// itself is returned. A negative \a from precedes every position. Returns
        /// \c std::nullopt if \a matches is empty.
        static std::optional<qsizetype> adjacentMatch(const QList<int> &matches, int from,
                                                      bool forward, bool inclusive);

        /// Shows in \a bar the number of \a matches with the match at \a current as the current
        /// match, or the error of \a search if it is invalid.
        static void showResult(FindBar *bar, const kit::TextSearch &search,
                               const QList<int> &matches, int current);

        /// Returns the translucent orange in which VS Code highlights the matches in the editor
        /// in both its light and dark themes.
        static QColor matchColor();

        /// Returns the rectangles of the nonempty \a matches in \a text, drawn on one line in
        /// \a rect with \a alignment and the font of \a metrics.
        static QList<QRectF> matchRects(const QFontMetricsF &metrics, const QRectF &rect,
                                        Qt::Alignment alignment, const QString &text,
                                        const QList<kit::TextSearch::Match> &matches);

        /// Fills the rectangles of \a matches in the text of the item of \a option with
        /// \a color, after the style has drawn the item. The rectangles follow the text layout
        /// of QCommonStyle, which the Windows 11 style and the Fusion style also use. If the
        /// text is elided at the right, the matches after the ellipsis are not drawn. Nothing
        /// is drawn for other elisions or for wrapped text that does not fit on one line.
        static void drawItemMatches(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QList<kit::TextSearch::Match> &matches,
                                    const QColor &color);
    };

}

#endif // HELLOUTAU_EDITOR_FINDSUPPORT_P_H
