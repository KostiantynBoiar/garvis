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

class TTSStreamer {
public:
    TTSStreamer() = default;
    ~TTSStreamer() { abort(); }

    void start(const TextToSpeech* tts,
               AudioMixer*         mixer,
               const ma_device_id* speaker_id      = nullptr,
               float               speaker_volume  = 3.0f,
               float               loopback_volume = 1.0f);

    void feed(const std::string& chunk);
    void finish();
    void abort();
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

    std::atomic<bool> m_feeding{false};
    std::atomic<bool> m_synth_done{false};
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_active{false};

    std::thread m_synth_thread;
    std::thread m_play_thread;
};
