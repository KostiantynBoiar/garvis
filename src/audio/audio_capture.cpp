#include "audio_capture.h"

#include <algorithm>
#include <cmath>
#include <cstring>

AudioCapture::~AudioCapture() { shutdown(); }

bool AudioCapture::init(uint32_t sample_rate, uint32_t channels) {
    return init(sample_rate, channels, nullptr);
}

bool AudioCapture::init(uint32_t sample_rate, uint32_t channels,
                        const ma_device_id* device_id) {
    if (m_device_inited) shutdown();

    m_sample_rate = sample_rate;
    m_channels    = channels;

    m_hangover_samples   = static_cast<uint64_t>(
        (m_vad.hangover_ms / 1000.0f) * m_sample_rate);
    m_pre_speech_samples = static_cast<size_t>(
        (m_vad.pre_speech_ms / 1000.0f) * m_sample_rate);

    m_pre_buffer.clear();
    m_pre_buffer.reserve(m_pre_speech_samples);

    ma_device_config config      = ma_device_config_init(ma_device_type_capture);
    config.capture.format        = ma_format_f32;
    config.capture.channels      = m_channels;
    config.capture.pDeviceID     = const_cast<ma_device_id*>(device_id);
    config.sampleRate            = m_sample_rate;
    config.dataCallback          = &AudioCapture::capture_callback;
    config.pUserData             = this;
    config.periodSizeInFrames    = m_sample_rate / 33;

    if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) return false;

    m_device_inited = true;
    m_state.store(CaptureState::Idle);
    return true;
}

bool AudioCapture::reinit(const ma_device_id* device_id) {
    const bool was_listening = (m_state.load() != CaptureState::Idle);
    shutdown();
    if (!init(m_sample_rate, m_channels, device_id)) return false;
    if (was_listening) start_listening();
    return true;
}

void AudioCapture::shutdown() {
    if (m_device_inited) {
        ma_device_uninit(&m_device);
        m_device_inited = false;
    }
    m_state.store(CaptureState::Idle);
}

void AudioCapture::start_listening() {
    if (!m_device_inited) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pre_buffer.clear();
        m_record_buffer.clear();
        m_ready_buffer.clear();
        m_hangover_counter = 0;
    }
    m_state.store(CaptureState::Listening);
    ma_device_start(&m_device);
}

void AudioCapture::stop_listening() {
    if (!m_device_inited) return;
    ma_device_stop(&m_device);
    m_state.store(CaptureState::Idle);
}

void AudioCapture::ptt_start() {
    if (!m_device_inited) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_record_buffer.clear();
        m_pre_buffer.clear();
        m_hangover_counter = 0;
    }
    m_state.store(CaptureState::Recording);
    ma_device_start(&m_device);
}

void AudioCapture::ptt_stop() {
    if (m_state.load() != CaptureState::Recording) return;
    ma_device_stop(&m_device);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready_buffer = std::move(m_record_buffer);
    m_record_buffer.clear();
    m_state.store(CaptureState::Done);
}

CaptureState AudioCapture::state()           const { return m_state.load(); }
bool         AudioCapture::is_speaking()     const {
    auto s = m_state.load();
    return s == CaptureState::Recording || s == CaptureState::Hangover;
}
bool AudioCapture::utterance_ready() const { return m_state.load() == CaptureState::Done; }
float AudioCapture::current_rms()    const { return m_current_rms.load(); }

std::vector<float> AudioCapture::consume_utterance() {
    std::vector<float> out;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out = std::move(m_ready_buffer);
        m_ready_buffer.clear();
        m_record_buffer.clear();
        m_pre_buffer.clear();
        m_hangover_counter = 0;
    }
    m_state.store(CaptureState::Idle);
    return out;
}

void AudioCapture::set_vad_config(const VADConfig& cfg) {
    m_vad = cfg;
    m_hangover_samples   = static_cast<uint64_t>(
        (m_vad.hangover_ms / 1000.0f) * m_sample_rate);
    m_pre_speech_samples = static_cast<size_t>(
        (m_vad.pre_speech_ms / 1000.0f) * m_sample_rate);
}

void AudioCapture::capture_callback(ma_device* device, void*, const void* input,
                                     ma_uint32 frame_count) {
    auto* self = static_cast<AudioCapture*>(device->pUserData);
    if (!input || frame_count == 0) return;
    self->on_audio_frames(static_cast<const float*>(input), frame_count);
}

void AudioCapture::on_audio_frames(const float* frames, uint32_t count) {
    const float rms = compute_rms(frames, count);
    m_current_rms.store(rms);

    const CaptureState current = m_state.load();
    if (current == CaptureState::Idle || current == CaptureState::Done) return;

    const bool speech = (rms > m_vad.energy_threshold);
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (current) {
    case CaptureState::Listening:
        if (m_pre_speech_samples > 0) {
            m_pre_buffer.insert(m_pre_buffer.end(), frames, frames + count);
            if (m_pre_buffer.size() > m_pre_speech_samples) {
                const size_t excess = m_pre_buffer.size() - m_pre_speech_samples;
                m_pre_buffer.erase(m_pre_buffer.begin(),
                                   m_pre_buffer.begin() + static_cast<ptrdiff_t>(excess));
            }
        }
        if (speech) {
            m_record_buffer.clear();
            if (!m_pre_buffer.empty()) {
                m_record_buffer = m_pre_buffer;
                m_pre_buffer.clear();
            }
            m_record_buffer.insert(m_record_buffer.end(), frames, frames + count);
            m_hangover_counter = 0;
            m_state.store(CaptureState::Recording);
        }
        break;

    case CaptureState::Recording:
        m_record_buffer.insert(m_record_buffer.end(), frames, frames + count);
        if (!speech) {
            m_hangover_counter = count;
            m_state.store(CaptureState::Hangover);
        }
        break;

    case CaptureState::Hangover:
        m_record_buffer.insert(m_record_buffer.end(), frames, frames + count);
        m_hangover_counter += count;
        if (speech) {
            m_hangover_counter = 0;
            m_state.store(CaptureState::Recording);
        } else if (m_hangover_counter >= m_hangover_samples) {
            m_ready_buffer = std::move(m_record_buffer);
            m_record_buffer.clear();
            m_state.store(CaptureState::Done);
        }
        break;

    default:
        break;
    }
}

float AudioCapture::compute_rms(const float* frames, uint32_t count) const {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; ++i) sum += frames[i] * frames[i];
    return std::sqrt(sum / static_cast<float>(count));
}
