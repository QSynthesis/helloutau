#ifndef HELLOKIT_SYNTH_WAVEAUDIO_H
#define HELLOKIT_SYNTH_WAVEAUDIO_H

#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// The audio of a WAVE file, as floating-point samples.
    ///
    /// Read to play a rendered track: the wavtool writes 16-bit PCM at 44100 Hz, and other
    /// synth tools may write other sample formats. The formats read are integer PCM of 8, 16, 24
    /// and 32 bits, IEEE floating point of 32 and 64 bits, and either as the subformat of
    /// \c WAVE_FORMAT_EXTENSIBLE, as the Microsoft documentation of the WAVE format defines them
    /// (https://learn.microsoft.com/en-us/windows/win32/multimedia/waveformatex and
    /// https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible).
    struct HELLOKIT_SYNTH_EXPORT WaveAudio {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::WaveAudio)
    public:
        int sampleRate = 0;
        int channels = 0;

        /// The samples, interleaved by channel, each between -1 and 1.
        std::vector<float> samples;

        inline qsizetype frameCount() const {
            return channels > 0 ? qsizetype(samples.size()) / channels : 0;
        }

        /// The duration in milliseconds.
        double duration() const;

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

#endif // HELLOKIT_SYNTH_WAVEAUDIO_H
