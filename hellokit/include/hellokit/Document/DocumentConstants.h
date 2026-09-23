#ifndef HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H
#define HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H

#include <stdutau/utaconst.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

/// \file
/// Default values of note properties, defined in one place.
///
/// Each value here is a contract rather than a local choice, and therefore does not belong in
/// the file that first needed it. The initial lyric of a note must be identical whether the note
/// was imported from a MIDI file or drawn in the piano roll, and defining the same value twice
/// leads to silent divergence.
///
/// **UTAU defaults are not repeated here.** stdutau already defines the defaults of the UST
/// format, and they are used from there rather than copied. Only values specific to HelloUTAU
/// are defined below.

namespace hello::kit {

    /// The lyric assigned to a note when none is specified.
    ///
    /// A UST note with an empty lyric is a rest, so a note imported without a lyric cannot be
    /// left blank. This value is used both for notes created in the editor and for notes
    /// produced by importers.
    ///
    /// \note stdutau defines its own \c DEFAULT_LYRIC , which serves a different purpose. That
    ///       constant describes UST behavior. This one is a HelloUTAU decision, and the two may
    ///       differ.
    inline constexpr char defaultLyric[] = "la";

    /// The lyric written for a rest. UTAU also reads \c r and an empty lyric as rests, which
    /// \c Note::isRest() accounts for, but this is the form to write.
    inline constexpr char restLyric[] = "R";

    /// Ticks per quarter note.
    inline constexpr int ticksPerQuarter = utau::TIME_BASE;

    /// The \c .usth format version written by this build, and the highest version it reads.
    inline constexpr int usthFormatVersion = 1;

    /// \name Control note
    ///
    /// The first note of a \c .ust written by HelloUTAU, which stores data that UST cannot
    /// represent. No voice bank has a sample for this lyric, so UTAU finds none, produces no
    /// sound and spends no rendering time on it, while the note still occupies its length. The
    /// lyric is deliberately conspicuous, so that a user opening the file in UTAU can recognize
    /// immediately that the note was not written by hand.
    ///
    /// The entry name must begin with \c $ . UTAU preserves an unrecognized entry only in a note
    /// section and only with this prefix, as determined by measurement.
    ///
    /// \sa docs/UsthFormat.md
    /// @{
    inline constexpr char controlNoteLyric[] = "_USTH_";
    inline constexpr char controlNoteEntry[] = "$usth";
    inline constexpr int controlNoteLength = 480;
    inline constexpr int controlNoteNoteNum = 60;

    /// The version of the entry payload, which is versioned independently of
    /// \c usthFormatVersion.
    inline constexpr int controlNotePayloadVersion = 1;
    /// @}

    /// \name Keyboard range
    ///
    /// C1 to B7, the range of the UTAU piano roll. A note outside this range cannot be placed.
    /// Derived from stdutau rather than written out, so that the two cannot diverge.
    /// @{
    inline constexpr int lowestNoteNum = utau::TONE_NUMBER_BASE;
    inline constexpr int highestNoteNum =
        utau::TONE_NUMBER_BASE +
        (utau::TONE_OCTAVE_MAX - utau::TONE_OCTAVE_MIN + 1) * utau::TONE_OCTAVE_STEPS - 1;
    /// @}

}

#endif // HELLOKIT_DOCUMENT_DOCUMENTCONSTANTS_H
