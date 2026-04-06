#include "raylib.h"
#include "./config/config.h"
#include "./ui/app_ui.h"
#include "./audio/audio_capture.h"
#include "./audio/audio_devices.h"
#include "./audio/audio_mixer.h"
#include "./stt/stt.h"
#include "./llm/llm.h"
#include "./tts/tts.h"
#include "./pipeline/tts_streamer.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <future>
#include <string>
#include <vector>

extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
static constexpr unsigned int CP_UTF8_VAL = 65001;

static bool is_junk_transcript(const std::string& t) {
    if (t.empty()) return true;
    if (t.find("[BLANK_AUDIO]") != std::string::npos) return true;
    if (t.find("(blank)")       != std::string::npos) return true;
    for (unsigned char c : t)
        if (std::isalpha(c) || c > 127) return false;
    return true;
}

static bool is_sentence_end(const std::string& s) {
    if (s.size() < 2) return false;
    const char last = s.back();
    const char prev = s[s.size() - 2];
    if ((prev == '.' || prev == '!' || prev == '?') &&
        (last == ' ' || last == '\n' || last == '\r'))
        return true;
    if ((last == '.' || last == '!' || last == '?') && s.size() > 10)
        return true;
    return false;
}

static const char* lang_code(Language l) {
    return l == Language::Russian ? "ru" : "en";
}

static const ma_device_id* device_id_ptr(const std::vector<AudioDeviceInfo>& devs, int idx) {
    if (idx < 0 || idx >= static_cast<int>(devs.size())) return nullptr;
    return &devs[static_cast<size_t>(idx)].id;
}

