#pragma once

#include "miniaudio.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

struct VADConfig {
    float energy_threshold = 0.02f;
    float hangover_ms      = 900.0f;
    float pre_speech_ms    = 200.0f;
};

enum class CaptureState {
    Idle,
    Listening,
    Recording,
    Hangover,
    Done,
};

class AudioCapture {
public:
    AudioCapture()  = default;
    ~AudioCapture();

    bool init(uint32_t sample_rate = 16000, uint32_t channels = 1);
    bool init(uint32_t sample_rate, uint32_t channels, const ma_device_id* device_id);
    bool reinit(const ma_device_id* device_id);

    void shutdown();

    void start_listening();
    void stop_listening();

    void ptt_start();
    void ptt_stop();

    CaptureState state()           const;
    bool         is_speaking()     const;
    bool         utterance_ready() const;

    std::vector<float> consume_utterance();

    float    current_rms()  const;
    uint32_t sample_rate()  const { return m_sample_rate; }

    void set_vad_config(const VADConfig& cfg);

private:
    static void capture_callback(ma_device* device, void* output,
                                 const void* input, ma_uint32 frame_count);
    void  on_audio_frames(const float* frames, uint32_t count);
    float compute_rms(const float* frames, uint32_t count) const;

    ma_device    m_device{};
    bool         m_device_inited = false;

    uint32_t     m_sample_rate = 16000;
    uint32_t     m_channels    = 1;
    VADConfig    m_vad{};

    std::atomic<CaptureState> m_state{CaptureState::Idle};
    std::atomic<float>        m_current_rms{0.0f};

    mutable std::mutex   m_mutex;
    std::vector<float>   m_pre_buffer;
    std::vector<float>   m_record_buffer;
    std::vector<float>   m_ready_buffer;

    uint64_t m_hangover_samples   = 0;
    uint64_t m_hangover_counter   = 0;
    size_t   m_pre_speech_samples = 0;
};
