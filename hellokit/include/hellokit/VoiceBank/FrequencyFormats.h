#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATS_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATS_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The frequency map formats registered in a host, in the order of registration. See the
    /// registration interfaces in docs/Plugins.md.
    ///
    /// FrequencyFormats creates and holds the registry and creates one instance of each format.
    /// Built-in formats and plugin formats are registered alike, with an \c Add or \c AddFactory
    /// object that a plugin creates in initialize() and destroys in aboutToShutdown(), before its
    /// library is unloaded. The registry is used only on the application thread.
    using FrequencyFormatRegistry = OrderedObjectRegistry<FrequencyFormat>;

    /// The frequency map formats of a host, such as an editor: one instance of each format
    /// registered in registry(). The content changes as formats are registered and
    /// unregistered. See docs/FrequencyMaps.md.
    ///
    /// The object is not a singleton. The owner creates it and passes it to its users, and each
    /// object emits formatsChanged().
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormats : public QObject {
        Q_OBJECT
    public:
        explicit FrequencyFormats(QObject *parent = nullptr);
        ~FrequencyFormats();

        /// Returns the registry in which the formats of this object are registered.
        FrequencyFormatRegistry &registry() const;

        /// Returns the formats in the order of registration.
        QList<FrequencyFormat *> formats() const;

        FrequencyFormat *format(const QString &id) const;

        /// Returns the format read by the resampler \a resampler: the last registered format
        /// whose patterns match the file name case-insensitively. A format registered later
        /// therefore takes precedence over an earlier format for the same resampler. Only the
        /// file name is examined. The resampler is not executed.
        ///
        /// \return the matching format, else \c frq (the format of the UTAU resampler), or null
        ///         if neither is registered
        FrequencyFormat *formatForResampler(const std::filesystem::path &resampler) const;

        /// Removes the maps of \a format of the audio files \a wavs, names in the directory
        /// \a directory, with their analysis caches, as FrequencyFormat::changesRemoving() and
        /// FrequencyFormat::changesInvalidating() return the changes. A file that cannot be
        /// changed is reported in \a diagnostics, and the other changes are made regardless.
        ///
        /// \return whether every map and every cache was removed
        static bool removeMaps(const FrequencyFormat &format,
                               const std::filesystem::path &directory,
                               const QList<std::filesystem::path> &wavs,
                               DiagnosticList &diagnostics);

        /// Writes the maps \a writes of \a format of audio files in the directory \a directory,
        /// as FrequencyFormat::changesWriting() returns the changes, and removes the analysis
        /// caches of the audio files as well if \a invalidatingCaches is set, so that the
        /// resampler derives the caches from the new maps. Failures are reported as removeMaps()
        /// reports them.
        /// No file is changed if a map cannot be written.
        ///
        /// \return whether every map was written and every cache removed
        static bool writeMaps(const FrequencyFormat &format, const std::filesystem::path &directory,
                              const QList<FrequencyFormat::MapWrite> &writes,
                              bool invalidatingCaches, DiagnosticList &diagnostics);

        /// Removes the analysis caches of \a format of the audio files \a wavs, names in the
        /// directory \a directory, and keeps the maps, so that the resampler writes the caches
        /// again from the maps, as removeMaps() reports failures.
        ///
        /// \return whether every cache was removed
        static bool invalidateCaches(const FrequencyFormat &format,
                                     const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs,
                                     DiagnosticList &diagnostics);

    Q_SIGNALS:
        /// Emitted after a format was registered or unregistered.
        void formatsChanged();

    private:
        // Makes changes in directory, each failure reported and the others made regardless.
        static bool execute(const std::filesystem::path &directory,
                            const QList<FrequencyFormat::FileChange> &changes,
                            DiagnosticList &diagnostics);

        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(FrequencyFormats)
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATS_H
