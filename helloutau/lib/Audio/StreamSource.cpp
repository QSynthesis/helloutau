#include "StreamSource.h"

#include <atomic>
#include <cmath>
#include <thread>

#include <stdcorelib/pimpl.h>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {

        // The samples pulled from the generator at a time
        constexpr qsizetype generatorBlock = 1024;

        // The smallest ring buffer, in samples at the rate of the device
        constexpr qsizetype minimumCapacity = 8192;

    }

    class StreamSource::Impl {
    public:
        using Decl = StreamSource;

        Generator generator;
        int sourceRate = 0;
        int deviceRate = 0;

        // Mono samples at the rate of the device. The producer advances written and the audio
        // thread advances consumed. Neither index decreases.
        std::vector<float> ring;
        std::atomic<qint64> written = 0;
        std::atomic<qint64> consumed = 0;

        std::atomic<bool> ended = false;
        std::atomic<bool> starved = false;
        std::atomic<bool> stopping = false;
        std::thread producer;

        qsizetype capacity() const {
            return qsizetype(ring.size());
        }

        qsizetype space() const {
            return capacity() - qsizetype(written.load(std::memory_order_relaxed) -
                                          consumed.load(std::memory_order_acquire));
        }

        // Writes \a count samples, waiting for space as the device plays. Returns false if
        // stopped.
        bool push(const double *samples, qsizetype count) {
            while (count > 0) {
                const qsizetype room = std::min(space(), count);
                if (room == 0) {
                    if (stopping.load()) {
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                qint64 at = written.load(std::memory_order_relaxed);
                for (qsizetype i = 0; i < room; ++i, ++at) {
                    ring[size_t(at % capacity())] = float(samples[i]);
                }
                written.store(at, std::memory_order_release);
                samples += room;
                count -= room;
            }
            return true;
        }

        void produce() {
            std::vector<float> input(static_cast<size_t>(generatorBlock));
            std::vector<double> converted(static_cast<size_t>(generatorBlock));
            std::unique_ptr<r8b::CDSPResampler24> resampler;
            if (sourceRate != deviceRate) {
                resampler = std::make_unique<r8b::CDSPResampler24>(sourceRate, deviceRate,
                                                                   int(generatorBlock));
            }
            qint64 pulled = 0;
            qint64 produced = 0;

            // Passes \a count samples of \a from through the conversion to the ring, at most
            // \a limit of the result, or all if negative.
            const auto convert = [&](qsizetype count, qint64 limit) {
                if (!resampler) {
                    produced += count;
                    return push(converted.data(), count);
                }
                double *output = nullptr;
                qsizetype made = resampler->process(converted.data(), int(count), output);
                if (limit >= 0) {
                    made = std::min<qsizetype>(made, limit - produced);
                }
                produced += made;
                return push(output, made);
            };

            while (!stopping.load()) {
                const qsizetype n = generator(input.data(), generatorBlock);
                if (n < 0) {
                    break;
                }
                if (n == 0) {
                    continue;
                }
                pulled += n;
                std::copy(input.begin(), input.begin() + n, converted.begin());
                if (!convert(n, -1)) {
                    return;
                }
            }

            // The filter still holds the end of the audio, which silence pushes out, up to the
            // length the whole would have after conversion.
            if (resampler && !stopping.load()) {
                const auto expected =
                    qint64(std::llround(double(pulled) * deviceRate / sourceRate));
                std::fill(converted.begin(), converted.end(), 0.0);
                while (produced < expected && !stopping.load()) {
                    if (!convert(generatorBlock, expected)) {
                        return;
                    }
                }
            }
            ended.store(true);
        }
    };

    StreamSource::StreamSource(Generator generator, int sourceRate, int deviceRate, double buffer)
        : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.generator = std::move(generator);
        impl.sourceRate = sourceRate;
        impl.deviceRate = deviceRate;
        impl.ring.resize(
            size_t(std::max<qsizetype>(minimumCapacity, qsizetype(buffer * deviceRate))));
    }

    StreamSource::~StreamSource() {
        stop();
    }

    void StreamSource::stop() {
        stdc_impl_t;
        impl.stopping.store(true);
        if (impl.producer.joinable()) {
            impl.producer.join();
        }
    }

    void StreamSource::start() {
        stdc_impl_t;
        if (!impl.producer.joinable()) {
            impl.producer = std::thread([this] {
                stdc_impl_t;
                impl.produce();
            });
        }
    }

    qsizetype StreamSource::read(float *out, qsizetype frames, int channels) noexcept {
        stdc_impl_t;
        // Loaded before the indices, so that a producer that ends after they are loaded is
        // not mistaken for one that has no samples left.
        const bool ended = impl.ended.load(std::memory_order_acquire);
        const qint64 from = impl.consumed.load(std::memory_order_relaxed);
        const qint64 available = impl.written.load(std::memory_order_acquire) - from;
        const qsizetype count = std::clamp<qsizetype>(available, 0, frames);
        for (qsizetype i = 0; i < count; ++i) {
            const float sample = impl.ring[size_t((from + i) % impl.capacity())];
            for (int channel = 0; channel < channels; ++channel) {
                *out++ = sample;
            }
        }
        impl.consumed.store(from + count, std::memory_order_release);
        if (count == frames) {
            impl.starved.store(false);
            return frames;
        }
        // The ring buffer is empty. The source ends if the producer had ended before the
        // indices were loaded.
        std::fill(out, out + (frames - count) * channels, 0.0f);
        if (ended) {
            impl.starved.store(false);
            return count;
        }
        impl.starved.store(true);
        return frames;
    }

    qint64 StreamSource::position() const noexcept {
        stdc_impl_t;
        const auto played = double(impl.consumed.load(std::memory_order_relaxed));
        return qint64(std::llround(played * impl.sourceRate / impl.deviceRate));
    }

    bool StreamSource::isStarved() const {
        stdc_impl_t;
        return impl.starved.load();
    }

}
