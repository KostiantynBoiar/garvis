#pragma once

#include <string>
#include <vector>

struct whisper_context;

class SpeechToText {
public:
    bool init(const std::string& model_path);
    void shutdown();

    // Hot-swap to a different whisper model (e.g. multilingual on language switch).
    bool reinit(const std::string& model_path);

    std::string transcribe(const std::vector<float>& pcm_audio,
                           const std::string& lang = "en");

    bool is_ready() const { return m_ctx != nullptr; }

private:
    whisper_context* m_ctx = nullptr;
};
