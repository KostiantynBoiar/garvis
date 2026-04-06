#include "audio_mixer.h"
#include "resample.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

bool AudioMixer::init(const ma_device_id* capture_id,
                      const ma_device_id* playback_id,
                      uint32_t sample_rate)
{
    shutdown();

    m_sample_rate = sample_rate;
    m_ring.assign(RING_CAPACITY, 0.0f);
    m_ring_write.store(0, std::memory_order_relaxed);
    m_ring_read.store(0, std::memory_order_relaxed);

    ma_device_config cfg  = ma_device_config_init(ma_device_type_duplex);
    cfg.capture.format    = ma_format_f32;
    cfg.capture.channels  = 1;
    cfg.capture.pDeviceID = const_cast<ma_device_id*>(capture_id);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.playback.pDeviceID = const_cast<ma_device_id*>(playback_id);
    cfg.sampleRate        = sample_rate;
    cfg.dataCallback      = duplex_callback;
    cfg.pUserData         = this;
    cfg.performanceProfile = ma_performance_profile_low_latency;

    ma_result res = ma_device_init(nullptr, &cfg, &m_device);
    if (res != MA_SUCCESS) {
        fprintf(stderr, "[mixer] ma_device_init FAILED: %d\n", res);
        return false;
    }
    m_device_inited = true;

    fprintf(stdout, "[mixer] duplex device: capture=\"%s\"  playback=\"%s\"  rate=%u\n",
            m_device.capture.name, m_device.playback.name, sample_rate);

    res = ma_device_start(&m_device);
    if (res != MA_SUCCESS) {
        fprintf(stderr, "[mixer] ma_device_start FAILED: %d\n", res);
        ma_device_uninit(&m_device);
        m_device_inited = false;
        return false;
    }
    m_running.store(true, std::memory_order_release);
    fprintf(stdout, "[mixer] running.\n");
    return true;
}

void AudioMixer::shutdown()
{
    if (!m_device_inited) return;
    m_running.store(false, std::memory_order_release);
    ma_device_stop(&m_device);
    ma_device_uninit(&m_device);
    m_device_inited = false;
    fprintf(stdout, "[mixer] shutdown.\n");
}

void AudioMixer::feed_tts(const std::vector<int16_t>& samples,
                           uint32_t sample_rate, float volume)
{
    if (samples.empty() || !m_running.load(std::memory_order_relaxed)) return;

    std::vector<int16_t> resampled;
    const std::vector<int16_t>* pcm = &samples;
    if (sample_rate != m_sample_rate) {
        resampled = resample_s16(samples, sample_rate, m_sample_rate);
        pcm = &resampled;
    }

    const float scale = volume / 32768.0f;

    std::lock_guard<std::mutex> lock(m_ring_mtx);

    const size_t cap = m_ring.size();
    size_t w = m_ring_write.load(std::memory_order_relaxed);

    for (size_t i = 0; i < pcm->size(); ++i) {
        m_ring[w % cap] = static_cast<float>((*pcm)[i]) * scale;
        ++w;
    }
    m_ring_write.store(w, std::memory_order_release);
}

void AudioMixer::flush_tts()
{
    std::lock_guard<std::mutex> lock(m_ring_mtx);
    m_ring_read.store(m_ring_write.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
}

void AudioMixer::duplex_callback(ma_device* device, void* output,
                                  const void* input, ma_uint32 frame_count)
{
    auto* self      = static_cast<AudioMixer*>(device->pUserData);
    auto* out       = static_cast<float*>(output);
    const auto* in  = static_cast<const float*>(input);

    const size_t cap = self->m_ring.size();
    size_t r = self->m_ring_read.load(std::memory_order_acquire);
    const size_t w = self->m_ring_write.load(std::memory_order_acquire);
    const size_t available = w - r;

    for (ma_uint32 i = 0; i < frame_count; ++i) {
        float mic = in ? in[i] : 0.0f;

        float tts = 0.0f;
        if (i < available) {
            tts = self->m_ring[(r + i) % cap];
        }

        float mixed = mic + tts;
        out[i * 2 + 0] = mixed;
        out[i * 2 + 1] = mixed;
    }

    const size_t consumed = std::min(static_cast<size_t>(frame_count), available);
    self->m_ring_read.store(r + consumed, std::memory_order_release);
}
