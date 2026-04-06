#pragma once
#include "../tts/tts.h"
#include "../tts/audio_player.h"
#include "../audio/audio_mixer.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Three-stage pipeline: text_queue -> synth_thread -> pcm_queue -> play_thread.
// Usage:
//   streamer.start(&tts, &mixer, speaker_id);
//   streamer.feed("First sentence.");   // from any thread
//   streamer.feed("Second sentence.");
//   streamer.finish();                 // blocks until all audio has played
class TTSStreamer {
public:
    TTSStreamer() = default;
    ~TTSStreamer() { abort(); }

    // mixer          — pointer to a running AudioMixer (mic+TTS -> VB-CABLE).
    //                  nullptr disables loopback mixing.
    // speaker_id     — nullptr uses the OS default output device.
    // speaker_volume — linear gain for local speaker output (default 3.0 = 3x boost).
    // loopback_volume— linear gain for the mixer's TTS feed (1.0 = unity).
    void start(const TextToSpeech* tts,
               AudioMixer*         mixer,
               const ma_device_id* speaker_id      = nullptr,
               float               speaker_volume  = 3.0f,
               float               loopback_volume = 1.0f);

    // Enqueue a text chunk for synthesis. Thread-safe.
    void feed(const std::string& chunk);

    // Signal that no more chunks are coming; blocks until playback drains.
    void finish();

    // Immediately stop both threads and discard remaining work.
    void abort();

    // True while either thread is still running.
    bool is_active() const;

private:
    void synth_loop();
    void play_loop();

    const TextToSpeech* m_tts             = nullptr;
    AudioMixer*         m_mixer           = nullptr;
    const ma_device_id* m_speaker_id      = nullptr;
    float               m_speaker_volume  = 3.0f;
    float               m_loopback_volume = 1.0f;

    std::queue<std::string>          m_text_q;
    std::mutex                       m_text_mtx;
    std::condition_variable          m_text_cv;

    std::queue<std::vector<int16_t>> m_pcm_q;
    std::mutex                       m_pcm_mtx;
    std::condition_variable          m_pcm_cv;

    // m_feeding: true while caller can still call feed()
    // m_synth_done: synth_thread finished pushing all PCM
    std::atomic<bool> m_feeding{false};
    std::atomic<bool> m_synth_done{false};
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_active{false};

    std::thread m_synth_thread;
    std::thread m_play_thread;
};
