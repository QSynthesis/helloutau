#include "FrequencyFormatRegistry.h"

#include <QtCore/QRegularExpression>

#include <stdcorelib/pimpl.h>

#include "FrequencyFormatRegistrations_p.h"

namespace hello::kit {

    FrequencyFormat::~FrequencyFormat() = default;

    class FrequencyFormatRegistry::Impl : public FrequencyFormatRegistrations::Listener {
    public:
        explicit Impl(FrequencyFormatRegistry *registry) : registry(registry) {
        }

        FrequencyFormatRegistry *registry;

        void formatAdded(FrequencyFormat *format) override {
            Q_UNUSED(format);
            Q_EMIT registry->formatsChanged();
        }

        void formatRemoved(FrequencyFormat *format) override {
            Q_UNUSED(format);
            Q_EMIT registry->formatsChanged();
        }
    };

    FrequencyFormatRegistry::FrequencyFormatRegistry(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        FrequencyFormatRegistrations::instance().addListener(&impl);
    }

    FrequencyFormatRegistry::~FrequencyFormatRegistry() {
        stdc_impl_t;
        FrequencyFormatRegistrations::instance().removeListener(&impl);
    }

    // Of the formats of one ID, the first registered is used, and the next one registered once
    // it goes.
    QList<FrequencyFormat *> FrequencyFormatRegistry::formats() const {
        QList<FrequencyFormat *> result;
        for (const auto format : FrequencyFormatRegistrations::instance().formats()) {
            const auto id = format->id();
            if (std::none_of(result.begin(), result.end(),
                             [&id](const FrequencyFormat *other) { return other->id() == id; })) {
                result.push_back(format);
            }
        }
        return result;
    }

    FrequencyFormat *FrequencyFormatRegistry::format(const QString &id) const {
        for (const auto format : formats()) {
            if (format->id() == id) {
                return format;
            }
        }
        return nullptr;
    }

    FrequencyFormat *
        FrequencyFormatRegistry::formatForResampler(const std::filesystem::path &resampler) const {
        const auto name = QString::fromStdU16String(resampler.filename().u16string());
        const auto all = formats();
        for (auto it = all.crbegin(); it != all.crend(); ++it) {
            for (const auto &pattern : (*it)->resamplerPatterns()) {
                // The whole name, so that resampler*.exe does not match moresampler.exe
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
