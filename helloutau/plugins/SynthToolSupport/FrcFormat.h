#ifndef HELLOUTAU_SYNTHTOOLSUPPORT_FRCFORMAT_H
#define HELLOUTAU_SYNTHTOOLSUPPORT_FRCFORMAT_H

#include <QtCore/QCoreApplication>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <SynthToolSupport/SynthToolSupportPluginGlobal.h>

namespace hello::daw {

    /// The frequency maps of model4, beside the audio file as a_wav.frc, registered by the
    /// SynthToolSupport plugin. The layout is that of frq with different reserved bytes. See
    /// docs/FrequencyMaps.md.
    class SYNTHTOOLSUPPORTPLUGIN_EXPORT FrcFormat : public kit::FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::FrcFormat)
    public:
        static constexpr char formatId[] = "frc";

        QString id() const override;
        QString name() const override;
        QStringList resamplerPatterns() const override;
        QList<bool> mapsExist(const std::filesystem::path &directory,
                              const QList<std::filesystem::path> &wavs) const override;
        std::optional<kit::FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                              kit::DiagnosticList &diagnostics) const override;

        std::optional<std::filesystem::path>
            sidecarNameOf(const std::filesystem::path &wav) const override;

        /// Reports each map that read() rejects, which is not repairable.
        QList<MapFileDamage> checkMapFiles(const std::filesystem::path &directory,
                                           const QList<std::filesystem::path> &wavs) const override;

        bool canWrite() const override;

        /// Requires an amplitude for each frame. The reserved bytes of an existing map are kept,
        /// and a new map has those of model4. A map without an average frequency is written with
        /// FrequencyMap::pitchAverage().
        std::optional<QList<FileChange>>
            changesWriting(const std::filesystem::path &directory, const QList<MapWrite> &writes,
                           kit::DiagnosticList &diagnostics) const override;

    private:
        // Returns whether bytes is a complete map: the header and every frame that it counts
        static bool isComplete(const QByteArray &bytes);
    };

}

#endif // HELLOUTAU_SYNTHTOOLSUPPORT_FRCFORMAT_H
