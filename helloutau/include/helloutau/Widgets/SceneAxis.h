#ifndef HELLOUTAU_WIDGETS_SCENEAXIS_H
#define HELLOUTAU_WIDGETS_SCENEAXIS_H

#include <cmath>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// The horizontal axis of a timeline: positions in ticks mapped to pixels of a view.
    ///
    /// Zooming changes only the scale, and scrolling only the position of the left edge, so that
    /// nothing drawn on the timeline depends on either.
    struct TimeAxis {
        /// The width of one tick. The default is 60 pixels for a quarter note of 480 ticks.
        double pixelsPerTick = 0.125;

        /// The tick at the left edge of the view.
        double left = 0;

        inline double toX(double tick) const {
            return (tick - left) * pixelsPerTick;
        }

        inline double toTick(double x) const {
            return left + x / pixelsPerTick;
        }

        inline bool operator==(const TimeAxis &RHS) const {
            return pixelsPerTick == RHS.pixelsPerTick && left == RHS.left;
        }

        inline bool operator!=(const TimeAxis &RHS) const {
            return !(*this == RHS);
        }
    };

    /// The vertical axis of a piano roll: keys mapped to pixels of a view, the higher key above.
    ///
    /// Key \c k occupies the row between the positions \c k and \c k + 1, so that toY(k + 1) is
    /// the top of its row and toY(k) the bottom.
    struct KeyAxis {
        /// The height of the row of one key.
        double pixelsPerKey = 24;

        /// The key position at the top edge of the view.
        double top = 84;

        inline double toY(double key) const {
            return (top - key) * pixelsPerKey;
        }

        inline double toKey(double y) const {
            return top - y / pixelsPerKey;
        }

        /// The key whose row contains \a y.
        inline int keyAt(double y) const {
            return int(std::floor(toKey(y)));
        }

        inline bool operator==(const KeyAxis &RHS) const {
            return pixelsPerKey == RHS.pixelsPerKey && top == RHS.top;
        }

        inline bool operator!=(const KeyAxis &RHS) const {
            return !(*this == RHS);
        }
    };

}

#endif // HELLOUTAU_WIDGETS_SCENEAXIS_H
