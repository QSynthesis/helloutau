#include "FindMatches.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

namespace hello::daw {

    std::optional<qsizetype> FindMatches::adjacentMatch(const QList<int> &positions, int from,
                                                        bool forward, bool inclusive) {
        if (positions.isEmpty()) {
            return std::nullopt;
        }
        if (forward) {
            const auto found = inclusive
                                   ? std::lower_bound(positions.begin(), positions.end(), from)
                                   : std::upper_bound(positions.begin(), positions.end(), from);
            return found == positions.end() ? 0 : found - positions.begin();
        }
        if (from < 0) {
            return positions.size() - 1;
        }
        const auto found = inclusive ? std::upper_bound(positions.begin(), positions.end(), from)
                                     : std::lower_bound(positions.begin(), positions.end(), from);
        return found == positions.begin() ? positions.size() - 1 : found - positions.begin() - 1;
    }

    QColor FindMatches::matchColor() {
        // The color is editor.findMatchHighlightBackground of the default themes of VS Code.
        return QColor(0xea, 0x5c, 0x00, 0x55);
    }

    QList<QRectF> FindMatches::matchRects(const QFontMetricsF &metrics, const QRectF &rect,
                                          Qt::Alignment alignment, const QString &text,
                                          const QList<Range> &ranges) {
        const double width = metrics.horizontalAdvance(text);
        double left = rect.left();
        if (alignment & Qt::AlignRight) {
            left = rect.right() - width;
        } else if (alignment & Qt::AlignHCenter) {
            left = rect.left() + (rect.width() - width) / 2;
        }
        double top = rect.top();
        if (alignment & Qt::AlignBottom) {
            top = rect.bottom() - metrics.height();
        } else if (alignment & Qt::AlignVCenter) {
            top = rect.top() + (rect.height() - metrics.height()) / 2;
        }
        QList<QRectF> rects;
        for (const auto &range : ranges) {
            if (range.length == 0) {
                continue;
            }
            const double start = metrics.horizontalAdvance(text.left(range.start));
            const double end = metrics.horizontalAdvance(text.left(range.start + range.length));
            rects.push_back(QRectF(left + start, top, end - start, metrics.height()));
        }
        return rects;
    }

    void FindMatches::drawItemMatches(QPainter *painter, const QStyleOptionViewItem &option,
                                      const QList<Range> &ranges, const QColor &color) {
        const auto widget = option.widget;
        const auto style = widget ? widget->style() : QApplication::style();
        // The margin that QCommonStylePrivate::viewItemDrawText() removes from the text
        // rectangle on both sides
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        const QRectF rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, widget)
                                .adjusted(margin, 0, -margin, 0);
        const QFontMetricsF metrics(option.font);
        auto text = option.text;
        auto shown = ranges;
        if (metrics.horizontalAdvance(text) > rect.width()) {
            if ((option.features & QStyleOptionViewItem::WrapText) ||
                option.textElideMode != Qt::ElideRight) {
                return;
            }
            // The elided text is the kept prefix of the text followed by the ellipsis.
            const auto elided = metrics.elidedText(text, Qt::ElideRight, rect.width());
            qsizetype kept = 0;
            while (kept < elided.size() && kept < text.size() && elided[kept] == text[kept]) {
                ++kept;
            }
            shown.clear();
            for (auto range : ranges) {
                if (range.start < kept) {
                    range.length = std::min(range.length, kept - range.start);
                    shown.push_back(range);
                }
            }
            text = elided;
        }
        const auto alignment = QStyle::visualAlignment(option.direction, option.displayAlignment);
        painter->save();
        painter->setClipRect(rect, Qt::IntersectClip);
        for (const auto &match : matchRects(metrics, rect, alignment, text, shown)) {
            painter->fillRect(match, color);
        }
        painter->restore();
    }

}
