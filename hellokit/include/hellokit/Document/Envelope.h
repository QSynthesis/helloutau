#ifndef HELLOKIT_DOCUMENT_ENVELOPE_H
#define HELLOKIT_DOCUMENT_ENVELOPE_H

#include <array>
#include <optional>

#include <QtCore/QDataStream>
#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// One envelope point: \c x in milliseconds from the previous point, \c y in percent.
    struct EnvelopeAnchor {
        double x = 0;
        double y = 0;

        inline bool operator==(const EnvelopeAnchor &RHS) const {
            return x == RHS.x && y == RHS.y;
        }

        inline bool operator!=(const EnvelopeAnchor &RHS) const {
            return !(*this == RHS);
        }
    };

    /// The volume envelope, with four anchors or with five including the middle anchor.
    ///
    /// Each index has a fixed role: 0 is the start, 1 the end of the attack, 2 the middle anchor,
    /// 3 the start of the release, and 4 the end. The index of an anchor therefore does not
    /// depend on whether the middle anchor exists. UST and \c .usth list the anchors in time
    /// order instead.
    ///
    /// \sa anchorsInTimeOrder()
    struct HELLOKIT_DOCUMENT_EXPORT Envelope {
        /// The anchors by role. Index 2 is part of the envelope only if \c hasMiddle is true.
        std::array<EnvelopeAnchor, 5> anchors;

        bool hasMiddle = false;

        /// Returns the four or five anchors of the envelope in time order.
        inline QList<EnvelopeAnchor> anchorsInTimeOrder() const {
            QList<EnvelopeAnchor> result{anchors[0], anchors[1]};
            if (hasMiddle) {
                result.append(anchors[2]);
            }
            result.append(anchors[3]);
            result.append(anchors[4]);
            return result;
        }

        /// Returns the envelope with \a anchors in time order, or \c std::nullopt unless it has
        /// four or five of them.
        static inline std::optional<Envelope> fromTimeOrder(const QList<EnvelopeAnchor> &anchors) {
            if (anchors.size() != 4 && anchors.size() != 5) {
                return std::nullopt;
            }
            Envelope envelope;
            envelope.hasMiddle = anchors.size() == 5;
            const int shift = envelope.hasMiddle ? 1 : 0;
            envelope.anchors[0] = anchors[0];
            envelope.anchors[1] = anchors[1];
            if (envelope.hasMiddle) {
                envelope.anchors[2] = anchors[2];
            }
            envelope.anchors[3] = anchors[2 + shift];
            envelope.anchors[4] = anchors[3 + shift];
            return envelope;
        }

        /// Returns whether both envelopes have the same anchors. An unused middle anchor is not
        /// compared.
        inline bool operator==(const Envelope &RHS) const {
            return hasMiddle == RHS.hasMiddle && anchors[0] == RHS.anchors[0] &&
                   anchors[1] == RHS.anchors[1] && (!hasMiddle || anchors[2] == RHS.anchors[2]) &&
                   anchors[3] == RHS.anchors[3] && anchors[4] == RHS.anchors[4];
        }

        inline bool operator!=(const Envelope &RHS) const {
            return !(*this == RHS);
        }

        /// Returns the envelope without the anchor at \a index in time order, over a fragment of
        /// \a length milliseconds, with the other anchors at the same times and volumes. Of five
        /// anchors the other four remain. Of four, the anchor is replaced by one halfway between
        /// its neighbours in time and volume, the start and the end of the fragment at a volume
        /// of 0 counting as neighbours, so that the outline stays the line through the others.
        /// The distances between the anchors are rounded to a thousandth of a millisecond, the
        /// new volume to a percent.
        ///
        /// \return \c *this if \a index is not that of an anchor.
        Envelope withoutAnchor(int index, double length) const;

        /// Returns the envelope as written in \c .usth, with the anchors in time order.
        QJsonObject toJson() const;

        /// Returns the envelope written as in \c .usth, or \c std::nullopt unless \a object lists
        /// four or five anchors.
        static std::optional<Envelope> fromJson(const QJsonObject &object);
    };

    // The stream operators of a value stored as a whole in the edit history. They are declared
    // with the type, because Qt records the stream operators of a type where its meta-type is
    // first instantiated. The format is part of the history format and must not change.

    inline QDataStream &operator<<(QDataStream &out, const Envelope &envelope) {
        for (const auto &anchor : envelope.anchors) {
            out << anchor.x << anchor.y;
        }
        return out << envelope.hasMiddle;
    }

    inline QDataStream &operator>>(QDataStream &in, Envelope &envelope) {
        for (auto &anchor : envelope.anchors) {
            in >> anchor.x >> anchor.y;
        }
        return in >> envelope.hasMiddle;
    }

}

#endif // HELLOKIT_DOCUMENT_ENVELOPE_H
