#include "AudioOutput.h"

#include <atomic>
#include <cmath>
#include <thread>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {

        // The samples pulled from the generator at a time
        constexpr qsizetype Block = 1024;

        // The smallest ring buffer, in samples at the rate of the device
        constexpr qsizetype MinimumCapacity = 8192;

    }

    class StreamSource::Impl {
    public:
        Generator generator;
        int sourceRate = 0;
        int deviceRate = 0;

        // Mono samples at the rate of the device. The producer advances written, the audio
        // thread advances consumed; each index only grows.
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

        // Writes \a count samples, waiting for room as the device plays; false if stopped.
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
            std::vector<float> input(static_cast<size_t>(Block));
            std::vector<double> converted(static_cast<size_t>(Block));
            std::unique_ptr<r8b::CDSPResampler24> resampler;
            if (sourceRate != deviceRate) {
                resampler =
                    std::make_unique<r8b::CDSPResampler24>(sourceRate, deviceRate, int(Block));
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
                const qsizetype n = generator(input.data(), Block);
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
                    if (!convert(Block, expected)) {
                        return;
                    }
                }
            }
            ended.store(true);
        }
    };

    StreamSource::StreamSource(Generator generator, int sourceRate, int deviceRate, double buffer)
        : _impl(std::make_unique<Impl>()) {
        _impl->generator = std::move(generator);
        _impl->sourceRate = sourceRate;
        _impl->deviceRate = deviceRate;
        _impl->ring.resize(
            size_t(std::max<qsizetype>(MinimumCapacity, qsizetype(buffer * deviceRate))));
    }

    StreamSource::~StreamSource() {
        _impl->stopping.store(true);
        if (_impl->producer.joinable()) {
            _impl->producer.join();
        }
    }

    void StreamSource::start() {
        if (!_impl->producer.joinable()) {
            _impl->producer = std::thread([this] { _impl->produce(); });
        }
    }

    qsizetype StreamSource::read(float *out, qsizetype frames, int channels) noexcept {
        auto &impl = *_impl;
        // Read before the index, so that a producer that ends meanwhile is not taken for one
        // with nothing more to give.
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
        // The ring ran dry: at the end if the producer had finished before it was read.
        std::fill(out, out + (frames - count) * channels, 0.0f);
        if (ended) {
            impl.starved.store(false);
            return count;
        }
        impl.starved.store(true);
        return frames;
    }

    qint64 StreamSource::position() const {
        const auto played = double(_impl->consumed.load(std::memory_order_relaxed));
        return qint64(std::llround(played * _impl->sourceRate / _impl->deviceRate));
    }

    bool StreamSource::isStarved() const {
        return _impl->starved.load();
    }

}
