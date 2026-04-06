#include "tts.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool TextToSpeech::init(const std::string& piper_exe,
                        const std::string& voice_model) {
    if (!std::filesystem::exists(piper_exe)) {
        fprintf(stderr, "[TTS] piper.exe not found at: %s\n", piper_exe.c_str());
        return false;
    }
    if (!std::filesystem::exists(voice_model)) {
        fprintf(stderr, "[TTS] Voice model not found at: %s\n", voice_model.c_str());
        return false;
    }
    m_piper_exe   = piper_exe;
    m_voice_model = voice_model;
    m_ready       = true;
    fprintf(stdout, "[TTS] Ready. exe=%s model=%s\n",
            piper_exe.c_str(), voice_model.c_str());
    return true;
}

void TextToSpeech::shutdown() {
    m_ready = false;
}

bool TextToSpeech::reinit_voice(const std::string& voice_model) {
    shutdown();
    return init(m_piper_exe, voice_model);
}

std::vector<int16_t> TextToSpeech::synthesize(const std::string& text) const {
    if (!m_ready || text.empty()) return {};

    std::string cmd = "\"" + m_piper_exe + "\""
                    + " --model \"" + m_voice_model + "\""
                    + " --output-raw"
                    + " --quiet";

    HANDLE stdin_read  = nullptr, stdin_write  = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&stdin_read,  &stdin_write,  &sa, 0) ||
        !CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        fprintf(stderr, "[TTS] CreatePipe failed\n");
        return {};
    }

    // Parent holds the write-end of stdin and read-end of stdout;
    // those must not be inherited by the child process.
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb        = sizeof(STARTUPINFOA);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags   |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    const BOOL ok = CreateProcessA(
        nullptr, cmd_buf.data(),
        nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW,  // hide the console window piper would open
        nullptr, nullptr,
        &si, &pi
    );

    CloseHandle(stdin_read);
    CloseHandle(stdout_write);

    if (!ok) {
        fprintf(stderr, "[TTS] CreateProcess failed (err=%lu): %s\n",
                GetLastError(), cmd.c_str());
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        return {};
    }

    std::string input = text + "\n";
    DWORD written = 0;
    WriteFile(stdin_write, input.c_str(), static_cast<DWORD>(input.size()), &written, nullptr);
    CloseHandle(stdin_write);

    std::vector<int16_t> samples;
    {
        constexpr DWORD BUF_BYTES = 65536;
        std::vector<uint8_t> tmp(BUF_BYTES);
        DWORD bytes_read = 0;
        while (ReadFile(stdout_read, tmp.data(), BUF_BYTES, &bytes_read, nullptr) &&
               bytes_read > 0) {
            const int16_t* p = reinterpret_cast<const int16_t*>(tmp.data());
            samples.insert(samples.end(), p, p + bytes_read / sizeof(int16_t));
        }
    }
    CloseHandle(stdout_read);

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    fprintf(stdout, "[TTS] Synthesized %zu samples (%.2f s @ 22050 Hz)\n",
            samples.size(), static_cast<float>(samples.size()) / 22050.0f);
    return samples;
}
