#ifndef HELLOKIT_DOCUMENT_PORTAMENTOSETTINGS_H
#define HELLOKIT_DOCUMENT_PORTAMENTOSETTINGS_H

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// The settings of the portamento group of Pitch Control, after the tone control dialog of
    /// UTAU, and the Mode2 points that they give a note. The settings are written as a whole as
    /// the default of the dialog, see toJson().
    struct HELLOKIT_DOCUMENT_EXPORT PortamentoSettings {
        /// The way the points are given.
        enum Mode {
            Preset,    ///< Two points of a preset
            Custom,    ///< Two points from \c start, \c length long
            AddPoints, ///< The current points changed to \c count points
        };

        /// Where a preset lies around the start of the note.
        enum Position {
            Center, ///< From \c presetLength before the start to \c presetLength after it
            Left,   ///< From \c presetLength before the start to the start
            Right,  ///< From the start to \c presetLength after it
        };

        /// The lengths of the presets in milliseconds.
        static constexpr int presetLengths[] = {50, 100, 200};

        /// The interval in milliseconds of the points that AddPoints appends, as in UTAU.
        static constexpr double appendedInterval = 25;

        Mode mode = Preset;
        Position position = Center;
        int presetLength = 50;

        /// Custom: the first point and the distance to the second point, in milliseconds
        int start = -30;
        int length = 59;

        /// AddPoints: the number of points, at least 2, and whether they are spread evenly over
        /// the note
        int count = 2;
        bool evenlyDistributed = true;

        inline bool operator==(const PortamentoSettings &RHS) const {
            return mode == RHS.mode && position == RHS.position &&
                   presetLength == RHS.presetLength && start == RHS.start && length == RHS.length &&
                   count == RHS.count && evenlyDistributed == RHS.evenlyDistributed;
        }

        inline bool operator!=(const PortamentoSettings &RHS) const {
            return !(*this == RHS);
        }

        /// Returns the points of a note whose points are \a current and whose duration is
        /// \a duration milliseconds:
        ///
        /// - Preset: the two points of the preset at the height of the note, with \c S curves.
        /// - Custom: two points from \c start, \c length apart, which keep the heights and types
        ///   of \a current if it has two points.
        /// - AddPoints: \c count points. If \c evenlyDistributed, the points are spread evenly
        ///   from the first current point to the end of the note, as in UTAU. Otherwise more
        ///   points than \a current are appended after the last current point at
        ///   appendedInterval, closer if the end of the note is nearer, and in the middle of the
        ///   longest segments if the last point is at or after the end of the note. Fewer points
        ///   are spread evenly between the first and the last current point. A new point lies on
        ///   the current curve and takes the type of the segment in which it lies, which keeps
        ///   the shape of an \c S curve split in its middle exactly and of other curves
        ///   approximately. Without current points, the points are spread evenly over the
        ///   preset.
        QList<PortamentoPoint> pointsFor(const QList<PortamentoPoint> &current,
                                         double duration) const;

        /// Returns the settings as written in the application settings, with the mode and the
        /// position by name.
        QJsonObject toJson() const;

        /// Returns the settings written as by toJson(). A field that is absent or not valid
        /// keeps its default.
        static PortamentoSettings fromJson(const QJsonObject &object);
    };

}

#endif // HELLOKIT_DOCUMENT_PORTAMENTOSETTINGS_H
