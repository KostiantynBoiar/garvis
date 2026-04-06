#include "tts_streamer.h"
#include <cstdio>

void TTSStreamer::start(const TextToSpeech*  tts,
                        AudioMixer*          mixer,
                        const ma_device_id*  speaker_id,
                        float                speaker_volume,
                        float                loopback_volume) {
    abort();  // clean up any previous run

    m_tts             = tts;
    m_mixer           = mixer;
    m_speaker_id      = speaker_id;
    m_speaker_volume  = speaker_volume;
    m_loopback_volume = loopback_volume;
    m_feeding    = true;
    m_synth_done = false;
    m_abort      = false;
    m_active     = true;

    // Drain leftover queue entries from a previous run
    { std::lock_guard<std::mutex> l(m_text_mtx); while (!m_text_q.empty()) m_text_q.pop(); }
    { std::lock_guard<std::mutex> l(m_pcm_mtx);  while (!m_pcm_q.empty())  m_pcm_q.pop();  }

    m_synth_thread = std::thread(&TTSStreamer::synth_loop, this);
    m_play_thread  = std::thread(&TTSStreamer::play_loop,  this);
}

void TTSStreamer::feed(const std::string& chunk) {
    if (chunk.empty() || !m_feeding) return;
    {
        std::lock_guard<std::mutex> l(m_text_mtx);
        m_text_q.push(chunk);
    }
    m_text_cv.notify_one();
}

void TTSStreamer::finish() {
    if (!m_active.load()) return;  // already stopped (e.g. by abort())

    m_feeding = false;
    m_text_cv.notify_all();  // wake synth_thread so it can observe m_feeding == false

    if (m_synth_thread.joinable()) m_synth_thread.join();
    if (m_play_thread.joinable())  m_play_thread.join();
    m_active = false;
}

void TTSStreamer::abort() {
    if (!m_active.load()) return;  // nothing running — prevents double-join

    m_abort   = true;
    m_feeding = false;
    m_text_cv.notify_all();
    m_pcm_cv.notify_all();
    if (m_synth_thread.joinable()) m_synth_thread.join();
    if (m_play_thread.joinable())  m_play_thread.join();
    m_active = false;

    if (m_mixer) m_mixer->flush_tts();
}

bool TTSStreamer::is_active() const { return m_active.load(); }

// ---------------------------------------------------------------------------
// synth_loop -- pops text chunks, synthesises via piper, pushes PCM
// ---------------------------------------------------------------------------

void TTSStreamer::synth_loop() {
    while (true) {
        std::string chunk;
        {
            std::unique_lock<std::mutex> lock(m_text_mtx);
            m_text_cv.wait(lock, [this] {
                return !m_text_q.empty() || !m_feeding || m_abort;
            });

            if (m_abort) break;
            if (m_text_q.empty()) break;  // m_feeding == false and queue empty -> done

            chunk = std::move(m_text_q.front());
            m_text_q.pop();
        }

        printf("[streamer] Synthesising: \"%s\"\n", chunk.c_str());
        auto pcm = m_tts->synthesize(chunk);

        if (!m_abort && !pcm.empty()) {
            {
                std::lock_guard<std::mutex> l(m_pcm_mtx);
                m_pcm_q.push(std::move(pcm));
            }
            m_pcm_cv.notify_one();
        }
    }

    // Signal play_thread that no more PCM will arrive
    m_synth_done = true;
    m_pcm_cv.notify_all();
}

// ---------------------------------------------------------------------------
// play_loop -- pops PCM chunks: plays on speaker AND feeds to mixer
// ---------------------------------------------------------------------------

void TTSStreamer::play_loop() {
    while (true) {
        std::vector<int16_t> pcm;
        {
            std::unique_lock<std::mutex> lock(m_pcm_mtx);
            m_pcm_cv.wait(lock, [this] {
                return !m_pcm_q.empty() || m_synth_done || m_abort;
            });

            if (m_abort) break;

            if (m_pcm_q.empty()) break;

            pcm = std::move(m_pcm_q.front());
            m_pcm_q.pop();
        }

        // Feed TTS audio into the mixer (mic + TTS -> VB-CABLE)
        if (m_mixer && m_mixer->is_running())
            m_mixer->feed_tts(pcm, 22050, m_loopback_volume);

        // Play on the local speaker
        play_pcm(pcm, 22050, m_speaker_id, m_speaker_volume);
    }
}
