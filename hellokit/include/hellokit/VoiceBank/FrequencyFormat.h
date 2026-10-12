#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/FrequencyMap.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Driver of a frequency map format. A frequency map is the analysis file that a
    /// resampler generates for an audio file. Built-in formats and plugin formats are both
    /// registered in a FrequencyFormatRegistry. See docs/FrequencyMaps.md.
    ///
    /// The interface is read-only. It neither writes maps nor executes resamplers, and returns
    /// the changes of files that the caller executes.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FrequencyFormat)
    public:
        /// The rename of an audio file in its directory.
        struct AudioRename {
            /// The name on disk
            std::filesystem::path from;
            /// The new name
            std::filesystem::path to;
        };

        /// A change of a file of a directory, which the caller executes.
        struct HELLOKIT_VOICEBANK_EXPORT FileChange {
            enum Kind {
                Rename,  ///< The file is renamed to \a to
                Rewrite, ///< The file is written with \a contents
                Remove,  ///< The file is removed
            };

            Kind kind = Rename;
            /// The name of the file in the directory
            std::filesystem::path path;
            /// The new name of a renamed file
            std::filesystem::path to;
            /// The new contents of a rewritten file
            QByteArray contents;

            /// Returns the rename of the file \a from to \a to.
            static FileChange renamed(std::filesystem::path from, std::filesystem::path to);

            /// Returns the rewrite of the file \a path with \a contents.
            static FileChange rewritten(std::filesystem::path path, QByteArray contents);

            /// Returns the removal of the file \a path.
            static FileChange removed(std::filesystem::path path);
        };

        virtual ~FrequencyFormat();

        /// Returns a short identifier that is unique in a registry, such as \c frq.
        virtual QString id() const = 0;

        /// Returns the display name, which includes the name of the resampler.
        virtual QString name() const = 0;

        /// Returns the wildcard patterns of the file names of the resamplers that read this
        /// format, such as <tt>moresampler*.exe</tt>. The registry selects the format of the
        /// configured resampler by these patterns. Empty if the format is selected only
        /// manually.
        ///
        /// \sa FrequencyFormats::formatForResampler()
        virtual QStringList resamplerPatterns() const = 0;

        /// Returns whether the audio file \a wav has a map of this format.
        virtual bool exists(const std::filesystem::path &wav) const = 0;

        /// Reads the map of the audio file \a wav with the sample rate \a sampleRate.
        ///
        /// \return the map, or \c std::nullopt with the reason in \a diagnostics if the map
        ///         is missing or invalid
        virtual std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                                   DiagnosticList &diagnostics) const = 0;

        /// Returns the name of the sidecar file of the audio file \a wav, the file beside \a wav
        /// that holds its map alone, if the format keeps one; otherwise \c std::nullopt, which
        /// the default implementation returns. A file that holds the maps of several audio
        /// files, such as \c desc.mrq, is no sidecar file.
        virtual std::optional<std::filesystem::path>
            sidecarNameOf(const std::filesystem::path &wav) const;

        /// Returns the changes of the maps of this format that follow \a renames, all renames
        /// of audio files in the directory \a directory at once, so that a file that holds the
        /// maps of several audio files changes once. An audio file without a map needs no
        /// change. The caller executes the changes together with the renames.
        ///
        /// The default implementation renames each sidecar file that sidecarNameOf() names
        /// after the new name of its audio file. A format whose maps are kept otherwise
        /// overrides it.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if they
        ///         cannot be determined, which the default implementation returns if
        ///         sidecarNameOf() names no sidecar file
        virtual std::optional<QList<FileChange>>
            changesFollowing(const std::filesystem::path &directory,
                             const QList<AudioRename> &renames, DiagnosticList &diagnostics) const;

        /// Returns the changes that remove the maps of this format of the audio files \a wavs,
        /// names in the directory \a directory, at once. An audio file without a map needs no
        /// change. Removing a map makes a resampler generate it again.
        ///
        /// The default implementation removes each sidecar file that sidecarNameOf() names. A
        /// format whose maps are kept otherwise overrides it.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if they
        ///         cannot be determined, which the default implementation returns if
        ///         sidecarNameOf() names no sidecar file
        virtual std::optional<QList<FileChange>>
            changesRemoving(const std::filesystem::path &directory,
                            const QList<std::filesystem::path> &wavs,
                            DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
