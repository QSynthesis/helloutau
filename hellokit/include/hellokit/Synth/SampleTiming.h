#ifndef HELLOKIT_SYNTH_SAMPLETIMING_H
#define HELLOKIT_SYNTH_SAMPLETIMING_H

#include <QtCore/QList>

#include <hellokit/Document/Note.h>
#include <hellokit/Document/TempoMap.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// The timing of the sample of a note as the synthesis uses it, in milliseconds.
    ///
    /// The pre-utterance and the overlap are those of the oto entry, or of the note where it
    /// gives them, scaled by the velocity; where the previous note is too short for them, both
    /// shrink in proportion, and the start point grows by what the pre-utterance lost. The rules
    /// are those of stdutau (\c correctedTiming() in \c synth.cpp), and a test compares the
    /// values with the timing in SynthStep.
    ///
    /// The fragment of a note therefore starts its pre-utterance before the note, and the
    /// wavtool appends its duration plus its pre-utterance less the pre-utterance of the next
    /// note plus the overlap of the next note (see SynthPlan and WavtoolMixer).
    struct HELLOKIT_SYNTH_EXPORT SampleTiming {
        double preUtterance = 0;
        double voiceOverlap = 0;
        double startPoint = 0;

        /// The timings of \a notes, whose tempos \a tempos gives, with the oto entries of
        /// \a bank; without a bank, or for a lyric it has no sample for, the entry is empty.
        static QList<SampleTiming> of(const QList<Note> &notes, const TempoMap &tempos,
                                      const VoiceBank *bank);
    };

}

#endif // HELLOKIT_SYNTH_SAMPLETIMING_H