int main() {
    SetConsoleOutputCP(CP_UTF8_VAL);

    Config cfg = Config::defaults();

    AppUI ui;
    ui.init(cfg.window_width, cfg.window_height, "Garvis");
    SetExitKey(0);

    std::vector<AudioDeviceInfo> capture_devs  = enumerate_capture_devices();
    std::vector<AudioDeviceInfo> playback_devs = enumerate_playback_devices();

    printf("[main] Found %zu capture device(s), %zu playback device(s)\n",
           capture_devs.size(), playback_devs.size());

    for (size_t i = 0; i < playback_devs.size(); ++i)
        printf("[main]   playback[%zu]: %s\n", i, playback_devs[i].name.c_str());

    const int auto_loopback = find_virtual_cable(playback_devs);
    int auto_speaker = -1;

    if (auto_loopback >= 0) {
        printf("[main] Auto-detected loopback device: \"%s\" (index %d)\n",
               playback_devs[static_cast<size_t>(auto_loopback)].name.c_str(), auto_loopback);

        auto_speaker = find_real_speaker(playback_devs);
        if (auto_speaker >= 0)
            printf("[main] Auto-selected speaker: \"%s\" (index %d)\n",
                   playback_devs[static_cast<size_t>(auto_speaker)].name.c_str(), auto_speaker);
        else
            printf("[main] WARNING: no non-virtual playback device found — "
                   "speaker will use OS default (which may be VB-CABLE)\n");
    } else {
        printf("[main] No virtual audio cable found. Install VB-CABLE "
               "(https://vb-audio.com/Cable/) to enable loopback to Discord.\n");
    }

    AudioMixer mixer;
    if (auto_loopback >= 0) {
        const ma_device_id* mic_id  = nullptr;
        const ma_device_id* vbc_id  = device_id_ptr(playback_devs, auto_loopback);
        if (!mixer.init(mic_id, vbc_id, 48000))
            printf("[main] WARNING: AudioMixer init failed — loopback disabled\n");
    }

    AudioCapture capture;
    if (!capture.init(cfg.audio_sample_rate, cfg.audio_channels)) {
        printf("ERROR: failed to initialise audio capture device\n");
        ui.shutdown();
        return 1;
    }
    VADConfig vad;
    vad.energy_threshold = cfg.vad_energy_threshold;
    vad.hangover_ms      = cfg.vad_hangover_ms;
    vad.pre_speech_ms    = cfg.vad_pre_speech_ms;
    capture.set_vad_config(vad);

    Language current_lang = cfg.language;

    SpeechToText stt;
    if (!stt.init(cfg.whisper_model()))
        printf("WARNING: STT model not loaded — place whisper model at: %s\n",
               cfg.whisper_model().c_str());

    LLMEngine llm;
    LLMConfig llm_cfg;
    llm_cfg.model_path    = cfg.llm_model_path;
    llm_cfg.system_prompt = cfg.system_prompt();
    llm_cfg.n_ctx         = cfg.llm_n_ctx;
    llm_cfg.max_tokens    = cfg.llm_max_tokens;
    llm_cfg.temperature   = cfg.llm_temperature;
    llm_cfg.top_p         = cfg.llm_top_p;
    if (!llm.init(llm_cfg))
        printf("WARNING: LLM model not loaded — place GGUF model at: %s\n",
               cfg.llm_model_path.c_str());

    TextToSpeech tts;
    if (!tts.init(cfg.piper_exe, cfg.piper_voice()))
        printf("WARNING: TTS not ready — place piper.exe at: %s\n",
               cfg.piper_exe.c_str());

    std::future<std::string> stt_future;
    std::future<std::string> llm_future;
    TTSStreamer               streamer;

    AppState              app_state = AppState::Idle;
    std::vector<ChatTurn> history;
    std::string           pending_user;

    int   cur_capture  = -1;
    int   cur_playback = auto_speaker;
    int   cur_loopback = auto_loopback;
    float cur_loopback_volume = 1.0f;

    while (!ui.should_close()) {
        CaptureState cap_st = capture.state();

        const bool pipeline_busy = stt_future.valid()
                                || llm_future.valid()
                                || streamer.is_active();

        if (!pipeline_busy) {
            if (IsKeyPressed(KEY_F1)) {
                capture.ptt_start();
                app_state = AppState::Recording;
            }
        }
        if (IsKeyReleased(KEY_F1) && app_state == AppState::Recording) {
            capture.ptt_stop();
        }

        cap_st = capture.state();

        if (cap_st == CaptureState::Done && !pipeline_busy) {
            auto buffer = capture.consume_utterance();
            const float dur = static_cast<float>(buffer.size()) /
                              static_cast<float>(cfg.audio_sample_rate);
            printf("[main] Utterance captured: %zu samples (%.2fs)\n", buffer.size(), dur);

            if (stt.is_ready()) {
                const std::string stt_lang = lang_code(current_lang);
                stt_future = std::async(std::launch::async,
                    [&stt, buf = std::move(buffer), stt_lang]() mutable {
                        return stt.transcribe(buf, stt_lang);
                    });
                app_state = AppState::Transcribing;
            } else {
                app_state = AppState::Idle;
            }
            cap_st = capture.state();
        }

        if (stt_future.valid() &&
            stt_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            pending_user = stt_future.get();
            printf("[STT] Transcript: \"%s\"\n", pending_user.c_str());

            if (!is_junk_transcript(pending_user) && llm.is_ready()) {
                const ma_device_id* spk_id = device_id_ptr(playback_devs, cur_playback);
                streamer.start(&tts, &mixer, spk_id, 3.0f, cur_loopback_volume);

                const size_t chunk_max = cfg.tts_sentence_chunk_max;
                llm_future = std::async(std::launch::async,
                    [&llm, &streamer, user = pending_user, chunk_max]() {
                        std::string full_response;
                        std::string sentence_buf;

                        llm.complete_streaming(user, [&](const std::string& tok) {
                            full_response += tok;
                            sentence_buf  += tok;
                            if (is_sentence_end(sentence_buf) ||
                                sentence_buf.size() > chunk_max)
                            {
                                streamer.feed(sentence_buf);
                                sentence_buf.clear();
                            }
                        });

                        if (!sentence_buf.empty())
                            streamer.feed(sentence_buf);

                        streamer.finish();
                        printf("[LLM] Full response: \"%s\"\n", full_response.c_str());
                        return full_response;
                    });

                app_state = AppState::Thinking;
            } else {
                if (is_junk_transcript(pending_user))
                    printf("[main] Junk transcript discarded: \"%s\"\n", pending_user.c_str());
                pending_user.clear();
                app_state = AppState::Idle;
            }
        }

        if (llm_future.valid() &&
            llm_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            const std::string response = llm_future.get();
            history.push_back({pending_user, response});
            pending_user.clear();
            app_state = AppState::Idle;
        }

        if (app_state == AppState::Thinking && streamer.is_active())
            app_state = AppState::Speaking;

        UIState ui_state;
        ui_state.app_state        = app_state;
        ui_state.cap_state        = cap_st;
        ui_state.rms              = capture.current_rms();
        ui_state.mic_muted        = streamer.is_active();
        ui_state.language         = current_lang;
        ui_state.capture_devices  = capture_devs;
        ui_state.playback_devices = playback_devs;
        ui_state.selected_capture  = cur_capture;
        ui_state.selected_playback = cur_playback;
        ui_state.selected_loopback = cur_loopback;
        ui_state.loopback_volume   = cur_loopback_volume;
        ui_state.history          = history;
        if (app_state == AppState::Transcribing || app_state == AppState::Thinking)
            ui_state.partial_user = pending_user;

        ui.draw(ui_state);

        const bool stop_requested = ui_state.stop_clicked
            || (IsKeyPressed(KEY_F2) &&
                (app_state == AppState::Speaking || app_state == AppState::Thinking));
        if (stop_requested) {
            streamer.abort();
            mixer.flush_tts();
            if (llm_future.valid()) llm_future.wait();
            history.push_back({pending_user, "[interrupted]"});
            pending_user.clear();
            app_state = AppState::Idle;
        }

        if (ui_state.device_selection_changed) {
            cur_loopback_volume = ui_state.loopback_volume;

            const bool capture_changed  = (ui_state.selected_capture  != cur_capture);
            const bool playback_changed = (ui_state.selected_playback != cur_playback);
            const bool loopback_changed = (ui_state.selected_loopback != cur_loopback);

            cur_capture  = ui_state.selected_capture;
            cur_playback = ui_state.selected_playback;
            cur_loopback = ui_state.selected_loopback;

            if (capture_changed && !pipeline_busy) {
                const ma_device_id* new_cap_id = device_id_ptr(capture_devs, cur_capture);
                if (!capture.reinit(new_cap_id))
                    printf("[main] WARNING: failed to reinit capture device\n");
            }

            printf("[main] Audio devices: mic=%d  speaker=%d  loopback=%d  vol=%.2f\n",
                   cur_capture, cur_playback, cur_loopback, static_cast<double>(cur_loopback_volume));
        }

        if (ui_state.language_toggle_clicked && !pipeline_busy) {
            current_lang = (current_lang == Language::English)
                           ? Language::Russian
                           : Language::English;

            const std::string new_voice = (current_lang == Language::Russian)
                                          ? cfg.piper_voice_ru
                                          : cfg.piper_voice_en;
            const std::string new_whisper = (current_lang == Language::Russian)
                                            ? cfg.whisper_model_ru
                                            : cfg.whisper_model_en;
            printf("[main] Language switched to %s\n", lang_code(current_lang));

            stt.reinit(new_whisper);
            tts.reinit_voice(new_voice);
            llm.set_system_prompt(current_lang == Language::Russian
                                  ? cfg.system_prompt_ru
                                  : cfg.system_prompt_en);
        }
    }

    streamer.abort();
    if (stt_future.valid()) stt_future.wait();
    if (llm_future.valid()) llm_future.wait();

    mixer.shutdown();
    tts.shutdown();
    llm.shutdown();
    stt.shutdown();
    capture.shutdown();
    ui.shutdown();
    return 0;
}
