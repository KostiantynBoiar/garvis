#pragma once
#include "miniaudio.h"

#include <cstdint>
#include <optional>
#include <string>

enum class Language { English, Russian };

struct Config {
    Language language = Language::English;

    std::string llm_model_path;
    std::string piper_exe;

    std::string whisper_model_en;
    std::string whisper_model_ru;
    std::string piper_voice_en;
    std::string piper_voice_ru;

    std::string system_prompt_en;
    std::string system_prompt_ru;

    uint32_t audio_sample_rate    = 16000;
    uint32_t audio_channels       = 1;
    float    vad_energy_threshold = 0.02f;
    float    vad_hangover_ms      = 900.0f;
    float    vad_pre_speech_ms    = 200.0f;

    std::optional<ma_device_id> capture_device_id;
    std::optional<ma_device_id> playback_device_id;
    std::optional<ma_device_id> loopback_device_id;

    int   llm_n_ctx       = 4096;
    int   llm_max_tokens  = 512;
    float llm_temperature = 0.7f;
    float llm_top_p       = 0.9f;

    int window_width  = 900;
    int window_height = 700;

    size_t tts_sentence_chunk_max = 200;

    const std::string& whisper_model() const {
        return language == Language::Russian ? whisper_model_ru : whisper_model_en;
    }
    const std::string& piper_voice() const {
        return language == Language::Russian ? piper_voice_ru : piper_voice_en;
    }
    const std::string& system_prompt() const {
        return language == Language::Russian ? system_prompt_ru : system_prompt_en;
    }

    static Config defaults();
};
