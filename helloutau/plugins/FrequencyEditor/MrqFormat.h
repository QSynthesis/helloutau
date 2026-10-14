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

        /// \c desc.mrq for every audio file
        std::optional<std::filesystem::path>
            mapFileNameOf(const std::filesystem::path &wav) const override;

        /// LLSM, the analysis of moresampler beside the audio file as a.wav.llsm
        QList<AnalysisCache> analysisCaches() const override;

        /// Writes the entry of each renamed audio file again under the new name. An entry under
        /// the new name is deleted, so that the entry written is the only one.
        std::optional<QList<FileChange>>
            changesRenaming(const std::filesystem::path &directory,
                            const QList<AudioRename> &renames,
                            kit::DiagnosticList &diagnostics) const override;

        /// Deletes every entry of each audio file regardless of the case of its name, and removes
        /// \c desc.mrq if no other entry remains.
        std::optional<QList<FileChange>>
            changesRemoving(const std::filesystem::path &directory,
                            const QList<std::filesystem::path> &wavs,
                            kit::DiagnosticList &diagnostics) const override;

        /// Reports \c desc.mrq as repairable if its entry count differs from the number of its
        /// complete entries, or bytes follow the last complete entry. Reports the LLSM file of an
        /// audio file without an entry, since moresampler does not write the entry while the file
        /// exists.
        QList<MapFileDamage> checkMapFiles(const std::filesystem::path &directory,
                                           const QList<std::filesystem::path> &wavs) const override;

        /// Records the number of complete entries in the header of \c desc.mrq and removes the
        /// bytes after the last complete entry.
        std::optional<QList<FileChange>>
            changesRepairing(const std::filesystem::path &directory,
                             const std::filesystem::path &path,
                             kit::DiagnosticList &diagnostics) const override;

        bool canWrite() const override;

        /// Writes each entry as \c mrq_write_entry() of the mrq library writes it, and creates
        /// \c desc.mrq if absent. A new entry has the sample rate of the audio file.
        std::optional<QList<FileChange>>
            changesWriting(const std::filesystem::path &directory, const QList<MapWrite> &writes,
                           kit::DiagnosticList &diagnostics) const override;

    private:
        // The complete entries of a file up to its end regardless of the entry count, and the
        // position after the last of them
        struct Scan {
            qint32 count = 0;
            QList<std::pair<QString, qsizetype>> entries;
            qsizetype end = 12;
        };

        // Returns the scan of bytes, or none if bytes has no header
        static std::optional<Scan> scan(const QByteArray &bytes);

        // Zeroes the name of each entry whose name, without case, is one of names, and returns
        // whether any name was zeroed
        static bool zeroNames(QByteArray &bytes, const QSet<QString> &names);

        // The name and the position of the data of each entry, up to the first incomplete entry
        static QList<std::pair<QString, qsizetype>> entriesOf(const QByteArray &bytes);

        // The name and the position of the data of the entry of wav, or none
        static std::optional<std::pair<QString, qsizetype>> find(const QByteArray &bytes,
                                                                 const std::filesystem::path &wav);
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_MRQFORMAT_H
