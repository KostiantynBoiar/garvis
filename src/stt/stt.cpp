#include "stt.h"
#include "whisper.h"

#include <cstdio>
#include <string>
#include <thread>

bool SpeechToText::init(const std::string& model_path) {
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;

    m_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!m_ctx) {
        fprintf(stderr, "[STT] Failed to load whisper model from: %s\n", model_path.c_str());
        return false;
    }

    printf("[STT] Model loaded: %s\n", model_path.c_str());
    printf("[STT]   type        : %s\n", whisper_model_type_readable(m_ctx));
    printf("[STT]   vocab size  : %d\n", whisper_model_n_vocab(m_ctx));
    printf("[STT]   audio layers: %d\n", whisper_model_n_audio_layer(m_ctx));
    printf("[STT]   text layers : %d\n", whisper_model_n_text_layer(m_ctx));
    return true;
}

void SpeechToText::shutdown() {
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

bool SpeechToText::reinit(const std::string& model_path) {
    shutdown();
    return init(model_path);
}

std::string SpeechToText::transcribe(const std::vector<float>& pcm_audio,
                                      const std::string& lang) {
    if (!m_ctx || pcm_audio.empty()) return {};

    // Multilingual model required for non-English; warn if the user loaded an
    // English-only model (filename contains ".en") with a non-English lang tag.
    if (lang != "en") {
        const char* model_type = whisper_model_type_readable(m_ctx);
        if (model_type && std::string(model_type).find(".en") != std::string::npos) {
            fprintf(stderr,
                "[STT] WARNING: language=%s requested but model appears to be "
                "English-only. Download ggml-base.bin (multilingual) for Russian.\n",
                lang.c_str());
        }
    }

    // Beam search is noticeably more accurate than greedy at a small latency cost.
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    params.n_threads        = std::max(1, static_cast<int>(
                                  std::thread::hardware_concurrency()) - 1);
    params.beam_search.beam_size = 5;
    params.language         = lang.c_str();
    params.no_timestamps    = true;
    params.single_segment   = false;  // let whisper decide segment boundaries
    params.print_special    = false;
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;

    // An initial prompt primes the vocabulary towards conversational speech and
    // the assistant's name, reducing hallucinations on short or noisy inputs.
    const std::string prompt = (lang == "ru")
        ? "Привет Гарвис. Как дела? Что ты умеешь?"
        : "Hey Garvis. How are you? What can you do?";
    params.initial_prompt = prompt.c_str();

    if (whisper_full(m_ctx, params, pcm_audio.data(),
                     static_cast<int>(pcm_audio.size())) != 0) {
        fprintf(stderr, "[STT] whisper_full() failed\n");
        return {};
    }

    std::string result;
    const int n = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n; ++i) {
        const char* t = whisper_full_get_segment_text(m_ctx, i);
        if (t) result += t;
    }

    const auto first = result.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return {};
    const auto last = result.find_last_not_of(" \t\n\r");
    return result.substr(first, last - first + 1);
}
