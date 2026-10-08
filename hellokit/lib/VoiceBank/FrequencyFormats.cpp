#include "FrequencyFormats.h"

#include <QtCore/QRegularExpression>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/RegistryInstanceList.h>

namespace hello::kit {

    FrequencyFormat::~FrequencyFormat() = default;

    class FrequencyFormats::Impl {
    public:
        using Instances = RegistryInstanceList<FrequencyFormatRegistry>;

        explicit Impl(FrequencyFormats *decl)
            : instances(
                  registry,
                  [decl](Instances::Item &item) {
                      if (!item.instance) {
                          return false;
                      }
                      const auto name = QString::fromStdString(item.entry->name());
                      if (item.instance->id() != name) {
                          qWarning("The frequency table format \"%s\" is registered under the "
                                   "name \"%s\".",
                                   qUtf8Printable(item.instance->id()), qUtf8Printable(name));
                          return false;
                      }
                      Q_EMIT decl->formatsChanged();
                      return true;
                  },
                  [decl](Instances::Item &) { Q_EMIT decl->formatsChanged(); }) {
        }

        // The instances are declared after their registry, so that they are destroyed first.
        mutable FrequencyFormatRegistry registry;
        Instances instances;
    };

    FrequencyFormats::FrequencyFormats(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    FrequencyFormats::~FrequencyFormats() = default;

    FrequencyFormatRegistry &FrequencyFormats::registry() const {
        stdc_impl_t;
        return impl.registry;
    }

    QList<FrequencyFormat *> FrequencyFormats::formats() const {
        stdc_impl_t;
        QList<FrequencyFormat *> result;
        for (const auto &item : impl.instances.items()) {
            result.push_back(item.instance.get());
        }
        return result;
    }

    FrequencyFormat *FrequencyFormats::format(const QString &id) const {
        for (const auto format : formats()) {
            if (format->id() == id) {
                return format;
            }
        }
        return nullptr;
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
