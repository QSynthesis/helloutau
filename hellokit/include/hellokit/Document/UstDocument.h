#ifndef HELLOKIT_DOCUMENT_USTDOCUMENT_H
#define HELLOKIT_DOCUMENT_USTDOCUMENT_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArrayView>
#include <QtCore/QList>
#include <QtCore/QString>

#include <stdutau/ustfile.h>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Project.h>

namespace hello::kit {

    /// A \c .ust, either one that was read or one built from a project.
    ///
    /// UST cannot say what encoding it is in beyond whether it is UTF-8, so a user often has to,
    /// and this library cannot ask one. Hence two steps with **one parse between them**: open()
    /// reads and holds the file, toProject() decodes what is already in hand.
    ///
    /// Writing is the same pair backwards, fromProject() then save().
    ///
    /// \code
    ///   auto ust = UstDocument::open(path, diagnostics);
    ///   if (!ust) return;
    ///   const auto charset = ust->settledCharset().value_or(askTheUser(*ust));
    ///   auto project = ust->toProject(charset, diagnostics);
    ///
    ///   auto out = UstDocument::fromProject(project, options, diagnostics);
    ///   if (out) out->save(path, diagnostics);
    /// \endcode
    ///
    /// \sa docs/UsthFormat.md
    class HELLOKIT_DOCUMENT_EXPORT UstDocument {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::UstDocument)
    public:
        /// What to write into a \c .ust beyond the project itself.
        struct ExportOptions {
            /// The encoding to write in. UTF-8 is also declared in \c [#VERSION] , and
            /// anything else cannot be, which is why the control note carries it either way.
            QString charset = QStringLiteral("UTF-8");

            /// Engines to name where the project names none.
            ///
            /// UTAU opening a UST with no engine in it has nothing to render with, so the
            /// ones from the local settings are written instead. The project's own are left
            /// exactly as they were found where it has them.
            QString wavtool;
            QString resampler;
        };

        /// Reads \a path once. Nothing in it is decoded.
        static std::optional<UstDocument> open(const std::filesystem::path &path,
                                               DiagnosticList &diagnostics);

        /// Writes it to \a path.
        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// Turns \a project into a UST, control note and all, without writing anything yet.
        ///
        /// All the encoding happens here, which is why \a options belongs here and not on
        /// save().
        static std::optional<UstDocument> fromProject(const Project &project,
                                                      const ExportOptions &options,
                                                      DiagnosticList &diagnostics);

        /// The encoding recorded in the control note, which is the one authority there is.
        ///
        /// Readable before the encoding is known, because the payload is plain ASCII.
        std::optional<QString> recordedCharset() const;

        /// Whether \c [#VERSION] carries \c Charset=UTF-8 , which is all UST itself can say.
        bool declaresUtf8() const;

        /// What the two above settle between them, or nothing where the user has to say.
        std::optional<QString> settledCharset() const;

        /// Whether the control note is there, which is what says HelloUTAU wrote this file.
        ///
        /// It decides how the text in it has to be read: escapes mean what they say only in a
        /// file that was written with them.
        bool hasControlNote() const {
            return m_hasControlNote;
        }

        /// \name Text that has not been decoded
        ///
        /// For a chooser to show under each candidate encoding. Decoding it here would answer
        /// the question before it was put.
        ///
        /// \warning These look into this object and do not outlive it.
        /// @{
        QByteArrayView rawProjectName() const;
        QByteArrayView rawVoiceDir() const;
        QList<QByteArrayView> rawLyrics() const;
        /// @}

        /// Decodes what open() read, in \a charset.
        ///
        /// The control note is taken out. Leaving it in would put a second one in the next file
        /// written, and repeated round trips would grow a run of leaders.
        std::optional<Project> toProject(const QString &charset, DiagnosticList &diagnostics) const;

        /// The parse underneath, for what \c Project has no field for.
        ///
        /// \warning Every string in it is raw bytes in the file's own encoding. Put anything
        ///          taken from here through \c TextCodec before treating it as text.
        const utau::UstFile &file() const {
            return m_file;
        }

    private:
        UstDocument() = default;

        utau::UstFile m_file;

        // Worked out while the file is read, or set while it is built. Both are asked before
        // anything is decoded, and neither depends on the encoding.
        std::optional<QString> m_recorded;
        bool m_utf8 = false;
        bool m_hasControlNote = false;
    };

}

#endif // HELLOKIT_DOCUMENT_USTDOCUMENT_H
