#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Widgets/FindMatches.h>

using namespace hello::daw;

namespace {

    using Range = FindMatches::Range;

    // Returns the number of pixels that drawItemMatches() fills in red for an item of \a text,
    // 60 pixels wide, with \a ranges, \a elide and \a features.
    int filledPixels(const QString &text, const QList<Range> &ranges,
                     Qt::TextElideMode elide = Qt::ElideRight,
                     QStyleOptionViewItem::ViewItemFeatures features = {}) {
        QImage image(200, 20, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, 60, 20);
        option.text = text;
        option.font = QApplication::font();
        option.textElideMode = elide;
        option.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
        option.features = features | QStyleOptionViewItem::HasDisplay;
        option.direction = Qt::LeftToRight;
        {
            QPainter painter(&image);
            FindMatches::drawItemMatches(&painter, option, ranges, Qt::red);
        }
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                count += image.pixelColor(x, y) == QColor(Qt::red) ? 1 : 0;
            }
        }
        return count;
    }

}

class test_FindMatches : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The search continues from the other end past the last or the first match.
    void the_adjacent_match_wraps_around() {
        const QList<int> positions{2, 5, 9};
        QCOMPARE(FindMatches::adjacentMatch({}, 0, true, false), std::nullopt);

        QCOMPARE(FindMatches::adjacentMatch(positions, 5, true, false), qsizetype(2));
        QCOMPARE(FindMatches::adjacentMatch(positions, 5, true, true), qsizetype(1));
        QCOMPARE(FindMatches::adjacentMatch(positions, 6, true, false), qsizetype(2));
        QCOMPARE(FindMatches::adjacentMatch(positions, 9, true, false), qsizetype(0));

        QCOMPARE(FindMatches::adjacentMatch(positions, 5, false, false), qsizetype(0));
        QCOMPARE(FindMatches::adjacentMatch(positions, 5, false, true), qsizetype(1));
        QCOMPARE(FindMatches::adjacentMatch(positions, 6, false, false), qsizetype(1));
        QCOMPARE(FindMatches::adjacentMatch(positions, 2, false, false), qsizetype(2));

        // A negative start precedes every position.
        QCOMPARE(FindMatches::adjacentMatch(positions, -1, true, false), qsizetype(0));
        QCOMPARE(FindMatches::adjacentMatch(positions, -1, false, false), qsizetype(2));
    }

    void the_rectangles_follow_the_alignment() {
        const QFontMetricsF metrics(QApplication::font());
        const QRectF rect(10, 20, 200, 30);
        const auto text = QStringLiteral("abcdef");
        const QList<Range> ranges{
            {1, 2},
            {3, 0}
        };
        const double width = metrics.horizontalAdvance(text);
        const double start = metrics.horizontalAdvance(QStringLiteral("a"));
        const double end = metrics.horizontalAdvance(QStringLiteral("abc"));
        const double top = 20 + (30 - metrics.height()) / 2;

        const auto left =
            FindMatches::matchRects(metrics, rect, Qt::AlignLeft | Qt::AlignVCenter, text, ranges);
        // The empty range is skipped.
        QCOMPARE(left.size(), 1);
        QCOMPARE(left[0], QRectF(10 + start, top, end - start, metrics.height()));

        const auto right =
            FindMatches::matchRects(metrics, rect, Qt::AlignRight | Qt::AlignTop, text, ranges);
        QCOMPARE(right[0].left(), 210 - width + start);
        QCOMPARE(right[0].top(), 20.0);

        const auto center = FindMatches::matchRects(metrics, rect, Qt::AlignHCenter, text, ranges);
        QCOMPARE(center[0].left(), 10 + (200 - width) / 2 + start);
    }

    // An elided text draws only the matches before the ellipsis, and nothing is drawn for
    // another elision or for wrapped text that does not fit.
    void only_the_shown_part_of_an_elided_text_is_drawn() {
        const auto text = QStringLiteral("aaaa bbbb cccc dddd eeee ffff");
        QVERIFY(filledPixels(text, {
                                       {0, 4}
        }) > 0);
        QCOMPARE(filledPixels(text,
                              {
                                  {25, 4}
        }),
                 0);
        QCOMPARE(filledPixels(text,
                              {
                                  {0, 4}
        },
                              Qt::ElideLeft),
                 0);
        QCOMPARE(filledPixels(text,
                              {
                                  {0, 4}
        },
                              Qt::ElideRight, QStyleOptionViewItem::WrapText),
                 0);
        QVERIFY(filledPixels(QStringLiteral("ab"), {
                                                       {0, 2}
        }) > 0);
    }
};

QTEST_MAIN(test_FindMatches)

#include "test_FindMatches.moc"
