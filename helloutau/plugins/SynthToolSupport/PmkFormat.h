#ifndef HELLOUTAU_SYNTHTOOLSUPPORT_PMKFORMAT_H
#define HELLOUTAU_SYNTHTOOLSUPPORT_PMKFORMAT_H

#include <QtCore/QCoreApplication>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <SynthToolSupport/SynthToolSupportPluginGlobal.h>

namespace hello::daw {

    /// The pitch marks of TIPS, beside the audio file as a_wav.pmk, registered by the
    /// SynthToolSupport plugin. See docs/FrequencyMaps.md.
    ///
    /// A pitch mark records the end of a period and its length in samples at 44100 Hz. The map
    /// of a pmk has a frame every 256 samples at 44100 Hz, each with the period of the mark that
    /// covers the start of the frame. The format records no amplitudes.
    class SYNTHTOOLSUPPORTPLUGIN_EXPORT PmkFormat : public kit::FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::PmkFormat)
    public:
        static constexpr char formatId[] = "pmk";

        /// The sample rate of the positions and the periods of a pmk, regardless of the audio
        /// file
        static constexpr int markRate = 44100;

        /// The hop of the frames in samples at markRate
        static constexpr int markHop = 256;

        /// The shortest period of a voiced mark in samples at markRate. A shorter period marks
        /// an unvoiced span.
        static constexpr int shortestPeriod = 50;

        QString id() const override;
        QString name() const override;
        QStringList resamplerPatterns() const override;
        QList<bool> mapsExist(const std::filesystem::path &directory,
                              const QList<std::filesystem::path> &wavs) const override;

        /// Reads the map of the pitch marks. The average frequency is that of the base period,
        /// 0 if the base period is shorter than shortestPeriod.
        std::optional<kit::FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                              kit::DiagnosticList &diagnostics) const override;

        /// Returns markHop samples at markRate in samples at \a sampleRate.
        double hopOf(int sampleRate) const override;

        std::optional<std::filesystem::path>
            sidecarNameOf(const std::filesystem::path &wav) const override;

        /// Reports each map that read() rejects, which is not repairable.
        QList<MapFileDamage> checkMapFiles(const std::filesystem::path &directory,
                                           const QList<std::filesystem::path> &wavs) const override;

        bool canWrite() const override;

        /// Requires the frames of hopOf() at the sample rate of the audio file. Writes marks of
        /// the period of each frame until the frame ends, as frqeditor does. A frame at 55 Hz or
        /// below is written as an unvoiced span. A map without an average frequency is written
        /// with FrequencyMap::pitchAverage(). The base period of an existing map is kept if the
        /// average frequency is unchanged.
        std::optional<QList<FileChange>>
            changesWriting(const std::filesystem::path &directory, const QList<MapWrite> &writes,
                           kit::DiagnosticList &diagnostics) const override;
    };

}

#endif // HELLOUTAU_SYNTHTOOLSUPPORT_PMKFORMAT_H
