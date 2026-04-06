#pragma once
#include "../audio/audio_capture.h"
#include "../audio/audio_devices.h"
#include "../config/config.h"

#include <string>
#include <vector>

enum class AppState {
    Idle,
    Recording,
    Transcribing,
    Thinking,
    Speaking,
};

struct ChatTurn {
    std::string user;
    std::string assistant;
};

struct UIState {
    AppState             app_state              = AppState::Idle;
    CaptureState         cap_state              = CaptureState::Idle;
    float                rms                    = 0.0f;
    bool                 mic_muted              = false;
    Language             language               = Language::English;
    bool                 language_toggle_clicked = false;
    bool                 stop_clicked           = false;

    bool device_selection_changed  = false;

    std::vector<AudioDeviceInfo> capture_devices;
    std::vector<AudioDeviceInfo> playback_devices;

    int selected_capture  = -1;
    int selected_playback = -1;
    int selected_loopback = -1;

    float loopback_volume = 1.0f;

    std::vector<ChatTurn> history;
    std::string          partial_user;
    std::string          partial_assistant;
};

class AppUI {
public:
    void init(int width, int height, const char* title);
    void shutdown();
    void draw(UIState& state);
    bool should_close() const;

private:
    void draw_header(const UIState& s);
    void draw_mic_bar(const UIState& s);
    void draw_chat(const UIState& s);
    void draw_footer(UIState& s);
    void draw_settings(UIState& s);

    int  m_scroll_capture  = 0;
    int  m_scroll_playback = 0;
    int  m_scroll_loopback = 0;
    bool m_settings_open   = false;

    int m_width  = 0;
    int m_height = 0;
};
