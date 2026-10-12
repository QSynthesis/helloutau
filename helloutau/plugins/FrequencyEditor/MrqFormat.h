#ifndef HELLOUTAU_FREQUENCYEDITOR_MRQFORMAT_H
#define HELLOUTAU_FREQUENCYEDITOR_MRQFORMAT_H

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QSet>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <FrequencyEditor/FrequencyEditorPluginGlobal.h>

namespace hello::daw {

    /// The frequency maps of moresampler, an entry named after the audio file in \c desc.mrq of
    /// its folder, registered by the FrequencyEditor plugin. See docs/FrequencyMaps.md.
    class FREQUENCYEDITORPLUGIN_EXPORT MrqFormat : public kit::FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::MrqFormat)
    public:
        static constexpr char formatId[] = "mrq";

        QString id() const override;
        QString name() const override;
        QStringList resamplerPatterns() const override;

        /// Reads \c desc.mrq once, and looks each name up as read() looks it up.
        QList<bool> mapsExist(const std::filesystem::path &directory,
                              const QList<std::filesystem::path> &wavs) const override;

        std::optional<kit::FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                              kit::DiagnosticList &diagnostics) const override;

        /// LLSM, the analysis of moresampler beside the audio file as a.wav.llsm
        QList<AnalysisCache> analysisCaches() const override;

        /// Writes the entry of each renamed audio file again under the new name. An entry under
        /// the new name is deleted, so that the entry written is the only one.
        std::optional<QList<FileChange>>
            changesRenaming(const std::filesystem::path &directory,
                            const QList<AudioRename> &renames,
                            kit::DiagnosticList &diagnostics) const override;

        /// Deletes every entry of each audio file, whatever the case of its name.
        std::optional<QList<FileChange>>
            changesRemoving(const std::filesystem::path &directory,
                            const QList<std::filesystem::path> &wavs,
                            kit::DiagnosticList &diagnostics) const override;

    private:
        // Zeroes the name of each entry whose name, without case, is one of names, and returns
        // whether any name was zeroed
        static bool zeroNames(QByteArray &bytes, const QSet<QString> &names);

        // The name and the position of the data of each entry, up to the first entry that is
        // cut short
        static QList<std::pair<QString, qsizetype>> entriesOf(const QByteArray &bytes);

        // The position of the data of the entry of wav, or none
        static std::optional<qsizetype> find(const QByteArray &bytes,
                                             const std::filesystem::path &wav);
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_MRQFORMAT_H
