#ifndef HELLOKIT_EDIT_PROJECTDOCUMENT_H
#define HELLOKIT_EDIT_PROJECTDOCUMENT_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/UstDocument.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    class VoiceBank;
    class VoiceBankCharsetSelector;

    /// Chooses the encoding of a UST that does not state it, by asking the user. Implemented by
    /// the side that links QtWidgets, as \c VoiceBankCharsetSelector is.
    class HELLOKIT_EDIT_EXPORT UstCharsetSelector {
    public:
        virtual ~UstCharsetSelector();

        /// Returns the encoding in which to read \a ust, the file at \a path, or
        /// \c std::nullopt if the user declined to open it.
        virtual std::optional<QString> selectCharset(const UstDocument &ust,
                                                     const std::filesystem::path &path) = 0;
    };

    /// One open project: its edit session, the file it belongs to, and whether it has changed
    /// since that file was written.
    ///
    /// The native format is \c .usth. A \c .ust is imported: the document has no file of its
    /// own until it is saved as \c .usth, and a UST is written only by exportUst(), which leaves
    /// the file of the document and its modified state unchanged. See docs/Widgets.md.
    ///
    /// A document is modified when the step of its session differs from the step at which it was
    /// last saved or opened, see the section on change notification in docs/Editing.md.
    class HELLOKIT_EDIT_EXPORT ProjectDocument : public QObject {
        Q_OBJECT
    public:
        /// A new project with one empty track and no file.
        explicit ProjectDocument(QObject *parent = nullptr);
        ~ProjectDocument();

        /// Opens the \c .usth or \c .ust at \a path. A UST whose encoding neither its control
        /// note nor its declaration states is read in the encoding that \a selector returns.
        ///
        /// \return \c nullptr if the file could not be read, with the reason in \a diagnostics,
        ///         or if the user declined to choose an encoding, without an error
        static std::unique_ptr<ProjectDocument> open(const std::filesystem::path &path,
                                                     UstCharsetSelector *selector,
                                                     DiagnosticList &diagnostics,
                                                     QObject *parent = nullptr);

        ProjectSession *session();
        const ProjectSession *session() const;

        /// The \c .usth the document is saved to, or an empty path if it has none: a new
        /// document, or one imported from a UST.
        std::filesystem::path filePath() const;

        /// The file the document was opened from, a \c .usth or a \c .ust, or else filePath().
        /// Suggests the name under which to save a document that has no file.
        std::filesystem::path sourcePath() const;

        /// The name to show for the document: the name of its source file, or an empty string
        /// for a new document.
        QString displayName() const;

        bool isModified() const;

        /// Writes the project to filePath(), which must not be empty.
        bool save(DiagnosticList &diagnostics);

        /// Writes the project to \a path as \c .usth, which then becomes filePath().
        bool saveAs(const std::filesystem::path &path, DiagnosticList &diagnostics);

        /// Writes the project to \a path as a UST, with \c CacheDir named after \a path.
        /// \a options.file is replaced by \a path.
        bool exportUst(const std::filesystem::path &path, UstDocument::ExportOptions options,
                       DiagnosticList &diagnostics) const;

        /// The voice bank of the track as last read by loadVoiceBank(), or \c nullptr if none
        /// was read.
        ///
        /// A snapshot: later changes of the voice bank on disk do not affect it, as a project
        /// open in UTAU is not affected either. See the section on changes on disk in
        /// docs/Editing.md.
        std::shared_ptr<const VoiceBank> voiceBank() const;

        /// Reads the voice bank that the \c voiceDir of the track denotes, resolved against
        /// \a locations as by Track::voiceDirectory(), and makes it voiceBank().
        ///
        /// A directory whose encoding nothing on disk determines is read in the encoding that
        /// \a selector returns, and that encoding is recorded in the directory at once, so that
        /// the user is asked only once. The files of the voice bank are otherwise left as they
        /// are. Failing to record it is reported as a warning, since the voice bank is read
        /// nonetheless.
        ///
        /// \param selector may be null, in which case such a directory is left out, see
        ///        VoiceBank::open()
        /// \return whether a voice bank was read. voiceBank() is \c nullptr otherwise, and
        ///         \a diagnostics holds the reason unless the track names no voice bank.
        bool loadVoiceBank(const VoiceLocations &locations, VoiceBankCharsetSelector *selector,
                           DiagnosticList &diagnostics);

        /// Makes \a bank voiceBank() without reading the disk: the snapshot of the voice bank as
        /// an editor saved it. Neither an edit of the project nor a change of its saved state,
        /// see the section on a saved voice bank in docs/Editing.md.
        void setVoiceBank(std::shared_ptr<const VoiceBank> bank);

    Q_SIGNALS:
        void modifiedChanged(bool modified);

        /// Emitted when filePath() changes, on saveAs().
        void filePathChanged();

        /// Emitted when voiceBank() changes, on loadVoiceBank() and setVoiceBank().
        void voiceBankChanged();

    private:
        ProjectDocument(const Project &project, const std::filesystem::path &sourcePath,
                        bool native, QObject *parent);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_EDIT_PROJECTDOCUMENT_H
