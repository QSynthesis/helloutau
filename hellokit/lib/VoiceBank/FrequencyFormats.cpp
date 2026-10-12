#include "FrequencyFormats.h"

#include <QtCore/QRegularExpression>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

namespace hello::kit {

    FrequencyFormat::~FrequencyFormat() = default;

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
