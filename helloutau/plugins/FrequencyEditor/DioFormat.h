#ifndef HELLOUTAU_FREQUENCYEDITOR_DIOFORMAT_H
#define HELLOUTAU_FREQUENCYEDITOR_DIOFORMAT_H

#include <QtCore/QCoreApplication>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <FrequencyEditor/FrequencyEditorPluginGlobal.h>

namespace hello::daw {

    /// The frequency maps of world4utau, beside the audio file as a.dio, registered by the
    /// FrequencyEditor plugin. See docs/FrequencyMaps.md.
    class FREQUENCYEDITORPLUGIN_EXPORT DioFormat : public kit::FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::DioFormat)
    public:
        static constexpr char formatId[] = "dio";

        QString id() const override;
        QString name() const override;
        QStringList resamplerPatterns() const override;
        QList<bool> mapsExist(const std::filesystem::path &directory,
                              const QList<std::filesystem::path> &wavs) const override;
        std::optional<kit::FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                              kit::DiagnosticList &diagnostics) const override;
        std::optional<std::filesystem::path>
            sidecarNameOf(const std::filesystem::path &wav) const override;

        /// STAR and PLATINUM, the spectral envelope and the aperiodicity beside the map as
        /// a.star and a.platinum
        QList<AnalysisCache> analysisCaches() const override;

        bool canWrite() const override;

        /// Requires the frames of world4utau, one every 5.80498866213152 milliseconds. The signal
        /// length and the sample rate of a new map are those of the audio file.
        std::optional<QList<FileChange>>
            changesWriting(const std::filesystem::path &directory, const QList<MapWrite> &writes,
                           kit::DiagnosticList &diagnostics) const override;
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_DIOFORMAT_H
