#include "FrequencyFormats.h"

#include <cmath>
#include <limits>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

namespace hello::kit {

    FrequencyFormat::~FrequencyFormat() = default;

    FrequencyFormat::FileChange FrequencyFormat::FileChange::renamed(std::filesystem::path from,
                                                                     std::filesystem::path to) {
        FileChange change;
        change.kind = Rename;
        change.path = std::move(from);
        change.to = std::move(to);
        return change;
    }

    FrequencyFormat::FileChange FrequencyFormat::FileChange::rewritten(std::filesystem::path path,
                                                                       QByteArray contents) {
        FileChange change;
        change.kind = Rewrite;
        change.path = std::move(path);
        change.contents = std::move(contents);
        return change;
    }

    FrequencyFormat::FileChange FrequencyFormat::FileChange::removed(std::filesystem::path path) {
        FileChange change;
        change.kind = Remove;
        change.path = std::move(path);
        return change;
    }

    std::filesystem::path
        FrequencyFormat::AnalysisCache::fileNameOf(const std::filesystem::path &wav) const {
        auto name = wav.filename();
        if (naming == AppendedSuffix) {
            return name += suffix.toStdU16String();
        }
        return name.replace_extension(suffix.toStdU16String());
    }

    std::optional<std::filesystem::path>
        FrequencyFormat::sidecarNameOf(const std::filesystem::path &wav) const {
        Q_UNUSED(wav);
        return std::nullopt;
    }

    std::optional<std::filesystem::path>
        FrequencyFormat::mapFileNameOf(const std::filesystem::path &wav) const {
        return sidecarNameOf(wav);
    }

    // resampler.exe of UTAU 0.4.19 writes a hop of 256 at 22050, 44100, 48000 and 96000 Hz,
    // measured on 2026-10-14 (docs/FrequencyMaps.md).
    double FrequencyFormat::hopOf(int sampleRate) const {
        Q_UNUSED(sampleRate);
        return 256;
    }

    QList<FrequencyFormat::AnalysisCache> FrequencyFormat::analysisCaches() const {
        return {};
    }

    std::optional<QByteArray> FrequencyFormat::contentsOf(const std::filesystem::path &path,
                                                          DiagnosticList &diagnostics) {
        const auto text = QString::fromStdU16String(path.u16string());
        QFile file(text);
        if (!file.open(QIODevice::ReadOnly)) {
            diagnostics.push_back(
                {DiagnosticSeverity::Error, tr("\"%1\" could not be read.").arg(text)});
            return std::nullopt;
        }
        return file.readAll();
    }

    std::optional<FrequencyMap> FrequencyFormat::damaged(const std::filesystem::path &path,
                                                         DiagnosticList &diagnostics) {
        diagnostics.push_back({DiagnosticSeverity::Error,
                               tr("\"%1\" is no frequency map of its format, or it is incomplete.")
                                   .arg(QString::fromStdU16String(path.u16string()))});
        return std::nullopt;
    }

    QList<bool> FrequencyFormat::sidecarsExist(const std::filesystem::path &directory,
                                               const QList<std::filesystem::path> &wavs) const {
        QList<bool> found;
        for (const auto &wav : wavs) {
            const auto sidecar = sidecarNameOf(wav);
            std::error_code error;
            found.push_back(sidecar &&
                            std::filesystem::is_regular_file(directory / *sidecar, error));
        }
        return found;
    }

    std::nullopt_t FrequencyFormat::unwritable(const std::filesystem::path &wav,
                                               const QString &reason, DiagnosticList &diagnostics) {
        diagnostics.push_back({DiagnosticSeverity::Error,
                               tr("The frequency map of \"%1\" cannot be written. %2")
                                   .arg(QString::fromStdU16String(wav.u16string()), reason)});
        return std::nullopt;
    }

    std::optional<qint32> FrequencyFormat::integralHop(const MapWrite &write,
                                                       DiagnosticList &diagnostics) {
        const auto hop = write.map.hop;
        if (hop >= 1 && hop <= std::numeric_limits<qint32>::max() && std::floor(hop) == hop) {
            return qint32(hop);
        }
        return unwritable(
            write.wav,
            tr("The format records the distance between the frames in whole samples, and the "
               "frames of the map are %1 samples apart.")
                .arg(hop),
            diagnostics);
    }

    bool FrequencyFormat::canWrite() const {
        return false;
    }

    std::optional<QList<FrequencyFormat::FileChange>>
        FrequencyFormat::changesWriting(const std::filesystem::path &directory,
                                        const QList<MapWrite> &writes,
                                        DiagnosticList &diagnostics) const {
        Q_UNUSED(directory);
        Q_UNUSED(writes);
        diagnostics.push_back({DiagnosticSeverity::Error,
                               tr("The format %1 does not support writing maps.").arg(name())});
        return std::nullopt;
    }

    QList<FrequencyFormat::MapFileDamage>
        FrequencyFormat::checkMapFiles(const std::filesystem::path &directory,
                                       const QList<std::filesystem::path> &wavs) const {
        Q_UNUSED(directory);
        Q_UNUSED(wavs);
        return {};
    }

