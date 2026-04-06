#pragma once
#include <cstdint>
#include <string>
#include <vector>

class TextToSpeech {
public:
    bool init(const std::string& piper_exe, const std::string& voice_model);
    void shutdown();
    bool reinit_voice(const std::string& voice_model);
    std::vector<int16_t> synthesize(const std::string& text) const;
    bool is_ready() const { return m_ready; }

private:
    std::string m_piper_exe;
    std::string m_voice_model;
    bool        m_ready = false;
};
