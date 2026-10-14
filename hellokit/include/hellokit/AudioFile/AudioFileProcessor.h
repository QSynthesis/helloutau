#ifndef HELLOKIT_AUDIOFILE_AUDIOFILEPROCESSOR_H
#define HELLOKIT_AUDIOFILE_AUDIOFILEPROCESSOR_H

#include <filesystem>
#include <functional>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>
#include <QtCore/QFlags>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/AudioFile/HelloKitAudioFileGlobal.h>
#include <hellokit/AudioFile/WaveMetadata.h>

namespace hello::kit {

    /// Rewrites the audio files of a voice bank in the format that UTAU and its synth tools read
    /// most reliably, by default 44100 Hz, one channel, 16-bit integer samples, no metadata and a
    /// plain header. See the processing of audio files in docs/VoiceBankEditor.md.
    ///
    /// No processing moves the audio in time, so that the values of oto.ini, which are measured
    /// in milliseconds, stay valid.
    class HELLOKIT_AUDIOFILE_EXPORT AudioFileProcessor {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::AudioFileProcessor)
    public:
        /// The derivation of one channel from several channels.
        enum ChannelMix {
            AverageChannels, ///< The average of the channels
            FirstChannel,    ///< The first channel, the left channel of stereo audio
            SecondChannel,   ///< The second channel, the right channel of stereo audio
        };

        /// The processing of a file. The default is the format that UTAU and its synth tools read
        /// most reliably.
        struct Options {
            /// The sample rate to convert to, or \c std::nullopt to keep the sample rate
            std::optional<int> sampleRate = 44100;
            /// The derivation of one channel, or \c std::nullopt to keep the channels
            std::optional<ChannelMix> mono = AverageChannels;
            /// Whether the samples are written as 16-bit integers rather than in their format
            bool sixteenBits = true;
            /// Whether TPDF dither is added to samples that are rounded to integers
            bool dithered = true;
            /// Whether the chunks besides the format and the audio are removed
            bool removingMetadata = true;
            /// Whether a \c WAVE_FORMAT_EXTENSIBLE header is written as a plain header
            bool plainHeader = true;
            /// Whether the mean of each channel is subtracted from its samples
            bool removingDcOffset = false;
            /// The peak level in decibels relative to full scale to which each file is scaled, or
            /// \c std::nullopt to keep the level
            std::optional<double> peakLevel;
        };

        /// A change that a file requires.
        enum Change {
            SampleRateChange = 0x01, ///< The sample rate differs from Options::sampleRate
            ChannelChange = 0x02,    ///< The audio has several channels and Options::mono is set
            BitDepthChange = 0x04,   ///< The samples are no 16-bit integers and 16 bits are set
            MetadataChange = 0x08,   ///< The file has metadata and its removal is set
            HeaderChange = 0x10,     ///< The header is extensible and a plain header is set
            DataSizeChange = 0x20,   ///< The data chunk claims more bytes than the file holds
            DcOffsetChange = 0x40,   ///< A channel has a DC offset and its removal is set
            PeakChange = 0x80,       ///< The peak differs from Options::peakLevel
        };
        Q_DECLARE_FLAGS(Changes, Change)

        /// The changes that alter the samples, after which the frequency maps and the analysis
        /// caches made from the file are stale.
        static constexpr Changes sampleChanges = Changes::fromInt(
            SampleRateChange | ChannelChange | BitDepthChange | DcOffsetChange | PeakChange);

        /// The properties of a file on which its changes depend.
        struct HELLOKIT_AUDIOFILE_EXPORT Inspection {
            int sampleRate = 0;
            int channels = 0;
            int bitsPerSample = 0;
            bool floatingPoint = false;
            bool extensible = false;
            bool truncated = false;
            WaveMetadata::Report metadata;
            /// The largest absolute mean of a channel, a fraction of full scale
            double dcOffset = 0;
            /// The largest absolute sample, a fraction of full scale
            double peak = 0;

            /// Returns the changes that \a options require of the file.
            Changes changes(const Options &options) const;
        };

        /// The smallest DC offset that DcOffsetChange reports, 0.001 of full scale or -60 dBFS. A
        /// file with a smaller offset is not rewritten for its offset.
        static constexpr double dcOffsetThreshold = 0.001;

        /// The largest difference in decibels between the peak of a file and
        /// Options::peakLevel for which PeakChange is not reported.
        static constexpr double peakTolerance = 0.1;

        /// Returns the properties of the WAVE file of \a bytes.
        ///
        /// \return the properties, or \c std::nullopt with the reason in \a diagnostics if the
        ///         file cannot be read
        static std::optional<Inspection> inspect(QByteArrayView bytes, DiagnosticList &diagnostics);

        /// \overload for the file at \a file.
        static std::optional<Inspection> inspect(const std::filesystem::path &file,
                                                 DiagnosticList &diagnostics);

        /// Returns the properties of each WAVE file of \a files, read on \a threadCount threads
        /// at once, or on one thread per hardware thread if \a threadCount is zero, and returns
        /// on completion.
        ///
        /// \param progress called on a reading thread with the number of files read so far, or
        ///        empty
        /// \param canceled queried on the reading threads before each file, or empty. Once it
        ///        returns true, no further file is read.
        /// \return the properties in the order of \a files, \c std::nullopt for a file that
        ///         cannot be read or is not read after a cancellation. The reasons are appended
        ///         to \a diagnostics in the order of \a files.
        static QList<std::optional<Inspection>>
            inspectFiles(const QList<std::filesystem::path> &files, int threadCount,
                         DiagnosticList &diagnostics, const std::function<void(int)> &progress = {},
                         const std::function<bool()> &canceled = {});

        /// Returns the WAVE file of \a bytes processed by \a options. The samples are processed
        /// in the order of the channel mix, the removal of the DC offset, the sample rate
        /// conversion and the peak level, and are then written in 16 bits or in their format.
        /// Without a change of the samples, the samples are written unchanged. A rewritten
        /// header is always plain. Integer samples beyond full scale are limited, with a warning
        /// in \a diagnostics.
        ///
        /// \return the bytes, or \c std::nullopt with the reason in \a diagnostics if the file
        ///         cannot be read
        static std::optional<QByteArray> processed(QByteArrayView bytes, const Options &options,
                                                   DiagnosticList &diagnostics);

        /// Processes the file at \a file in place with processed(). The file is replaced only
        /// once the new file is completely written. A backup of the original file is the task of
        /// the caller.
        ///
        /// \return whether the file was processed, with the reason in \a diagnostics otherwise
        static bool process(const std::filesystem::path &file, const Options &options,
                            DiagnosticList &diagnostics);
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(AudioFileProcessor::Changes)

}

#endif // HELLOKIT_AUDIOFILE_AUDIOFILEPROCESSOR_H
