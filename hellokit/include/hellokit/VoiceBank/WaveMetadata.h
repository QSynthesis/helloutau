#ifndef HELLOKIT_VOICEBANK_WAVEMETADATA_H
#define HELLOKIT_VOICEBANK_WAVEMETADATA_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The parts of a WAVE file besides its format and its audio: the chunks that recording and
    /// editing programs add, such as \c LIST with \c INFO text, \c id3 , \c bext , \c cue ,
    /// \c smpl and \c JUNK , and bytes after the last chunk. See the removal of audio metadata
    /// in docs/VoiceBankEditor.md.
    ///
    /// A WAVE file is a RIFF file of chunks, each an identifier of four bytes, a little-endian
    /// size of four bytes and that many bytes padded to an even size, as the Microsoft
    /// documentation of RIFF defines it
    /// (https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-).
    /// Removing the metadata leaves the RIFF header, the \c fmt chunk and the \c data chunk, in
    /// that order, with the sizes written anew. The samples are unchanged, and so is every
    /// frequency table made from them.
    class HELLOKIT_VOICEBANK_EXPORT WaveMetadata {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::WaveMetadata)
    public:
        /// The metadata of a file.
        struct Report {
            /// The identifiers of the chunks other than the first \c fmt and \c data chunks, in
            /// their order.
            QList<QByteArray> chunks;

            /// The number of bytes after the last chunk, too few to form one.
            qint64 trailingBytes = 0;

            inline bool isEmpty() const {
                return chunks.isEmpty() && trailingBytes == 0;
            }
        };

        /// Returns the metadata of the WAVE file of \a bytes.
        ///
        /// \return the metadata, or \c std::nullopt if \a bytes is no RIFF WAVE file with a
        ///         \c fmt and a \c data chunk, with the reason in \a diagnostics
        static std::optional<Report> find(QByteArrayView bytes, DiagnosticList &diagnostics);

        /// \overload for the file at \a file, of which only the chunk headers are read.
        static std::optional<Report> find(const std::filesystem::path &file,
                                          DiagnosticList &diagnostics);

        /// Returns the WAVE file of \a bytes without its metadata. A \c data chunk that claims
        /// more bytes than the file holds keeps the bytes the file holds.
        ///
        /// \return the bytes, or \c std::nullopt as for find()
        static std::optional<QByteArray> stripped(QByteArrayView bytes,
                                                  DiagnosticList &diagnostics);

        /// Writes the WAVE file at \a file again without its metadata. The file is replaced only
        /// once the new one is completely written.
        ///
        /// \return whether the file was written, with the reason in \a diagnostics otherwise
        static bool strip(const std::filesystem::path &file, DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_VOICEBANK_WAVEMETADATA_H
