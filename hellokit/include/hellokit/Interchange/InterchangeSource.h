#ifndef HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H
#define HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>

namespace hello::kit {

    /// One importable unit within a file, such as a MIDI track, a VSQ part or a ustx track.
    struct InterchangeEntry {
        int index = 0;
        int noteCount = 0;

        /// The pitch range, for display in the selector. Empty if the entry has no notes.
        std::optional<int> lowestNote;
        std::optional<int> highestNote;

        /// \name Undecoded text
        ///
        /// Deliberately bytes rather than strings, the only such case in this module. The
        /// encoding of this text is one of the settings the user is about to choose, and the
        /// selector displays the text decoded with the current selection so that the user can
        /// identify the correct one. Decoding in inspect() would presuppose the answer.
        ///
        /// \warning No component after the selector may retain these. The result of read() is
        ///          UTF-8 throughout. See docs/Interchange.md.
        /// @{
        QByteArray rawName;
        QList<QByteArray> rawLyrics;
        /// @}
    };

    /// The contents of a file, determined before any conversion.
    ///
    /// Public in its own right, because a file dialog may inspect a file without importing it.
    struct InterchangeSource {
        QString formatId;
        QList<InterchangeEntry> entries;

        /// Markers and other text belonging to the file rather than to one entry. Undecoded, for
        /// the reason stated above.
        QList<QByteArray> rawLabels;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H
