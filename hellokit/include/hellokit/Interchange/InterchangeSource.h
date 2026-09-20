#ifndef HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H
#define HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>

namespace hello::kit {

    /// One importable thing inside a file: a MIDI track, a VSQ part, a ustx track.
    struct InterchangeEntry {
        int index = 0;
        int noteCount = 0;

        /// The range of pitches, for the chooser to show. Empty where the entry has no notes.
        std::optional<int> lowestNote;
        std::optional<int> highestNote;

        /// \name Text that has not been decoded
        ///
        /// Bytes rather than strings, on purpose, and the only place in this module where that
        /// is so. Which encoding these are in is one of the things the user is about to be
        /// asked, and the chooser shows them decoded with whatever is selected at the moment so
        /// that the user can see which selection is right. Decoding them in inspect() would
        /// answer the question before it was put.
        ///
        /// \warning Nothing downstream of the chooser may hold these. What read() returns is
        ///          UTF-8 throughout. See docs/Interchange.md.
        /// @{
        QByteArray rawName;
        QList<QByteArray> rawLyrics;
        /// @}
    };

    /// What a file turns out to hold, worked out before anything is converted.
    ///
    /// Public in its own right, since looking at a file without importing it is a thing a file
    /// dialog wants to do.
    struct InterchangeSource {
        QString formatId;
        QList<InterchangeEntry> entries;

        /// Markers and other text belonging to the file rather than to one entry. Undecoded, for
        /// the reason given above.
        QList<QByteArray> rawLabels;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGESOURCE_H
