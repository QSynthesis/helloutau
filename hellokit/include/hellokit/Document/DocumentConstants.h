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
    inline constexpr char defaultLyric[] = "la";

    /// What a rest is written as. UTAU also reads \c r and an empty lyric as rests, which is
    /// what \c Note::isRest() covers, but this is the one to write.
    inline constexpr char restLyric[] = "R";

    /// Ticks to the quarter note.
    inline constexpr int ticksPerQuarter = utau::TIME_BASE;

    /// The \c .usth format version this build writes, and the highest it can read.
    inline constexpr int usthFormatVersion = 1;

    /// \name The control note
    ///
    /// The first note of a \c .ust HelloUTAU wrote, which carries what UST has nowhere to put.
    /// No voice bank has a sample under this lyric, so UTAU finds nothing, makes no sound and
    /// spends no time rendering it, while the note still takes up its length. The lyric is
    /// deliberately conspicuous, so that a user opening the file in UTAU can see at a glance
    /// that they did not write it.
    ///
    /// The entry name has to begin with a \c $ . UTAU keeps an entry it does not recognize only
    /// on a note section and only with that prefix, which is measured rather than assumed.
    ///
    /// \sa docs/UsthFormat.md
    /// @{
    inline constexpr char controlNoteLyric[] = "_USTH_";
    inline constexpr char controlNoteEntry[] = "$usth";
    inline constexpr int controlNoteLength = 480;
    inline constexpr int controlNoteNoteNum = 60;

    /// The version of what the entry carries, which moves on its own rather than with
    /// \c usthFormatVersion.
    inline constexpr int controlNotePayloadVersion = 1;
    /// @}

    /// \name The keyboard
    ///
    /// C1 to B7, which is as far as UTAU's piano roll goes. A note outside it has nowhere to be
    /// put. Derived from stdutau rather than written out, so that the two cannot drift.
    /// @{
    inline constexpr int lowestNoteNum = utau::TONE_NUMBER_BASE;
    inline constexpr int highestNoteNum =
        utau::TONE_NUMBER_BASE +
        (utau::TONE_OCTAVE_MAX - utau::TONE_OCTAVE_MIN + 1) * utau::TONE_OCTAVE_STEPS - 1;
    /// @}

}

#endif // HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H
