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
    bool                 language_toggle_clicked = false;  // set by draw(), read by main
    bool                 stop_clicked           = false;   // set by draw_footer(), read by main

    // Settings panel state
    bool device_selection_changed  = false;  // set by draw(), read by main

    // Device lists — populated once at startup by main.cpp
    std::vector<AudioDeviceInfo> capture_devices;
    std::vector<AudioDeviceInfo> playback_devices;

    // Selected device indices; -1 means "OS default"
    int selected_capture  = -1;
    int selected_playback = -1;
    int selected_loopback = -1;

    // Loopback volume multiplier: 0.0 (silent) – 2.0 (double gain), 1.0 = unity
    float loopback_volume = 1.0f;

    std::vector<ChatTurn> history;
    std::string          partial_user;
    std::string          partial_assistant;
};

class AppUI {
public:
    void init(int width, int height, const char* title);
    void shutdown();
    void draw(UIState& state);  // non-const: sets toggle/settings flags
    bool should_close() const;

private:
    void draw_header(const UIState& s);
    void draw_mic_bar(const UIState& s);
    void draw_chat(const UIState& s);
    void draw_footer(UIState& s);        // non-const: writes language_toggle_clicked
    void draw_settings(UIState& s);      // non-const: writes device_selection_changed

    // Scrolling state for each device list in the settings panel
    int  m_scroll_capture  = 0;
    int  m_scroll_playback = 0;
    int  m_scroll_loopback = 0;
    bool m_settings_open   = false;  // persists across frames (UIState is rebuilt each frame)

    int m_width  = 0;
    int m_height = 0;
};
