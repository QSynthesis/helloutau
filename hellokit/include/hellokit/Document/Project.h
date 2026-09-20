#ifndef HELLOKIT_DOCUMENT_PROJECT_H
#define HELLOKIT_DOCUMENT_PROJECT_H

#include <QList>
#include <QString>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// One voice part. UST holds exactly one.
    struct Track {
        /// UST has no name for a track, so this is dropped on the way out to \c .ust.
        QString name;

        /// The voice bank directory. A \c %VOICE% prefix stands for the shared voice location.
        QString voiceDir;

        QList<Note> notes;
    };

    /// Everything the whole project shares.
    struct ProjectSettings {
        QString name;
        double tempo = 120.0;
        QString flags;
        QString outputFile;

        /// Where the rendered pieces are cached. UTAU rewrites this to follow the file name when
        /// it saves, and so do we.
        QString cacheDir;

        /// The engines named by the project file itself, \c Tool1 and \c Tool2 in UST.
        ///
        /// \warning Untrusted, in the same way as \c Note::patch. Kept as they were found,
        ///          because configuring an engine per project is ordinary UTAU practice and
        ///          dropping it would delete the user's settings. What is forbidden is running
        ///          them without asking, not holding them. See the security section of AGENTS.md.
        QString wavtool;
        QString resampler;

        /// Whether the pitch is the Mode2 curve rather than the Mode1 sample array. A project is
        /// in one mode or the other, so \c Note::portamento and \c Note::pitchBend never both
        /// carry anything.
        bool mode2 = true;
    };

    /// A project in memory, which is what \c .usth, \c .ust and every imported format turn into.
    ///
    /// \note \c tracks holds exactly one track for now. The array is here from the start so that
    ///       several tracks become possible without a new format version, and a reader that sees
    ///       any other length has to say so rather than quietly take the first one.
    struct Project {
        ProjectSettings settings;
        QList<Track> tracks;
    };

}

#endif // HELLOKIT_DOCUMENT_PROJECT_H
