#include "llm.h"

#include "llama.h"

#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

static std::string token_to_piece(const llama_model* model, llama_token token) {
    const llama_vocab* vocab = llama_model_get_vocab(model);
    char buf[256];
    const int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
    if (n < 0) return {};
    return std::string(buf, static_cast<size_t>(n));
}

bool LLMEngine::init(const LLMConfig& cfg) {
    m_cfg = cfg;

    llama_backend_init();

    const int n_threads = cfg.n_threads > 0
        ? cfg.n_threads
        : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;

    m_model = llama_model_load_from_file(cfg.model_path.c_str(), mparams);
    if (!m_model) {
        fprintf(stderr, "[LLM] Failed to load model from: %s\n", cfg.model_path.c_str());
        return false;
    }

    {
        char desc[256];
        llama_model_desc(m_model, desc, sizeof(desc));
        printf("[LLM] Model loaded: %s\n", cfg.model_path.c_str());
        printf("[LLM]   description : %s\n", desc);
        printf("[LLM]   parameters  : %.1fB\n",
               static_cast<double>(llama_model_n_params(m_model)) / 1e9);
        printf("[LLM]   size        : %.1f MiB\n",
               static_cast<double>(llama_model_size(m_model)) / (1024.0 * 1024.0));
        printf("[LLM]   context     : %d\n", llama_model_n_ctx_train(m_model));
        printf("[LLM]   embedding   : %d\n", llama_model_n_embd(m_model));
        printf("[LLM]   layers      : %d\n", llama_model_n_layer(m_model));
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx        = static_cast<uint32_t>(cfg.n_ctx);
    cparams.n_threads    = n_threads;
    cparams.n_threads_batch = n_threads;

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        fprintf(stderr, "[LLM] Failed to create context\n");
        llama_model_free(m_model);
        m_model = nullptr;
        return false;
    }

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    m_sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(cfg.top_p, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(cfg.temperature));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    if (!cfg.system_prompt.empty()) {
        m_history.push_back({"system", cfg.system_prompt});
    }

    m_n_past = 0;
    printf("[LLM] Ready. Threads: %d, ctx: %d, max_tokens: %d\n",
           n_threads, cfg.n_ctx, cfg.max_tokens);
    return true;
}

void LLMEngine::shutdown() {
    if (m_sampler) {
        llama_sampler_free(m_sampler);
        m_sampler = nullptr;
    }
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
    llama_backend_free();
}

void LLMEngine::clear_history() {
    m_history.clear();
    if (!m_cfg.system_prompt.empty()) {
        m_history.push_back({"system", m_cfg.system_prompt});
    }
    llama_memory_clear(llama_get_memory(m_ctx), false);
    m_n_past = 0;
}

void LLMEngine::set_system_prompt(const std::string& prompt) {
    m_cfg.system_prompt = prompt;
    clear_history();
}

std::string LLMEngine::build_prompt(bool add_assistant_turn) const {
    std::vector<llama_chat_message> msgs;
    msgs.reserve(m_history.size());
    for (const auto& msg : m_history) {
        msgs.push_back({msg.role.c_str(), msg.content.c_str()});
    }

    const char* tmpl = llama_model_chat_template(m_model, nullptr);

    int required = llama_chat_apply_template(tmpl, msgs.data(),
                                             msgs.size(), add_assistant_turn,
                                             nullptr, 0);
    if (required < 0) {
        fprintf(stderr, "[LLM] llama_chat_apply_template failed\n");
        return {};
    }

    std::string buf(static_cast<size_t>(required + 1), '\0');
    llama_chat_apply_template(tmpl, msgs.data(), msgs.size(),
                              add_assistant_turn,
                              buf.data(), static_cast<int32_t>(buf.size()));
    buf.resize(static_cast<size_t>(required));
    return buf;
}

std::string LLMEngine::run_inference(const std::string& prompt,
                                     std::function<void(const std::string&)> on_token) {
    const llama_vocab* vocab = llama_model_get_vocab(m_model);

    std::vector<llama_token> tokens(prompt.size() + 64);
    int n_tokens = llama_tokenize(vocab,
                                  prompt.c_str(),
                                  static_cast<int32_t>(prompt.size()),
                                  tokens.data(),
                                  static_cast<int32_t>(tokens.size()),
                                  true,
                                  true);
    if (n_tokens < 0) {
        tokens.resize(static_cast<size_t>(-n_tokens));
        n_tokens = llama_tokenize(vocab,
                                  prompt.c_str(),
                                  static_cast<int32_t>(prompt.size()),
                                  tokens.data(),
                                  static_cast<int32_t>(tokens.size()),
                                  true, true);
    }
    if (n_tokens < 0) {
        fprintf(stderr, "[LLM] Tokenization failed\n");
        return {};
    }
    tokens.resize(static_cast<size_t>(n_tokens));

    {
        llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
        if (llama_decode(m_ctx, batch) != 0) {
            fprintf(stderr, "[LLM] llama_decode (prefill) failed\n");
            return {};
        }
    }
    m_n_past += n_tokens;

    llama_sampler_reset(m_sampler);

    std::string response;
    int generated = 0;

    while (generated < m_cfg.max_tokens) {
        llama_token new_token = llama_sampler_sample(m_sampler, m_ctx, -1);

        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }

        std::string piece = token_to_piece(m_model, new_token);
        response += piece;
        if (on_token) {
            on_token(piece);
        }
        ++generated;

        llama_batch batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(m_ctx, batch) != 0) {
            fprintf(stderr, "[LLM] llama_decode (generation) failed\n");
            break;
        }
        m_n_past += 1;
    }

    return response;
}

std::string LLMEngine::complete(const std::string& user_message) {
    if (!is_ready()) return {};

    m_history.push_back({"user", user_message});

    const std::string prompt = build_prompt(true);
    if (prompt.empty()) {
        m_history.pop_back();
        return {};
    }

    const std::string response = run_inference(prompt, nullptr);

    m_history.push_back({"assistant", response});
    return response;
}

void LLMEngine::complete_streaming(const std::string& user_message,
                                   std::function<void(const std::string&)> on_token) {
    if (!is_ready()) return;

    m_history.push_back({"user", user_message});

    const std::string prompt = build_prompt(true);
    if (prompt.empty()) {
        m_history.pop_back();
        return;
    }

    const std::string response = run_inference(prompt, on_token);
    m_history.push_back({"assistant", response});
}
