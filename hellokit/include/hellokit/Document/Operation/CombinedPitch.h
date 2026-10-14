#ifndef HELLOKIT_DOCUMENT_OPERATION_COMBINEDPITCH_H
#define HELLOKIT_DOCUMENT_OPERATION_COMBINEDPITCH_H

#include <optional>

#include <QtCore/QList>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/PitchBend.h>
#include <hellokit/Document/PortamentoPoint.h>

namespace hello::kit {

    struct Note;

    /// The pitch curves of notes combined into the first of them, which keep their shapes and
    /// heights on the combined note, the pitch of each later note becoming part of the curves.
    /// See the combination of notes in docs/Widgets.md.
    ///
    /// The Mode2 points of each later note follow those before it, moved by its start and by its
    /// pitch relative to the first note. Its first point starts at the pitch of the note before
    /// it, as in the render after a note that is not a rest. A curve that reaches past the first
    /// point of the next note ends there, where the render switches to the next note. No
    /// interval between consecutive points is negative.
    ///
    /// The Mode1 values are those of the curves of the notes, each with its pitch relative to the
    /// first note, every five ticks from the start of the values of the first note.
    struct HELLOKIT_DOCUMENT_EXPORT CombinedPitch {
        /// Mode2
        QList<PortamentoPoint> portamento;

        /// Mode1, or \c std::nullopt if no note has values and all have one pitch
        std::optional<PitchBend> pitchBend;

        /// Returns the curves of \a notes, at least one, combined at \a tempo, the tempo of every
        /// one of them.
        static CombinedPitch of(const QList<Note> &notes, double tempo);
    };

}

#endif // HELLOKIT_DOCUMENT_OPERATION_COMBINEDPITCH_H
