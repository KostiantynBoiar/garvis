#include "audio_player.h"
#include "../audio/resample.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Playback state — one per device, driven by the miniaudio callback thread.
// We feed STEREO float32 to avoid any reliance on miniaudio's internal
// converter, which can fail silently on some WASAPI endpoints.
// ---------------------------------------------------------------------------

struct PlaybackState {
    const int16_t*      data   = nullptr;   // mono s16 source buffer
    size_t              total  = 0;         // total MONO frames
    std::atomic<size_t> cursor{0};
    float               volume = 1.0f;
    std::atomic<bool>   done{false};
    std::atomic<int>    cb_count{0};
    std::atomic<size_t> cb_frames_total{0};
    const char*         tag = "???";
};

// Callback delivers STEREO FLOAT frames — matches WASAPI internal format.
static void playback_callback(ma_device* device, void* output,
                               const void*, ma_uint32 frame_count)
{
    auto* ps  = static_cast<PlaybackState*>(device->pUserData);
    auto* out = static_cast<float*>(output);

    ps->cb_count.fetch_add(1, std::memory_order_relaxed);
    ps->cb_frames_total.fetch_add(frame_count, std::memory_order_relaxed);

    const size_t cur       = ps->cursor.load(std::memory_order_relaxed);
    const size_t remaining = (cur < ps->total) ? (ps->total - cur) : 0;
    const size_t to_copy   = std::min(static_cast<size_t>(frame_count), remaining);

    if (to_copy > 0) {
        const float scale = ps->volume / 32768.0f;
        for (size_t i = 0; i < to_copy; ++i) {
            const float sample = static_cast<float>(ps->data[cur + i]) * scale;
            out[i * 2 + 0] = sample;  // Left
            out[i * 2 + 1] = sample;  // Right
        }
        ps->cursor.store(cur + to_copy, std::memory_order_relaxed);
    }

    if (to_copy < static_cast<size_t>(frame_count)) {
        const size_t silence_frames = static_cast<size_t>(frame_count) - to_copy;
        std::memset(out + to_copy * 2, 0, silence_frames * 2 * sizeof(float));
        ps->done.store(true, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// Open one playback device (stereo f32 to match WASAPI natively).
// ---------------------------------------------------------------------------

static const char* backend_name(ma_backend b) {
    switch (b) {
        case ma_backend_wasapi:     return "WASAPI";
        case ma_backend_dsound:     return "DirectSound";
        case ma_backend_winmm:      return "WinMM";
        default:                    return "other";
    }
}

static bool open_device(ma_device& dev, PlaybackState& ps,
                         uint32_t sample_rate, const ma_device_id* device_id)
{
    ma_device_config cfg   = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format    = ma_format_f32;   // match WASAPI internal format
    cfg.playback.channels  = 2;               // match WASAPI internal channels
    cfg.playback.pDeviceID = const_cast<ma_device_id*>(device_id);
    cfg.sampleRate         = sample_rate;
    cfg.dataCallback       = playback_callback;
    cfg.pUserData          = &ps;
    cfg.performanceProfile = ma_performance_profile_low_latency;

    fprintf(stdout, "[audio_player:%s] ma_device_init (rate=%u, ch=2, f32, devID=%s)...\n",
            ps.tag, sample_rate, device_id ? "custom" : "default");

    ma_result res = ma_device_init(nullptr, &cfg, &dev);
    if (res != MA_SUCCESS) {
        fprintf(stderr, "[audio_player:%s] ma_device_init FAILED: %d\n", ps.tag, res);
        return false;
    }

    fprintf(stdout, "[audio_player:%s] OK — backend=%s dev=\"%s\" "
            "native=%uHz/%uch/%dfmt internal=%uHz/%uch/%dfmt\n",
            ps.tag,
            backend_name(dev.pContext->backend), dev.playback.name,
            dev.sampleRate, dev.playback.channels, dev.playback.format,
            dev.playback.internalSampleRate,
            dev.playback.internalChannels,
            dev.playback.internalFormat);

    res = ma_device_start(&dev);
    if (res != MA_SUCCESS) {
        fprintf(stderr, "[audio_player:%s] ma_device_start FAILED: %d\n", ps.tag, res);
        ma_device_uninit(&dev);
        return false;
    }
    fprintf(stdout, "[audio_player:%s] Device started.\n", ps.tag);
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static constexpr uint32_t OUTPUT_RATE = 48000;

void play_pcm(const std::vector<int16_t>& samples,
              uint32_t                    sample_rate,
              const ma_device_id*         speaker_id,
              float                       speaker_volume)
{
    if (samples.empty()) return;

    // Resample to 48 kHz to match the standard WASAPI shared-mode rate.
    std::vector<int16_t> resampled;
    const std::vector<int16_t>* pcm_ptr;
    if (sample_rate != OUTPUT_RATE) {
        resampled = resample_s16(samples, sample_rate, OUTPUT_RATE);
        pcm_ptr = &resampled;
    } else {
        pcm_ptr = &samples;
    }

    fprintf(stdout, "[audio_player] play_pcm: %zu mono frames @ %u Hz, spk_vol=%.1f\n",
            pcm_ptr->size(), OUTPUT_RATE, static_cast<double>(speaker_volume));

    PlaybackState ps_speaker;
    ps_speaker.data   = pcm_ptr->data();
    ps_speaker.total  = pcm_ptr->size();
    ps_speaker.volume = speaker_volume;
    ps_speaker.tag    = "speaker";

    ma_device dev_speaker{};
    bool speaker_ok = open_device(dev_speaker, ps_speaker, OUTPUT_RATE, speaker_id);

    while (speaker_ok && !ps_speaker.done.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (speaker_ok) {
        ma_device_stop(&dev_speaker);
        ma_device_uninit(&dev_speaker);
    }

    fprintf(stdout, "[audio_player] done — speaker: cb=%d frames=%zu\n",
            ps_speaker.cb_count.load(), ps_speaker.cb_frames_total.load());
}
