#include "AudioMixer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <stdcorelib/pimpl.h>

namespace hello::daw {

    class AudioMixer::Impl {
    public:
        using Decl = AudioMixer;
        enum State { Empty, Ready, Reading, Removing, Finished };
        static_assert(std::atomic<State>::is_always_lock_free);
        static constexpr qsizetype blockFrames = 256;

        struct Slot {
            std::atomic<State> state{Empty};
            SourceId id = 0;
            std::shared_ptr<AudioSource> source;
            std::shared_ptr<DeviceClock> clock;
        };

        Impl(int rate, int channels)
            : rate(std::max(1, rate)), channels(std::max(1, channels)),
              scratch(size_t(blockFrames * this->channels)) {
        }
        int rate;
        int channels;
        SourceId next = 0;
        std::array<Slot, capacity> slotList;
        std::vector<float> scratch;
    };

    AudioMixer::AudioMixer(int sampleRate, int channels)
        : _impl(std::make_unique<Impl>(sampleRate, channels)) {
    }

    AudioMixer::~AudioMixer() = default;

    std::optional<AudioMixer::SourceId> AudioMixer::add(std::shared_ptr<AudioSource> source) {
        stdc_impl_t;
        if (!source) {
            return std::nullopt;
        }
        collect();
        for (auto &slot : impl.slotList) {
            if (slot.state.load(std::memory_order_acquire) != Impl::Empty) {
                continue;
            }
            slot.id = ++impl.next;
            slot.source = std::move(source);
            slot.clock = std::make_shared<DeviceClock>(impl.rate);
            slot.state.store(Impl::Ready, std::memory_order_release);
            return slot.id;
        }
        return std::nullopt;
    }

    void AudioMixer::remove(SourceId id) {
        stdc_impl_t;
        for (auto &slot : impl.slotList) {
            if (slot.id != id) {
                continue;
            }
            auto state = slot.state.load(std::memory_order_acquire);
            while (state == Impl::Ready || state == Impl::Reading) {
                const auto next = state == Impl::Reading ? Impl::Removing : Impl::Finished;
                if (slot.state.compare_exchange_weak(state, next, std::memory_order_acq_rel)) {
                    break;
                }
            }
            return;
        }
    }

    bool AudioMixer::isFinished(SourceId id) const {
        stdc_impl_t;
        for (const auto &slot : impl.slotList) {
            if (slot.id == id) {
                const auto state = slot.state.load(std::memory_order_acquire);
                return state == Impl::Empty || state == Impl::Finished || state == Impl::Removing;
            }
        }
        return true;
    }

    std::shared_ptr<DeviceClock> AudioMixer::clock(SourceId id) const {
        stdc_impl_t;
        for (const auto &slot : impl.slotList) {
            if (slot.id == id) {
                return slot.clock;
            }
        }
        return {};
    }

    void AudioMixer::collect() {
        stdc_impl_t;
        for (auto &slot : impl.slotList) {
            if (slot.state.load(std::memory_order_acquire) == Impl::Finished) {
                slot.source.reset();
                slot.clock.reset();
                slot.state.store(Impl::Empty, std::memory_order_release);
            }
        }
    }

    void AudioMixer::render(float *out, qsizetype samples) noexcept {
        stdc_impl_t;
        std::fill_n(out, samples, 0.0f);
        const auto frames = samples / impl.channels;
        for (qsizetype offset = 0; offset < frames; offset += Impl::blockFrames) {
            const auto count = std::min(Impl::blockFrames, frames - offset);
            const auto now = DeviceClock::Clock::now();
            for (auto &slot : impl.slotList) {
                auto state = Impl::Ready;
                if (!slot.state.compare_exchange_strong(state, Impl::Reading,
                                                        std::memory_order_acq_rel)) {
                    continue;
                }
                const auto before = double(slot.source->position());
                const auto written = std::clamp<qsizetype>(
                    slot.source->read(impl.scratch.data(), count, impl.channels), 0, count);
                slot.clock->pulled(count, before, double(slot.source->position()), now);
                for (qsizetype i = 0; i < written * impl.channels; ++i) {
                    out[offset * impl.channels + i] += impl.scratch[size_t(i)];
                }
                state = Impl::Reading;
                if (!slot.state.compare_exchange_strong(
                        state, written < count ? Impl::Finished : Impl::Ready,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    // Removal was requested during read(). Reclamation occurs on the control
                    // thread.
                    slot.state.store(Impl::Finished, std::memory_order_release);
                }
            }
        }
        for (qsizetype i = 0; i < samples; ++i) {
            out[i] = std::isfinite(out[i]) ? std::clamp(out[i], -1.0f, 1.0f) : 0.0f;
        }
    }

}
