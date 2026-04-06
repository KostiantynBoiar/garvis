#pragma once

#include <functional>
#include <string>
#include <vector>

struct llama_model;
struct llama_context;
struct llama_sampler;

struct LLMConfig {
    std::string model_path;
    std::string system_prompt;
    int   n_ctx       = 4096;
    int   n_threads   = 0;
    int   max_tokens  = 512;
    float temperature = 0.7f;
    float top_p       = 0.9f;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

class LLMEngine {
public:
    bool init(const LLMConfig& cfg);
    void shutdown();

    std::string complete(const std::string& user_message);

    void complete_streaming(const std::string& user_message,
                            std::function<void(const std::string& token)> on_token);

    void clear_history();
    void set_system_prompt(const std::string& prompt);
    bool is_ready() const { return m_model != nullptr && m_ctx != nullptr; }

private:
    std::string build_prompt(bool add_assistant_turn) const;
    std::string run_inference(const std::string& prompt,
                              std::function<void(const std::string&)> on_token);

    llama_model*   m_model   = nullptr;
    llama_context* m_ctx     = nullptr;
    llama_sampler* m_sampler = nullptr;

    LLMConfig m_cfg;
    std::vector<ChatMessage> m_history;

    int m_n_past = 0;
};
