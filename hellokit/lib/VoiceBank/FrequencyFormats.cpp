#include "FrequencyFormats.h"

#include <QtCore/QDir>
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

    QList<FrequencyFormat::AnalysisCache> FrequencyFormat::analysisCaches() const {
        return {};
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

    bool FrequencyFormats::invalidateCaches(const FrequencyFormat &format,
                                            const std::filesystem::path &directory,
                                            const QList<std::filesystem::path> &wavs,
                                            DiagnosticList &diagnostics) {
        return execute(directory, format.changesInvalidating(directory, wavs), diagnostics);
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
