#ifndef HELLOKIT_DOCUMENT_USTDOCUMENT_H
#define HELLOKIT_DOCUMENT_USTDOCUMENT_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QByteArrayView>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Project.h>

namespace hello::kit {

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

    /// A \c .ust that has been read, but whose text has not been decoded.
    ///
    /// Reading a UST runs into a circle: the text cannot be read without knowing the encoding,
    /// and UST mostly cannot say what its encoding is, since \c Charset only manages to say
    /// whether it is UTF-8. Where the file does not settle it, a user has to, and this library
    /// cannot ask one.
    ///
    /// So reading is two steps with **one parse between them**. open() reads the file and holds
    /// it. What comes back answers what the file can say about itself and hands out the
    /// undecoded bytes a chooser needs to show. toProject() then decodes what is already in
    /// hand, so the encoding the user picked costs nothing more than the decoding.
    ///
    /// \code
    ///   auto ust = UstDocument::open(path, diagnostics);
    ///   if (!ust) return;
    ///   const auto charset = ust->settledCharset().value_or(askTheUser(*ust));
    ///   auto project = ust->toProject(charset, diagnostics);
    /// \endcode
    ///
    /// \sa docs/UsthFormat.md
    class HELLOKIT_DOCUMENT_EXPORT UstDocument {
    public:
        ~UstDocument();

        UstDocument(UstDocument &&RHS) noexcept;
        UstDocument &operator=(UstDocument &&RHS) noexcept;

        /// Reads \a path once. Nothing in it is decoded.
        static std::optional<UstDocument> open(const std::filesystem::path &path,
                                               DiagnosticList &diagnostics);

        /// The encoding recorded in the control note, which is the one authority there is.
        ///
        /// Readable without knowing the file's encoding, because the payload is plain ASCII.
        /// That is what it is for.
        std::optional<QString> recordedCharset() const;

        /// Whether \c [#VERSION] carries \c Charset=UTF-8 , which is all UST itself can say.
        bool declaresUtf8() const;

        /// What the two above settle between them, or nothing where the user has to say.
        std::optional<QString> settledCharset() const;

        /// \name Text that has not been decoded
        ///
        /// For a chooser to show under each candidate encoding, so that the user can see which
        /// one is right. Decoding it here would answer the question before it was put.
        ///
        /// \warning These look into this object and do not outlive it.
        /// @{
        QByteArrayView rawProjectName() const;
        QByteArrayView rawVoiceDir() const;
        QList<QByteArrayView> rawLyrics() const;
        /// @}

        /// Decodes what open() read, in \a charset.
        ///
        /// The control note is taken out rather than becoming a note of the project. Leaving it
        /// in would mean writing a second one on the way out, and a file going round a few times
        /// would grow a run of half second leaders.
        std::optional<Project> toProject(const QString &charset,
                                         DiagnosticList &diagnostics) const;

        /// Writes \a project out as a UST, control note and all.
        static bool write(const Project &project, const std::filesystem::path &path,
                          const UstExportOptions &options, DiagnosticList &diagnostics);

    private:
        UstDocument();

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_DOCUMENT_USTDOCUMENT_H
