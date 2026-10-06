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

    class TextCodec;

    /// A \c .ust, either read from disk or built from a project.
    ///
    /// UST can declare its encoding only as UTF-8 or not UTF-8, so the user must often specify
    /// it, and this library cannot ask the user. Reading therefore takes two steps with **a
    /// single parse**: open() reads and retains the file, and toProject() decodes the retained
    /// data.
    ///
    /// Writing is the same pair in reverse: fromProject(), then save().
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
        /// Export settings beyond the project data.
        struct ExportOptions {
            /// The output encoding. UTF-8 is additionally declared in \c [#VERSION] , and no
            /// other encoding can be declared there, so the control note records the encoding
            /// in every case.
            QString charset = QStringLiteral("UTF-8");

            /// Engines to write if the project specifies none.
            ///
            /// A UST without engines cannot be rendered when opened in UTAU, so the engines from
            /// the local settings are written instead. Engines specified by the project are
            /// written unchanged.
            QString wavtool;
            QString resampler;

            /// The file the UST will be saved as. If not empty, \c CacheDir is written as UTAU
            /// writes it on save, Project::cacheDirTextOf() this file, instead of the value in the
            /// project.
            std::filesystem::path file;
        };

        /// Reads \a path once. No content is decoded.
        static std::optional<UstDocument> open(const std::filesystem::path &path,
                                               DiagnosticList &diagnostics);

        /// Writes the document to \a path.
        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// Converts \a project into a UST, including the control note, without writing it.
        ///
        /// All encoding takes place here, which is why \a options is a parameter of this
        /// function and not of save().
        static std::optional<UstDocument> fromProject(const Project &project,
                                                      const ExportOptions &options,
                                                      DiagnosticList &diagnostics);

        /// The encoding recorded in the control note, which is the sole authoritative source.
        ///
        /// Available before the encoding is known, because the payload is plain ASCII.
        std::optional<QString> recordedCharset() const;

        /// Returns whether \c [#VERSION] contains \c Charset=UTF-8 , the only encoding
        /// declaration UST supports.
        bool declaresUtf8() const;

        /// The encoding determined by the two functions above, or \c std::nullopt if the user
        /// must specify it.
        std::optional<QString> settledCharset() const;

        /// Returns whether the control note is present, which identifies a file written by
        /// HelloUtau.
        ///
        /// Determines how the text must be read: escape sequences are meaningful only in a file
        /// written with them.
        inline bool hasControlNote() const {
            return m_hasControlNote;
        }

        /// \name Undecoded text
        ///
        /// For an encoding selector to display under each candidate encoding. The text is not
        /// decoded here, because decoding would presuppose the answer.
        ///
        /// \warning The views refer into this object and must not outlive it.
        /// @{
        QByteArrayView rawProjectName() const;
        QByteArrayView rawVoiceDir() const;
        QList<QByteArrayView> rawLyrics() const;
        /// @}

        /// Decodes the data read by open() in \a charset.
        ///
        /// The control note is removed. Otherwise the next file written would contain a second
        /// one, and repeated round trips would accumulate leading control notes.
        std::optional<Project> toProject(const QString &charset, DiagnosticList &diagnostics) const;

        /// \name Notes
        ///
        /// The conversion of one note, as open() with toProject() and fromProject() convert
        /// every note, for another file of UST notes, such as the temporary file of a plugin.
        /// Every string of a utau::Note is bytes in \a codec. Escape sequences are read and
        /// written only in a file that this program wrote in an encoding other than UTF-8, see
        /// TextCodec::escape().
        /// @{
        static Note noteFromUst(const utau::Note &note, const TextCodec &codec, bool unescaping);
        static utau::Note noteToUst(const Note &note, const TextCodec &codec, bool escaping);
        /// @}

        /// The underlying parse, for data that \c Project cannot represent.
        ///
        /// \warning Every string is raw bytes in the encoding of the file. Decode these through
        ///          \c TextCodec before treating them as text.
        inline const utau::UstFile &file() const {
            return m_file;
        }

    private:
        UstDocument() = default;

        utau::UstFile m_file;

        // Determined while the file is read, or set while it is built. Both are queried before
        // decoding, and neither depends on the encoding.
        std::optional<QString> m_recorded;
        bool m_utf8 = false;
        bool m_hasControlNote = false;
    };

}

#endif // HELLOKIT_DOCUMENT_USTDOCUMENT_H
