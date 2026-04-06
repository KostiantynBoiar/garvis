#pragma once
#include "miniaudio.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

class AudioMixer {
public:
    AudioMixer()  = default;
    ~AudioMixer() { shutdown(); }

    bool init(const ma_device_id* capture_id,
              const ma_device_id* playback_id,
              uint32_t sample_rate = 48000);

    void shutdown();

    void feed_tts(const std::vector<int16_t>& samples,
                  uint32_t sample_rate, float volume = 1.0f);

    void flush_tts();

    bool is_running() const { return m_running.load(std::memory_order_relaxed); }

private:
    static void duplex_callback(ma_device* device, void* output,
                                const void* input, ma_uint32 frame_count);

    ma_device          m_device{};
    bool               m_device_inited = false;
    uint32_t           m_sample_rate   = 48000;
    std::atomic<bool>  m_running{false};

    static constexpr size_t RING_CAPACITY = 48000 * 10;
    std::vector<float> m_ring;
    std::atomic<size_t> m_ring_write{0};
    std::atomic<size_t> m_ring_read{0};
    std::mutex          m_ring_mtx;
};
