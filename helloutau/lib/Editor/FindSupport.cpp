#include "FindSupport_p.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/FindBar.h>

namespace hello::daw {

    kit::TextSearch FindSupport::searchOf(const FindBar *bar) {
        kit::TextSearch::Options options;
        if (bar->isCaseSensitive()) {
            options |= kit::TextSearch::CaseSensitive;
        }
        if (bar->isWholeWord()) {
            options |= kit::TextSearch::WholeWord;
        }
        if (bar->isRegularExpression()) {
            options |= kit::TextSearch::RegularExpression;
        }
        return kit::TextSearch(bar->text(), options);
    }

    std::optional<qsizetype> FindSupport::adjacentMatch(const QList<int> &matches, int from,
                                                        bool forward, bool inclusive) {
        if (matches.isEmpty()) {
            return std::nullopt;
        }
        if (forward) {
            const auto found = inclusive ? std::lower_bound(matches.begin(), matches.end(), from)
                                         : std::upper_bound(matches.begin(), matches.end(), from);
            return found == matches.end() ? 0 : found - matches.begin();
        }
        if (from < 0) {
            return matches.size() - 1;
        }
        const auto found = inclusive ? std::upper_bound(matches.begin(), matches.end(), from)
                                     : std::lower_bound(matches.begin(), matches.end(), from);
        return found == matches.begin() ? matches.size() - 1 : found - matches.begin() - 1;
    }

    void FindSupport::showResult(FindBar *bar, const kit::TextSearch &search,
                                 const QList<int> &matches, int current) {
        if (!search.errorString().isEmpty()) {
            bar->setError(search.errorString());
            return;
        }
        const auto at = std::lower_bound(matches.begin(), matches.end(), current);
        const bool isMatch = current >= 0 && at != matches.end() && *at == current;
        bar->setResult(isMatch ? int(at - matches.begin()) + 1 : 0, int(matches.size()));
    }

    QColor FindSupport::matchColor() {
        // The color is editor.findMatchHighlightBackground of the default themes of VS Code.
        return QColor(0xea, 0x5c, 0x00, 0x55);
    }

    QList<QRectF> FindSupport::matchRects(const QFontMetricsF &metrics, const QRectF &rect,
                                          Qt::Alignment alignment, const QString &text,
                                          const QList<kit::TextSearch::Match> &matches) {
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
        for (const auto &match : matches) {
            if (match.length == 0) {
                continue;
            }
            const double start = metrics.horizontalAdvance(text.left(match.start));
            const double end = metrics.horizontalAdvance(text.left(match.start + match.length));
            rects.push_back(QRectF(left + start, top, end - start, metrics.height()));
        }
        return rects;
    }

    void FindSupport::drawItemMatches(QPainter *painter, const QStyleOptionViewItem &option,
                                      const QList<kit::TextSearch::Match> &matches,
                                      const QColor &color) {
        const auto widget = option.widget;
        const auto style = widget ? widget->style() : QApplication::style();
        // The margin that QCommonStylePrivate::viewItemDrawText() removes from the text
        // rectangle on both sides
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        const QRectF rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, widget)
                                .adjusted(margin, 0, -margin, 0);
        const QFontMetricsF metrics(option.font);
        auto text = option.text;
        auto shown = matches;
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
            for (auto match : matches) {
                if (match.start < kept) {
                    match.length = std::min(match.length, kept - match.start);
                    shown.push_back(match);
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
