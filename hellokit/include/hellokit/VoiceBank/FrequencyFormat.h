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
    /// The interface neither changes files nor executes resamplers. It returns the changes of
    /// files, which the caller executes.
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

        /// A file that the resampler of a format derives from an audio file and reuses in later
        /// renders, such as the LLSM file of moresampler. A cache holds the fundamental
        /// frequency and becomes stale once the map changes. The resampler writes a missing cache
        /// again from the map when it renders the audio file.
        struct HELLOKIT_VOICEBANK_EXPORT AnalysisCache {
            /// The derivation of the cache file name from the audio file name.
            enum Naming {
                AppendedSuffix,    ///< The suffix follows the whole name: a.wav.llsm
                ReplacedExtension, ///< The suffix replaces the extension: a.star
            };

            /// The name for display, such as LLSM
            QString name;
            Naming naming = AppendedSuffix;
            /// The suffix with its leading dot, such as .llsm
            QString suffix;

            /// Returns the name of the cache file of the audio file \a wav.
            std::filesystem::path fileNameOf(const std::filesystem::path &wav) const;
        };

        /// The new map of an audio file in its directory.
        struct MapWrite {
            /// The name of the audio file in the directory
            std::filesystem::path wav;
            FrequencyMap map;
            /// The sample rate of the audio file
            int sampleRate = 0;
            /// Whether the frequencies were edited by hand rather than converted, which
            /// \c desc.mrq records
            bool editedByHand = true;
        };

        /// The damage of a file that checkMapFiles() detects, a file that holds maps or a cache
        /// file of analysisCaches().
        struct MapFileDamage {
            /// The name of the file in the directory
            std::filesystem::path path;
            /// The damage, for display
            QString description;
            /// Whether changesRepairing() repairs the file in place. A damaged file that is not
            /// repairable is removed, and the resampler writes it again.
            bool repairable = false;
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

        /// Returns whether each audio file of \a wavs, names in the directory \a directory, has a
        /// map of this format, in the order of \a wavs. The audio files of a directory are
        /// queried at once, so that a format that keeps the maps of a directory in one file,
        /// such as \c desc.mrq, reads the file once.
        virtual QList<bool> mapsExist(const std::filesystem::path &directory,
                                      const QList<std::filesystem::path> &wavs) const = 0;

        /// Reads the map of the audio file \a wav with the sample rate \a sampleRate.
        ///
        /// \return the map, or \c std::nullopt with the reason in \a diagnostics if the map
        ///         is missing or invalid
        virtual std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                                 DiagnosticList &diagnostics) const = 0;

        /// Returns the hop in samples of the maps that the resampler of this format generates for
        /// an audio file with the sample rate \a sampleRate. A map converted to this format is
        /// resampled to this hop. The default implementation returns 256, the hop of the UTAU
        /// resampler at every sample rate.
        virtual double hopOf(int sampleRate) const;

        /// Returns the name of the sidecar file of the audio file \a wav, the file beside \a wav
        /// that holds its map alone, if the format keeps one, or else \c std::nullopt. The default
        /// implementation returns \c std::nullopt. A file that holds the maps of several audio
        /// files, such as \c desc.mrq, is no sidecar file.
        virtual std::optional<std::filesystem::path>
            sidecarNameOf(const std::filesystem::path &wav) const;

        /// Returns the name of the file in the folder of the audio file \a wav that holds its
        /// map, for display. The default implementation returns the name of the sidecar file. A
        /// format that keeps the maps of a folder in one file, such as \c desc.mrq, returns that
        /// file. Returns \c std::nullopt if the format keeps the map elsewhere.
        virtual std::optional<std::filesystem::path>
            mapFileNameOf(const std::filesystem::path &wav) const;

        /// Returns the analysis caches of the resampler of this format. The default
        /// implementation returns none.
        virtual QList<AnalysisCache> analysisCaches() const;

        /// Returns the changes that rename the maps of this format after \a renames, all renames
        /// of audio files in the directory \a directory at once, so that a file that holds the
        /// maps of several audio files changes once. An audio file without a map needs no
        /// change. The caller executes the changes together with the renames.
        ///
        /// The default implementation renames each sidecar file that sidecarNameOf() names
        /// and each cache file of analysisCaches() after the new name of its audio file. A
        /// format whose maps are kept otherwise overrides it, and renames the cache files with
        /// cacheRenames().
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if they
        ///         cannot be determined, which the default implementation returns if
        ///         sidecarNameOf() names no sidecar file
        virtual std::optional<QList<FileChange>>
            changesRenaming(const std::filesystem::path &directory,
                            const QList<AudioRename> &renames, DiagnosticList &diagnostics) const;

        /// Returns the changes that remove the maps of this format of the audio files \a wavs,
        /// names in the directory \a directory, at once, without their analysis caches. An
        /// audio file without a map needs no change.
        ///
        /// The default implementation removes each sidecar file that sidecarNameOf() names. A
        /// format whose maps are kept otherwise overrides it.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if they
        ///         cannot be determined, which the default implementation returns if
        ///         sidecarNameOf() names no sidecar file
        /// \sa FrequencyFormats::removeMaps()
        virtual std::optional<QList<FileChange>>
            changesRemoving(const std::filesystem::path &directory,
                            const QList<std::filesystem::path> &wavs,
                            DiagnosticList &diagnostics) const;

        /// Returns whether the format writes maps with changesWriting(). The default
        /// implementation returns false.
        virtual bool canWrite() const;

        /// Returns the changes that write the maps \a writes of audio files in the directory
        /// \a directory, at once, so that a file that holds the maps of several audio files
        /// changes once. The parts of an existing map beyond FrequencyMap are kept. The analysis
        /// caches are not removed.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if a map
        ///         cannot be written, which the default implementation returns
        /// \sa FrequencyFormats::writeMaps()
        virtual std::optional<QList<FileChange>>
            changesWriting(const std::filesystem::path &directory, const QList<MapWrite> &writes,
                           DiagnosticList &diagnostics) const;

        /// Returns the damaged files among the maps and the analysis caches of the audio files
        /// \a wavs, names in the directory \a directory. The default implementation returns none.
        virtual QList<MapFileDamage> checkMapFiles(const std::filesystem::path &directory,
                                                   const QList<std::filesystem::path> &wavs) const;

        /// Returns the changes that repair the file \a path of the directory \a directory in
        /// place, for a damage that checkMapFiles() reports as repairable.
        ///
        /// \return the changes, or \c std::nullopt with the reason in \a diagnostics if the file
        ///         cannot be repaired, which the default implementation returns
        virtual std::optional<QList<FileChange>>
            changesRepairing(const std::filesystem::path &directory,
                             const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// Returns the changes that remove the existing cache files of analysisCaches() of the
        /// audio files \a wavs, names in the directory \a directory.
        QList<FileChange> changesInvalidating(const std::filesystem::path &directory,
                                              const QList<std::filesystem::path> &wavs) const;

    protected:
        /// Returns the renames of the existing cache files of analysisCaches() after
        /// \a renames in the directory \a directory.
        QList<FileChange> cacheRenames(const std::filesystem::path &directory,
                                       const QList<AudioRename> &renames) const;

        /// \name Helpers of the formats
        /// @{

        /// Returns the contents of the file \a path, or \c std::nullopt with the reason in
        /// \a diagnostics if it cannot be read.
        static std::optional<QByteArray> contentsOf(const std::filesystem::path &path,
                                                    DiagnosticList &diagnostics);

        /// Reports \a path in \a diagnostics as no map of its format or an incomplete map, and
        /// returns \c std::nullopt.
        static std::optional<FrequencyMap> damaged(const std::filesystem::path &path,
                                                   DiagnosticList &diagnostics);

        /// Returns whether each audio file of \a wavs, names in the directory \a directory, has
        /// the sidecar file that sidecarNameOf() names, for mapsExist() of a format whose maps
        /// are sidecar files.
        QList<bool> sidecarsExist(const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) const;

        /// Reports in \a diagnostics that the map of the audio file \a wav cannot be written for
        /// \a reason, and returns \c std::nullopt.
        static std::nullopt_t unwritable(const std::filesystem::path &wav, const QString &reason,
                                         DiagnosticList &diagnostics);

        /// Returns the hop of \a write as an integral number of samples, for a format that
        /// records an integer, or \c std::nullopt with the reason in \a diagnostics.
        static std::optional<qint32> integralHop(const MapWrite &write,
                                                 DiagnosticList &diagnostics);

        /// @}
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
