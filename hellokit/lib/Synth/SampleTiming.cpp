#include "SampleTiming.h"

#include <cmath>

#include <stdutau/utaconst.h>
#include <stdutau/utautils.h>

namespace hello::kit {

    namespace {

        bool isRest(const QString &lyric) {
            const auto bytes = lyric.toUtf8();
            return utau::isRestLyric(std::string(bytes.constData(), size_t(bytes.size())));
        }

        double durationOf(int length, double tempo) {
            return length * 125.0 / tempo;
        }

    }

    QList<SampleTiming> SampleTiming::of(const QList<Note> &notes, const TempoMap &tempos,
                                         const VoiceBank *bank) {
        QList<SampleTiming> result;
        result.reserve(notes.size());
        double previousDuration = 0;
        bool previousIsRest = false;
        for (qsizetype i = 0; i < notes.size(); ++i) {
            const auto &note = notes[i];
            const auto sample = bank ? bank->find(note.noteNum, note.lyric) : nullptr;
            double preUtterance = note.preUtterance.value_or(sample ? sample->preUtterance : 0);
            double overlap = note.voiceOverlap.value_or(sample ? sample->voiceOverlap : 0);
            const double startPoint = note.startPoint.value_or(utau::DEFAULT_VALUE_START_POINT);
            const double velocity = note.velocity.value_or(utau::DEFAULT_VALUE_VELOCITY);
            const double duration = durationOf(note.length, tempos.tempo(int(i)));

            const double velocityRate = std::pow(2, 1 - velocity / 100);
            preUtterance *= velocityRate;
            overlap *= velocityRate;

            SampleTiming timing{preUtterance, overlap, startPoint};
            if (previousDuration != 0) {
                // A note after a rest may take all of it, after a sung note half of it.
                const double maximum = previousIsRest ? previousDuration : previousDuration / 2;
                double rate = 1;
                if (preUtterance - overlap > maximum) {
                    rate = maximum / (preUtterance - overlap);
                }
                timing.preUtterance = rate * preUtterance;
                timing.voiceOverlap = rate * overlap;
                timing.startPoint = preUtterance - timing.preUtterance + startPoint;
                // The overlap does not reach past the end of the note.
                if (timing.voiceOverlap - timing.preUtterance > duration) {
                    timing.voiceOverlap = timing.preUtterance + duration;
                }
            }
            result.push_back(timing);
            previousDuration = duration;
            previousIsRest = isRest(note.lyric);
        }
        return result;
    }

}