    std::optional<QList<FrequencyFormat::FileChange>>
        FrequencyFormat::changesRepairing(const std::filesystem::path &directory,
                                          const std::filesystem::path &path,
                                          DiagnosticList &diagnostics) const {
        Q_UNUSED(directory);
        diagnostics.push_back({DiagnosticSeverity::Error,
                               tr("The format %1 does not support repairing \"%2\".")
                                   .arg(name(), QString::fromStdU16String(path.u16string()))});
        return std::nullopt;
    }

    QList<FrequencyFormat::FileChange>
        FrequencyFormat::changesInvalidating(const std::filesystem::path &directory,
                                             const QList<std::filesystem::path> &wavs) const {
        QList<FileChange> changes;
        const auto caches = analysisCaches();
        for (const auto &wav : wavs) {
            for (const auto &cache : caches) {
                const auto name = cache.fileNameOf(wav);
                std::error_code error;
                if (std::filesystem::is_regular_file(directory / name, error)) {
                    changes.push_back(FileChange::removed(name));
                }
            }
        }
        return changes;
    }

    QList<FrequencyFormat::FileChange>
        FrequencyFormat::cacheRenames(const std::filesystem::path &directory,
                                      const QList<AudioRename> &renames) const {
        QList<FileChange> changes;
        const auto caches = analysisCaches();
        for (const auto &rename : renames) {
            for (const auto &cache : caches) {
                const auto from = cache.fileNameOf(rename.from);
                std::error_code error;
                if (std::filesystem::is_regular_file(directory / from, error)) {
                    changes.push_back(FileChange::renamed(from, cache.fileNameOf(rename.to)));
                }
            }
        }
        return changes;
    }

