#include "FrequencyFormatRegistry.h"

#include <vector>

#include <QtCore/QRegularExpression>

#include <stdcorelib/pimpl.h>

#include "BuiltinFrequencyFormats_p.h"
#include "FrequencyFormatPlugin.h"

namespace hello::kit {

    FrequencyFormat::~FrequencyFormat() = default;

    FrequencyFormatPlugin::~FrequencyFormatPlugin() = default;

    class FrequencyFormatRegistry::Impl {
    public:
        std::vector<std::unique_ptr<FrequencyFormat>> formats;
    };

    FrequencyFormatRegistry::FrequencyFormatRegistry() : _impl(std::make_unique<Impl>()) {
    }

    FrequencyFormatRegistry::~FrequencyFormatRegistry() = default;

    void FrequencyFormatRegistry::addBuiltinFormats() {
        for (auto &format : builtinFrequencyFormats()) {
            add(std::move(format));
        }
    }

    bool FrequencyFormatRegistry::add(std::unique_ptr<FrequencyFormat> format) {
        stdc_impl_t;
        if (!format || this->format(format->id())) {
            return false;
        }
        impl.formats.push_back(std::move(format));
        return true;
    }

    QList<FrequencyFormat *> FrequencyFormatRegistry::formats() const {
        stdc_impl_t;
        QList<FrequencyFormat *> result;
        for (const auto &format : impl.formats) {
            result.push_back(format.get());
        }
        return result;
    }

    FrequencyFormat *FrequencyFormatRegistry::format(const QString &id) const {
        stdc_impl_t;
        for (const auto &format : impl.formats) {
            if (format->id() == id) {
                return format.get();
            }
        }
        return nullptr;
    }

    FrequencyFormat *
        FrequencyFormatRegistry::formatForResampler(const std::filesystem::path &resampler) const {
        stdc_impl_t;
        const auto name = QString::fromStdU16String(resampler.filename().u16string());
        for (const auto &format : impl.formats) {
            for (const auto &pattern : format->resamplerPatterns()) {
                // The whole name, so that resampler*.exe does not match moresampler.exe
                const QRegularExpression expression(
                    QRegularExpression::wildcardToRegularExpression(pattern),
                    QRegularExpression::CaseInsensitiveOption);
                if (!name.isEmpty() && expression.match(name).hasMatch()) {
                    return format.get();
                }
            }
        }
        return format(QStringLiteral("frq"));
    }

}
