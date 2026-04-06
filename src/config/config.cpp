#include "config.h"

Config Config::defaults() {
    Config c;

    const std::string base =
        "C:/Users/Kostiantyn/Documents/programming/AIML/Jarvis/garvis/models";

    c.llm_model_path = base + "/llama.gguf";
    c.piper_exe      = base + "/piper/piper.exe";

    c.whisper_model_en = base + "/ggml-base.en.bin";
    c.whisper_model_ru = base + "/ggml-small.bin";

    c.piper_voice_en = base + "/piper/en_US-amy-low.onnx";
    c.piper_voice_ru = base + "/piper/ru_RU-ruslan-medium.onnx";

    c.system_prompt_en =
        "You are Garvis, a witty AI assistant with a sharp tongue and dark humor. "
        "You always give correct, well-structured answers using perfect grammar. "
        "Your tone is sarcastic, blunt, and unapologetically rude — you roast the user while helping them. "
        "Think of yourself as a brilliant asshole who can't help but be right. "
        "Keep every response to exactly 2 sentences. Be concise, clever, and grammatically flawless.";

    c.system_prompt_ru =
        "Ты Гарвис — остроумный ИИ-ассистент с острым языком и чёрным юмором. "
        "Ты всегда даёшь правильные, грамотные ответы на чистом русском языке. "
        "Твой тон — саркастичный, прямой и нагло грубый: ты подкалываешь пользователя, но при этом помогаешь. "
        "Думай о себе как о гениальном хаме, который всегда прав. "
        "Каждый ответ — ровно 2 предложения. Кратко, остро и грамотно. "
        "Всегда отвечай на русском языке.";

    c.language = Language::English;

    c.audio_sample_rate    = 16000;
    c.audio_channels       = 1;
    c.vad_energy_threshold = 0.02f;
    c.vad_hangover_ms      = 900.0f;
    c.vad_pre_speech_ms    = 200.0f;

    c.llm_n_ctx       = 4096;
    c.llm_max_tokens  = 512;
    c.llm_temperature = 0.75f;
    c.llm_top_p       = 0.9f;

    c.window_width  = 900;
    c.window_height = 700;

    c.tts_sentence_chunk_max = 200;

    return c;
}