    std::optional<QList<FrequencyFormat::FileChange>>
        FrequencyFormat::changesRenaming(const std::filesystem::path &directory,
                                         const QList<AudioRename> &renames,
                                         DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &rename : renames) {
            const auto from = sidecarNameOf(rename.from);
            const auto to = sidecarNameOf(rename.to);
            if (!from || !to) {
                diagnostics.push_back(
                    {DiagnosticSeverity::Warning,
                     tr("The maps of the format %1 cannot be renamed with their audio files.")
                         .arg(name())});
                return std::nullopt;
            }
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / *from, error)) {
                changes.push_back(FileChange::renamed(*from, *to));
            }
        }
        return changes + cacheRenames(directory, renames);
    }

    std::optional<QList<FrequencyFormat::FileChange>>
        FrequencyFormat::changesRemoving(const std::filesystem::path &directory,
                                         const QList<std::filesystem::path> &wavs,
                                         DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &wav : wavs) {
            const auto sidecar = sidecarNameOf(wav);
            if (!sidecar) {
                diagnostics.push_back(
                    {DiagnosticSeverity::Warning,
                     tr("The maps of the format %1 cannot be removed.").arg(name())});
                return std::nullopt;
            }
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / *sidecar, error)) {
                changes.push_back(FileChange::removed(*sidecar));
            }
        }
        return changes;
    }

    class FrequencyFormats::Impl {
    public:
        explicit Impl(FrequencyFormats *decl)
            : formats("frequency map format", [decl] { Q_EMIT decl->formatsChanged(); }) {
        }

        OrderedObjectRegistryKeeper<FrequencyFormat> formats;
    };

    FrequencyFormats::FrequencyFormats(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    FrequencyFormats::~FrequencyFormats() = default;

    FrequencyFormatRegistry &FrequencyFormats::registry() const {
        stdc_impl_t;
        return impl.formats.registry();
    }

    QList<FrequencyFormat *> FrequencyFormats::formats() const {
        stdc_impl_t;
        return impl.formats.instances();
    }

    FrequencyFormat *FrequencyFormats::format(const QString &id) const {
        stdc_impl_t;
        return impl.formats.instance(id);
    }

    FrequencyFormat *
        FrequencyFormats::formatForResampler(const std::filesystem::path &resampler) const {
        const auto name = QString::fromStdU16String(resampler.filename().u16string());
        const auto all = formats();
        for (auto it = all.crbegin(); it != all.crend(); ++it) {
            for (const auto &pattern : (*it)->resamplerPatterns()) {
                // Matches the whole name, so that resampler*.exe does not match moresampler.exe.
                const QRegularExpression expression(
                    QRegularExpression::wildcardToRegularExpression(pattern),
                    QRegularExpression::CaseInsensitiveOption);
                if (!name.isEmpty() && expression.match(name).hasMatch()) {
                    return *it;
                }
            }
        }
        return format(QStringLiteral("frq"));
    }

    bool FrequencyFormats::removeMaps(const FrequencyFormat &format,
                                      const std::filesystem::path &directory,
                                      const QList<std::filesystem::path> &wavs,
                                      DiagnosticList &diagnostics) {
        const auto changes = format.changesRemoving(directory, wavs, diagnostics);
        if (!changes) {
            return false;
        }
        return execute(directory, *changes + format.changesInvalidating(directory, wavs),
                       diagnostics);
    }

    bool FrequencyFormats::writeMaps(const FrequencyFormat &format,
                                     const std::filesystem::path &directory,
                                     const QList<FrequencyFormat::MapWrite> &writes,
                                     bool invalidatingCaches, DiagnosticList &diagnostics) {
        auto changes = format.changesWriting(directory, writes, diagnostics);
        if (!changes) {
            return false;
        }
        if (invalidatingCaches) {
            QList<std::filesystem::path> wavs;
            for (const auto &write : writes) {
                wavs.push_back(write.wav);
            }
            *changes += format.changesInvalidating(directory, wavs);
        }
        return execute(directory, *changes, diagnostics);
    }

    bool FrequencyFormats::invalidateCaches(const FrequencyFormat &format,
                                            const std::filesystem::path &directory,
                                            const QList<std::filesystem::path> &wavs,
                                            DiagnosticList &diagnostics) {
        return execute(directory, format.changesInvalidating(directory, wavs), diagnostics);
    }

    QList<FrequencyFormats::MapFile>
        FrequencyFormats::mapFilesOf(const FrequencyFormat &format,
                                     const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs) {
        QList<MapFile> files;
        const auto exists = [&directory](const std::filesystem::path &name) {
            std::error_code error;
            return std::filesystem::is_regular_file(directory / name, error);
        };
        // A file that holds the maps of several audio files, such as desc.mrq, is listed once,
        // and is listed even if missing. A missing sidecar file only indicates a missing map.
        for (const auto &wav : wavs) {
            const auto name = format.mapFileNameOf(wav);
            if (!name) {
                continue;
            }
            const bool present = exists(*name);
            if (!present && format.sidecarNameOf(wav) == name) {
                continue;
            }
            const auto found = std::find_if(files.begin(), files.end(),
                                            [&name](const MapFile &file) { return file.path == *name; });
            if (found != files.end()) {
                found->wavs.push_back(wav);
            } else {
                files.push_back({*name, -1, {wav}, std::nullopt, present});
            }
        }
        const auto caches = format.analysisCaches();
        for (int cache = 0; cache < caches.size(); ++cache) {
            for (const auto &wav : wavs) {
                const auto name = caches.at(cache).fileNameOf(wav);
                if (exists(name)) {
                    files.push_back({name, cache, {wav}, std::nullopt});
                }
            }
        }
        for (const auto &damage : format.checkMapFiles(directory, wavs)) {
            for (auto &file : files) {
                if (file.path == damage.path) {
                    file.damage = damage;
                }
            }
        }
        return files;
    }

    QList<std::filesystem::path>
        FrequencyFormats::repairMapFiles(const FrequencyFormat &format,
                                         const std::filesystem::path &directory,
                                         const QList<MapFile> &files,
                                         DiagnosticList &diagnostics) {
        QList<std::filesystem::path> again;
        const auto generateAgain = [&again](const std::filesystem::path &wav) {
            if (!again.contains(wav)) {
                again.push_back(wav);
            }
        };
        for (const auto &file : files) {
            if (!file.exists) {
                continue;
            }
            if (file.damage && file.damage->repairable) {
                const auto changes = format.changesRepairing(directory, file.path, diagnostics);
                if (changes && execute(directory, *changes, diagnostics)) {
                    // A repair may drop an incomplete map.
                    const auto found = format.mapsExist(directory, file.wavs);
                    for (qsizetype i = 0; i < file.wavs.size(); ++i) {
                        if (!found.value(i)) {
                            generateAgain(file.wavs.at(i));
                        }
                    }
                }
                continue;
            }
            QList<FrequencyFormat::FileChange> changes{FrequencyFormat::FileChange::removed(file.path)};
            if (file.cache < 0) {
                changes += format.changesInvalidating(directory, file.wavs);
            }
            if (execute(directory, changes, diagnostics)) {
                for (const auto &wav : file.wavs) {
                    generateAgain(wav);
                }
            }
        }
        return again;
    }

    bool FrequencyFormats::execute(const std::filesystem::path &directory,
                                   const QList<FrequencyFormat::FileChange> &changes,
                                   DiagnosticList &diagnostics) {
        namespace fs = std::filesystem;
        const auto fail = [&diagnostics](const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
            return false;
        };
        bool done = true;
        for (const auto &change : changes) {
            const auto path = directory / change.path;
            const auto text = QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
            std::error_code error;
            switch (change.kind) {
                case FrequencyFormat::FileChange::Remove:
                    fs::remove(path, error);
                    if (error) {
                        done = fail(tr("\"%1\" cannot be removed.").arg(text));
                    }
                    break;
                case FrequencyFormat::FileChange::Rename:
                    fs::rename(path, directory / change.to, error);
                    if (error) {
                        done = fail(tr("\"%1\" cannot be renamed.").arg(text));
                    }
                    break;
                case FrequencyFormat::FileChange::Rewrite: {
                    QSaveFile file(text);
                    if (!file.open(QIODevice::WriteOnly) ||
                        file.write(change.contents) != change.contents.size() || !file.commit()) {
                        done = fail(tr("\"%1\" cannot be written.").arg(text));
                    }
                    break;
                }
            }
        }
        return done;
    }

}
