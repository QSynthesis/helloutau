#include "FrequencyFormats.h"

#include <QtCore/QRegularExpression>

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

    std::optional<std::filesystem::path>
        FrequencyFormat::sidecarNameOf(const std::filesystem::path &wav) const {
        Q_UNUSED(wav);
        return std::nullopt;
    }

    std::optional<QList<FrequencyFormat::FileChange>>
        FrequencyFormat::changesFollowing(const std::filesystem::path &directory,
                                          const QList<AudioRename> &renames,
                                          DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &rename : renames) {
            const auto from = sidecarNameOf(rename.from);
            const auto to = sidecarNameOf(rename.to);
            if (!from || !to) {
                diagnostics.push_back(
                    {DiagnosticSeverity::Warning,
                     tr("The tables of the format %1 do not follow renamed audio files.")
                         .arg(name())});
                return std::nullopt;
            }
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / *from, error)) {
                changes.push_back(FileChange::renamed(*from, *to));
            }
        }
        return changes;
    }

    class FrequencyFormats::Impl {
    public:
        explicit Impl(FrequencyFormats *decl)
            : formats("frequency table format", [decl] { Q_EMIT decl->formatsChanged(); }) {
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

}
