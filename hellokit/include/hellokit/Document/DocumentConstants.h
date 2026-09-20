#ifndef HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H
#define HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H

#include <stdutau/utaconst.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

/// \file
/// What a note means when nobody said otherwise, in one place.
///
/// Anything here is a contract rather than a local choice, which is why none of it belongs in
/// whichever file happened to need it first. The lyric a note starts with has to be the same
/// whether the note arrived from a MIDI file or was drawn in the piano roll, and writing the
/// same value twice is how the two quietly stop agreeing.
///
/// **What UTAU decides is not repeated here.** stdutau already states the defaults UST itself
/// carries, and those are used from there rather than copied. Only what HelloUTAU decides for
/// itself is written below.

namespace hello::kit {

    /// The lyric a note is given where nothing said what to sing.
    ///
    /// A UST note with an empty lyric is a rest, so a note that brought no lyric of its own
    /// cannot simply be left blank. This is the value behind both the note the editor creates
    /// and the note an importer produces.
    ///
    /// \note stdutau has a \c DEFAULT_LYRIC of its own, and it is a different thing. That one
    ///       answers what UST does. This one is HelloUTAU's choice, and the two are free to
    ///       disagree.
    inline constexpr char DefaultLyric[] = "la";

    /// What a rest is written as. UTAU also reads \c r and an empty lyric as rests, which is
    /// what \c Note::isRest() covers, but this is the one to write.
    inline constexpr char RestLyric[] = "R";

    /// Ticks to the quarter note.
    inline constexpr int TicksPerQuarter = utau::TIME_BASE;

    /// \name The keyboard
    ///
    /// C1 to B7, which is as far as UTAU's piano roll goes. A note outside it has nowhere to be
    /// put. Derived from stdutau rather than written out, so that the two cannot drift.
    /// @{
    inline constexpr int LowestNoteNum = utau::TONE_NUMBER_BASE;
    inline constexpr int HighestNoteNum =
        utau::TONE_NUMBER_BASE +
        (utau::TONE_OCTAVE_MAX - utau::TONE_OCTAVE_MIN + 1) * utau::TONE_OCTAVE_STEPS - 1;
    /// @}

}

#endif // HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H
