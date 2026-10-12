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

#include <hellokit/VoiceBank/FrequencyTable.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Driver of a frequency table format. A frequency table is the analysis file that a
    /// resampler generates for an audio file. Built-in formats and plugin formats are both
    /// registered in a FrequencyFormatRegistry. See docs/FrequencyTables.md.
    ///
    /// The interface is read-only. It neither writes tables nor executes resamplers, and returns
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

        /// A change of a file of a directory, which follows renames of its audio files.
        struct HELLOKIT_VOICEBANK_EXPORT FileChange {
            enum Kind {
                Rename,  ///< The file is renamed to \a to
                Rewrite, ///< The file is written with \a contents
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

        /// Returns whether the audio file \a wav has a table of this format.
        virtual bool exists(const std::filesystem::path &wav) const = 0;

        /// Reads the table of the audio file \a wav with the sample rate \a sampleRate.
        ///
        /// \return the table, or \c std::nullopt with the reason in \a diagnostics if the table
        ///         is missing or invalid
        virtual std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
                                                   DiagnosticList &diagnostics) const = 0;

        /// Returns the name of the table of the audio file \a wav in the directory of \a wav, if
        /// the format keeps one table beside each audio file, or \c std::nullopt, which the
        /// default implementation returns, if it does not.
        virtual std::optional<std::filesystem::path>
            tableNameOf(const std::filesystem::path &wav) const;

        /// Returns the changes of the tables of this format that follow \a renames, all renames
        /// of audio files in the directory \a directory at once, so that a table that serves
        /// several audio files changes once. An audio file without a table needs no change.
        /// The caller executes the changes together with the renames.
        ///
        /// The default implementation renames each table that tableNameOf() names after the new
        /// name of its audio file. A format whose tables are kept otherwise overrides it.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if they
        ///         cannot be determined, which the default implementation returns if
        ///         tableNameOf() names no table
        virtual std::optional<QList<FileChange>>
            changesFollowing(const std::filesystem::path &directory,
                             const QList<AudioRename> &renames, DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
