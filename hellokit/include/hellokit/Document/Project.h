#ifndef HELLOKIT_DOCUMENT_PROJECT_H
#define HELLOKIT_DOCUMENT_PROJECT_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
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
        double tempo = utau::DEFAULT_VALUE_TEMPO;
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

    /// What a \c .ust says about itself before any of it is decoded.
    ///
    /// UST is bytes in an encoding the file mostly does not name, so which encoding it is has to
    /// be settled before anything in it can be read. That is a question with a user at the end
    /// of it, and this library cannot ask one, so the answer is worked out as far as it goes
    /// here and the rest is left to the caller.
    struct HELLOKIT_DOCUMENT_EXPORT UstProbe {
        /// The encoding recorded in the control note, which is the one authority there is.
        std::optional<QString> recordedCharset;

        /// Whether \c [#VERSION] carries \c Charset=UTF-8 , which is all UST itself can say.
        bool declaresUtf8 = false;

        /// \name Text that has not been decoded
        ///
        /// For a chooser to show under each candidate encoding, so that the user can see which
        /// one is right. Bytes rather than strings for the same reason as in
        /// \c InterchangeEntry, and they go no further than the chooser.
        /// @{
        QByteArray rawProjectName;
        QByteArray rawVoiceDir;
        QList<QByteArray> rawLyrics;
        /// @}

        /// The encoding this file settles by itself, or nothing where the user has to say.
        std::optional<QString> settledCharset() const;
    };

    /// What to write into a \c .ust beyond the project itself.
    struct UstExportOptions {
        /// The encoding to write in. UTF-8 is also declared in \c [#VERSION] , and anything else
        /// cannot be, which is why the control note carries it either way.
        QString charset = QStringLiteral("UTF-8");

        /// Engines to name where the project names none.
        ///
        /// UTAU opening a UST with no engine in it has nothing to render with, so the ones from
        /// the local settings are written instead. The project's own are left exactly as they
        /// were found where it has them.
        QString wavtool;
        QString resampler;
    };

    /// A project in memory, which is what \c .usth, \c .ust and every imported format turn into.
    ///
    /// Reading and writing \c .usth belongs here rather than to a class of its own, because
    /// \c .usth is not one format among several: it is how a project is written down. Every
    /// other format goes through \c HelloKitInterchange and turns into one of these.
    ///
    /// The file is JSON, UTF-8, without a byte order mark, with newlines. docs/UsthFormat.md
    /// defines it, and this follows that rather than the other way round. Nothing here has to
    /// guess at an encoding, which is what separates it from reading a \c .ust.
    ///
    /// \note \c tracks holds exactly one track for now. The array is here from the start so that
    ///       several tracks become possible without a new format version, and a reader that sees
    ///       any other length has to say so rather than quietly take the first one.
    struct HELLOKIT_DOCUMENT_EXPORT Project {
        ProjectSettings settings;
        QList<Track> tracks;

        /// Top level fields of the \c .usth file that this version has no field for, kept so
        /// that they are written back.
        ///
        /// An older build opening a project a newer one saved would otherwise eat whatever it
        /// did not recognize, and the user would find it gone after saving. Empty for a project
        /// that came from anywhere else.
        QJsonObject unknownFields;

        /// \return the project, or nothing where the file could not be understood, with the
        ///         reason in \a diagnostics
        static std::optional<Project> read(const std::filesystem::path &path,
                                           DiagnosticList &diagnostics);

        /// \overload
        ///
        /// Separate from read() so that a caller that already holds the bytes, the tests above
        /// all, does not have to put them on disk first.
        static std::optional<Project> parse(QByteArrayView json, DiagnosticList &diagnostics);

        bool write(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// \overload
        QByteArray serialize() const;

        /// \name UTAU's own format
        ///
        /// A \c .ust is written by UTAU and read by everything in its world, so this is the way
        /// in and out of that world rather than one import format among several. What is lost
        /// each way and what is not is written down in docs/UsthFormat.md.
        /// @{

        /// Works out what \a path is, without decoding any of its text.
        static std::optional<UstProbe> probeUst(const std::filesystem::path &path,
                                                DiagnosticList &diagnostics);

        /// \param charset the encoding to read it in, which probeUst() settles or the user does
        static std::optional<Project> fromUst(const std::filesystem::path &path,
                                              const QString &charset, DiagnosticList &diagnostics);

        bool toUst(const std::filesystem::path &path, const UstExportOptions &options,
                   DiagnosticList &diagnostics) const;

        /// @}
    };

}

#endif // HELLOKIT_DOCUMENT_PROJECT_H
