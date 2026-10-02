#include "BufferSource.h"

#include <algorithm>
#include <atomic>

#include <stdcorelib/pimpl.h>

namespace hello::daw {

    class BufferSource::Impl {
    public:
        using Decl = BufferSource;

        std::shared_ptr<const std::vector<float>> samples;
        int channels = 1;
        std::atomic<qsizetype> position = 0;
    };

    BufferSource::BufferSource(std::vector<float> samples, int channels)
        : BufferSource(std::make_shared<const std::vector<float>>(std::move(samples)), channels) {
    }

    BufferSource::BufferSource(std::shared_ptr<const std::vector<float>> samples, int channels,
                               qsizetype first)
        : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.samples = samples ? std::move(samples) : std::make_shared<const std::vector<float>>();
        impl.channels = std::max(1, channels);
        impl.position = std::clamp<qsizetype>(first, 0, frameCount());
    }

    BufferSource::~BufferSource() = default;

    qsizetype BufferSource::read(float *out, qsizetype frames, int channels) noexcept {
        stdc_impl_t;
        const int own = impl.channels;
        const auto position = impl.position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(frameCount() - position, 0, frames);
        const float *from = impl.samples->data() + position * own;
        for (qsizetype frame = 0; frame < count; ++frame) {
            for (int channel = 0; channel < channels; ++channel) {
                const int source = own == 1 ? 0 : channel;
                *out++ = source < own ? from[source] : 0.0f;
            }
            from += own;
        }
        impl.position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qsizetype BufferSource::frameCount() const {
        stdc_impl_t;
        return qsizetype(impl.samples->size()) / impl.channels;
    }

    qint64 BufferSource::position() const {
        stdc_impl_t;
        return impl.position.load(std::memory_order_relaxed);
    }

}
