#ifndef HELLOKIT_AUDIOFILE_WAVEAUDIO_H
#define HELLOKIT_AUDIOFILE_WAVEAUDIO_H

#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/AudioFile/HelloKitAudioFileGlobal.h>

namespace hello::kit {

    /// The audio of a WAVE file, as floating-point samples.
    ///
    /// The formats read are integer PCM of 8, 16, 24 and 32 bits, IEEE floating point of 32 and
    /// 64 bits, and either as the subformat of \c WAVE_FORMAT_EXTENSIBLE, as the Microsoft
    /// documentation of the WAVE format defines them
    /// (https://learn.microsoft.com/en-us/windows/win32/multimedia/waveformatex and
    /// https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible).
    /// The wavtool writes 16-bit PCM at 44100 Hz, and other synth tools and the recordings of
    /// voice banks use other sample formats.
    struct HELLOKIT_AUDIOFILE_EXPORT WaveAudio {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::WaveAudio)
    public:
        int sampleRate = 0;
        int channels = 0;

        /// The bits of a sample in the file: 8, 16, 24 or 32 for integers, 32 or 64 for floating
        /// point. formatChunk() and dataChunk() write the samples in this format.
        int bitsPerSample = 16;

        /// Whether the samples of the file are floating point rather than integers.
        bool floatingPoint = false;

        /// Whether the format of the file read is \c WAVE_FORMAT_EXTENSIBLE.
        bool extensible = false;

        /// Whether the data chunk of the file read claims more bytes than the file holds.
        bool truncated = false;

        /// The samples, interleaved by channel, each between -1 and 1.
        std::vector<float> samples;

        inline qsizetype frameCount() const {
            return channels > 0 ? qsizetype(samples.size()) / channels : 0;
        }

        /// The duration in milliseconds.
        double duration() const;

        /// Returns the audio converted to the sample rate \a sampleRate, which must be positive,
        /// as the static overload converts the samples. The other members are kept.
        WaveAudio resampled(int sampleRate) const;

        /// Converts \a samples, interleaved by \a channels, from \a sourceRate to \a targetRate
        /// with r8brain-free-src, each channel separately. The result has the same duration,
        /// rounded to whole frames, and no delay. Both rates must be positive.
        static std::vector<float> resampled(const std::vector<float> &samples, int channels,
                                            int sourceRate, int targetRate);

        /// Returns the contents of the \c fmt chunk of the audio, a \c WAVE_FORMAT_PCM or
        /// \c WAVE_FORMAT_IEEE_FLOAT header without extension.
        QByteArray formatChunk() const;

        /// Returns the contents of the \c data chunk of the audio in the sample format of
        /// bitsPerSample and floatingPoint. Integer samples are rounded to the nearest value and
        /// limited to the range of the format.
        QByteArray dataChunk() const;

        /// Reads the WAVE file at \a path.
        ///
        /// A data chunk that claims more bytes than the file holds is read up to the end of the
        /// file, with a warning, since a synth tool that stopped early leaves such a file and the
        /// audio before that point is intact.
        ///
        /// \return the audio, or \c std::nullopt with the reason in \a diagnostics
        static std::optional<WaveAudio> read(const std::filesystem::path &path,
                                             DiagnosticList &diagnostics);

        /// \overload for the bytes of a file.
        static std::optional<WaveAudio> fromBytes(QByteArrayView bytes,
                                                  DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_AUDIOFILE_WAVEAUDIO_H
