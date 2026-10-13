#include "CombinedPitch.h"

#include <algorithm>
#include <cmath>

#include "DocumentConstants.h"
#include "Note.h"
#include "TempoMap.h"

namespace hello::kit {

    namespace {

        // The interval of the values of Mode1 in ticks
        constexpr int bendInterval = 5;

        double ticksOf(double milliseconds, double tempo) {
            return milliseconds * tempo * ticksPerQuarter / 60000;
        }

    }

    CombinedPitch CombinedPitch::of(const QList<Note> &notes, double tempo) {
        CombinedPitch result;
        if (notes.isEmpty()) {
            return result;
        }
        const auto &first = notes.first();
        QList<double> starts;
        double end = 0;
        for (const auto &note : notes) {
            starts.push_back(end);
            end += note.length;
        }
        // The pitch of note k relative to the first note in cents, 0 for a note without pitch,
        // as for the first point in the render
        const auto relative = [&](qsizetype k) {
            const auto &note = notes.at(k);
            return note.noteNum <= 0 || first.noteNum <= 0
                       ? 0.0
                       : double(note.noteNum - first.noteNum) * 100;
        };

        // Mode2
        auto &points = result.portamento;
        for (qsizetype k = 0; k < notes.size(); ++k) {
            const auto &note = notes.at(k);
            auto own = note.portamento;
            // The pitch of a note without points bends from the note before at its start, which
            // two points at its start express in a combination of several notes.
            if (own.isEmpty() && notes.size() > 1) {
                own = {PortamentoPoint(), PortamentoPoint()};
            }
            if (own.isEmpty()) {
                continue;
            }
            if (k > 0) {
                const auto &previous = notes.at(k - 1);
                if (!previous.isRest()) {
                    own.first().y =
                        previous.noteNum <= 0 ? 0 : double(previous.noteNum - note.noteNum) * 100;
                }
            }
            const double shift = TempoMap::duration(starts.at(k), tempo);
            for (auto &point : own) {
                point.x += shift;
                point.y += relative(k);
            }
            if (k > 0 && !points.isEmpty()) {
                const double cut = own.first().x;
                if (points.last().x > cut) {
                    // The curve of the note before ends at the first point of this note, at its
                    // height there, in the shape of the segment that contains the point.
                    const double height = PortamentoPoint::heightAt(points, cut);
                    const auto after =
                        std::find_if(points.begin(), points.end(),
                                     [cut](const PortamentoPoint &point) { return point.x > cut; });
                    const auto type = after->type;
                    points.erase(after, points.end());
                    points.push_back({cut, height, type});
                } else if (points.last().y != relative(k - 1)) {
                    // After its last point the curve of the note before is at the pitch of that
                    // note.
                    points.push_back({points.last().x, relative(k - 1), PortamentoPoint::S});
                }
            }
            points += own;
        }
        // After the last point of the last note, the curve is at its pitch to the end of the
        // combined note.
        if (notes.size() > 1 && !points.isEmpty()) {
            const double last = relative(notes.size() - 1);
            const double endTime = TempoMap::duration(end, tempo);
            if (points.last().x < endTime) {
                if (points.last().y != last) {
                    points.push_back({points.last().x, last, PortamentoPoint::S});
                }
                if (last != 0) {
                    points.push_back({endTime, last, PortamentoPoint::S});
                }
            }
        }
        for (qsizetype i = 1; i < points.size(); ++i) {
            points[i].x = std::max(points[i].x, points[i - 1].x);
        }
        // A point between two points of the same height lies on a flat segment. A curve flat at
        // the pitch of the first note sounds as no points do, with a bend from the note before
        // at the start.
        for (qsizetype i = 1; i + 1 < points.size();) {
            if (points[i - 1].y == points[i].y && points[i].y == points[i + 1].y) {
                points.removeAt(i);
            } else {
                ++i;
            }
        }
        if (first.portamento.isEmpty() &&
            std::all_of(points.cbegin(), points.cend(),
                        [](const PortamentoPoint &point) { return point.y == 0; })) {
            points.clear();
        }

        // Mode1
        const bool hasValues = std::any_of(notes.cbegin(), notes.cend(), [](const Note &note) {
            return note.pitchBend && !note.pitchBend->values.isEmpty();
        });
        const bool onePitch = std::all_of(notes.cbegin(), notes.cend(), [&](const Note &note) {
            return note.noteNum <= 0 || first.noteNum <= 0 || note.noteNum == first.noteNum;
        });
        if (!hasValues && onePitch) {
            result.pitchBend = first.pitchBend;
            return result;
        }
        PitchBend bend;
        bend.start = first.pitchBend ? first.pitchBend->start.value_or(0) : 0;
        const double origin = ticksOf(*bend.start, tempo);
        // The values reach the end of the combined note, and the last values of the last note
        // beyond it, which apply before the start of the note after.
        double last = end;
        if (const auto &lastBend = notes.last().pitchBend;
            lastBend && !lastBend->values.isEmpty()) {
            last = std::max(last, starts.last() + ticksOf(lastBend->start.value_or(0), tempo) +
                                      double(lastBend->values.size() - 1) * bendInterval);
        }
        for (qsizetype i = 0; origin + double(i) * bendInterval <= last; ++i) {
            const double tick = origin + double(i) * bendInterval;
            qsizetype k = 0;
            while (k + 1 < notes.size() && tick >= starts.at(k + 1)) {
                ++k;
            }
            const double value = PitchBend::curveAt(notes.at(k).pitchBend, PreviousBend(),
                                                    tick - starts.at(k), tempo) +
                                 relative(k);
            bend.values.push_back(std::round(value));
        }
        result.pitchBend = bend;
        return result;
    }

}
