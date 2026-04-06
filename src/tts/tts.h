#pragma once
#include <cstdint>
#include <string>
#include <vector>

class TextToSpeech {
public:
    bool init(const std::string& piper_exe, const std::string& voice_model);
    void shutdown();

    // Switch to a different voice model at runtime (e.g. when the language is toggled).
    // Equivalent to shutdown() + init() with the new voice, keeping the same piper_exe.
    bool reinit_voice(const std::string& voice_model);

    // Synthesize text → raw s16le PCM at 22050 Hz mono. Blocks until done.
    // Returns an empty vector on failure.
    std::vector<int16_t> synthesize(const std::string& text) const;

    bool is_ready() const { return m_ready; }

private:
    std::string m_piper_exe;
    std::string m_voice_model;
    bool        m_ready = false;
};
