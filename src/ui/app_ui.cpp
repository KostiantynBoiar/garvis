#include "app_ui.h"
#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static constexpr Color COL_BG          = {15,  15,  23,  255};
static constexpr Color COL_SURFACE     = {24,  24,  38,  255};
static constexpr Color COL_BORDER      = {45,  45,  70,  255};
static constexpr Color COL_TEXT        = {220, 220, 230, 255};
static constexpr Color COL_TEXT_DIM    = {110, 110, 140, 255};
static constexpr Color COL_ACCENT_BLUE = {80,  160, 255, 255};
static constexpr Color COL_ACCENT_GRN  = {80,  220, 140, 255};
static constexpr Color COL_ACCENT_YLW  = {255, 210, 80,  255};
static constexpr Color COL_ACCENT_RED  = {255, 90,  90,  255};
static constexpr Color COL_ACCENT_PRP  = {170, 100, 255, 255};
static constexpr Color COL_MIC_BAR_BG  = {35,  35,  55,  255};

static constexpr int PAD          = 24;
static constexpr int HEADER_H     = 64;
static constexpr int MIC_BAR_H    = 48;
static constexpr int FOOTER_H     = 36;
static constexpr int FONT_TITLE   = 28;
static constexpr int FONT_LABEL   = 16;
static constexpr int FONT_CHAT    = 17;
static constexpr int FONT_STATUS  = 15;
static constexpr int TURN_GAP     = 14;
static constexpr int BUBBLE_PAD   = 10;
static constexpr int MAX_CHAT_W   = 780;

static Color state_badge_color(AppState s) {
    switch (s) {
        case AppState::Idle:         return COL_TEXT_DIM;
        case AppState::Recording:    return COL_ACCENT_RED;
        case AppState::Transcribing: return COL_ACCENT_YLW;
        case AppState::Thinking:     return COL_ACCENT_BLUE;
        case AppState::Speaking:     return COL_ACCENT_PRP;
    }
    return COL_TEXT_DIM;
}

static const char* state_badge_label(AppState s) {
    switch (s) {
        case AppState::Idle:         return "IDLE";
        case AppState::Recording:    return "RECORDING";
        case AppState::Transcribing: return "TRANSCRIBING";
        case AppState::Thinking:     return "THINKING";
        case AppState::Speaking:     return "SPEAKING";
    }
    return "";
}

static Color cap_indicator_color(CaptureState s) {
    switch (s) {
        case CaptureState::Idle:      return COL_TEXT_DIM;
        case CaptureState::Listening: return COL_ACCENT_BLUE;
        case CaptureState::Recording: return COL_ACCENT_RED;
        case CaptureState::Hangover:  return COL_ACCENT_YLW;
        case CaptureState::Done:      return COL_ACCENT_GRN;
    }
    return COL_TEXT_DIM;
}

static int draw_wrapped(const std::string& text, int x, int y,
                        int font_size, int max_w, Color col) {
    if (text.empty()) return 0;
    const int char_w = font_size / 2;
    const int chars  = std::max(1, max_w / char_w);
    int cy = y;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = std::min(pos + static_cast<size_t>(chars), text.size());
        if (end < text.size()) {
            size_t sp = text.rfind(' ', end);
            if (sp != std::string::npos && sp > pos) end = sp + 1;
        }
        DrawText(text.substr(pos, end - pos).c_str(), x, cy, font_size, col);
        cy += font_size + 3;
        pos = end;
    }
    return cy - y;
}

static int wrapped_height(const std::string& text, int font_size, int max_w) {
    if (text.empty()) return 0;
    const int char_w = std::max(1, font_size / 2);
    const int chars  = std::max(1, max_w / char_w);
    int lines = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = std::min(pos + static_cast<size_t>(chars), text.size());
        if (end < text.size()) {
            size_t sp = text.rfind(' ', end);
            if (sp != std::string::npos && sp > pos) end = sp + 1;
        }
        ++lines;
        pos = end;
    }
    return lines * (font_size + 3);
}

void AppUI::init(int width, int height, const char* title) {
    m_width  = width;
    m_height = height;
    InitWindow(width, height, title);
    SetTargetFPS(60);
}

void AppUI::shutdown() {
    CloseWindow();
}

bool AppUI::should_close() const {
    return WindowShouldClose();
}

void AppUI::draw(UIState& state) {
    state.language_toggle_clicked = false;
    state.device_selection_changed = false;
    state.stop_clicked = false;
    BeginDrawing();
    ClearBackground(COL_BG);

    draw_header(state);
    draw_mic_bar(state);
    draw_chat(state);

    const bool settings_was_open = m_settings_open;
    draw_footer(state);

    if (m_settings_open && settings_was_open)
        draw_settings(state);

    EndDrawing();
}

void AppUI::draw_header(const UIState& s) {
    DrawRectangle(0, 0, m_width, HEADER_H, COL_SURFACE);
    DrawLine(0, HEADER_H, m_width, HEADER_H, COL_BORDER);

    DrawText("GARVIS", PAD, (HEADER_H - FONT_TITLE) / 2, FONT_TITLE, COL_TEXT);

    const char* badge    = state_badge_label(s.app_state);
    Color        badge_c = state_badge_color(s.app_state);
    const int    bw      = MeasureText(badge, FONT_LABEL) + 20;
    const int    bx      = m_width - PAD - bw;
    const int    by      = (HEADER_H - FONT_LABEL - 8) / 2;

    DrawRectangleRounded({(float)bx, (float)by, (float)bw, (float)(FONT_LABEL + 8)},
                         0.4f, 6, {badge_c.r, badge_c.g, badge_c.b, 40});
    DrawRectangleRoundedLines({(float)bx, (float)by, (float)bw, (float)(FONT_LABEL + 8)},
                               0.4f, 6, badge_c);
    DrawText(badge, bx + 10, by + 4, FONT_LABEL, badge_c);
}

void AppUI::draw_mic_bar(const UIState& s) {
    const int y0 = HEADER_H;

    DrawRectangle(0, y0, m_width, MIC_BAR_H, COL_SURFACE);
    DrawLine(0, y0 + MIC_BAR_H, m_width, y0 + MIC_BAR_H, COL_BORDER);

    const Color dot_c = s.mic_muted ? COL_TEXT_DIM
                                    : cap_indicator_color(s.cap_state);

    DrawCircle(PAD + 8, y0 + MIC_BAR_H / 2, 7, dot_c);

    const char* mic_label = s.mic_muted ? "MIC PAUSED" : "MIC";
    DrawText(mic_label, PAD + 22, y0 + (MIC_BAR_H - FONT_STATUS) / 2,
             FONT_STATUS, s.mic_muted ? COL_TEXT_DIM : COL_TEXT);

    constexpr int BAR_X = PAD + 100;
    constexpr int BAR_H = 10;
    const int     bar_y = y0 + (MIC_BAR_H - BAR_H) / 2;
    const int     bar_w = m_width - BAR_X - PAD - 60;

    DrawRectangle(BAR_X, bar_y, bar_w, BAR_H, COL_MIC_BAR_BG);
    DrawRectangleRounded({0, 0, 0, 0}, 0, 0, BLANK);

    const float clamped = std::min(s.rms / 0.08f, 1.0f);
    const int   filled  = static_cast<int>(clamped * bar_w);
    if (filled > 0) {
        DrawRectangle(BAR_X, bar_y, filled, BAR_H,
                      s.mic_muted ? COL_TEXT_DIM : dot_c);
    }

    const int tick_x = BAR_X + static_cast<int>(0.25f * bar_w);
    DrawLine(tick_x, bar_y - 3, tick_x, bar_y + BAR_H + 3, COL_ACCENT_YLW);
}

void AppUI::draw_chat(const UIState& s) {
    const int top    = HEADER_H + MIC_BAR_H + PAD;
    const int bottom = m_height - FOOTER_H - PAD;
    const int chat_h = bottom - top;
    if (chat_h <= 0) return;

    const int chat_w = std::min(m_width - 2 * PAD, MAX_CHAT_W);
    const int chat_x = PAD;

    struct Line { std::string text; Color color; bool is_label; };
    std::vector<Line> lines;

    auto push_turn = [&](const ChatTurn& t) {
        if (!t.assistant.empty()) {
            lines.push_back({"Garvis:", COL_ACCENT_GRN, true});
            lines.push_back({t.assistant, COL_TEXT, false});
        }
        if (!t.user.empty()) {
            lines.push_back({"You:", COL_ACCENT_BLUE, true});
            lines.push_back({t.user, COL_TEXT, false});
        }
    };

    if (!s.partial_assistant.empty()) {
        lines.push_back({"Garvis:", COL_ACCENT_GRN, true});
        lines.push_back({s.partial_assistant, COL_TEXT_DIM, false});
    }
    if (!s.partial_user.empty()) {
        lines.push_back({"You:", COL_ACCENT_BLUE, true});
        lines.push_back({s.partial_user, COL_TEXT_DIM, false});
    }

    for (int i = static_cast<int>(s.history.size()) - 1; i >= 0; --i) {
        push_turn(s.history[static_cast<size_t>(i)]);
    }

    int y = bottom;
    const int label_h = FONT_CHAT + 4;
    const int text_indent = 16;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& ln = lines[i];
        int lh = ln.is_label
            ? label_h
            : wrapped_height(ln.text, FONT_CHAT, chat_w - text_indent) + TURN_GAP;

        y -= lh;
        if (y < top) break;

        if (ln.is_label) {
            DrawText(ln.text.c_str(), chat_x, y, FONT_CHAT, ln.color);
        } else {
            draw_wrapped(ln.text, chat_x + text_indent, y, FONT_CHAT,
                         chat_w - text_indent, ln.color);
        }
    }

    for (int row = 0; row < 32; ++row) {
        const unsigned char alpha = static_cast<unsigned char>(
            (1.0f - row / 32.0f) * 220.0f);
        DrawRectangle(0, top + row, m_width, 1,
                      {COL_BG.r, COL_BG.g, COL_BG.b, alpha});
    }
}

void AppUI::draw_footer(UIState& s) {
    const int y0 = m_height - FOOTER_H;
    DrawLine(0, y0, m_width, y0, COL_BORDER);
    DrawRectangle(0, y0, m_width, FOOTER_H, COL_SURFACE);

    const char* hint;
    Color hint_c;
    if (s.mic_muted) {
        hint   = "Speaking — microphone paused";
        hint_c = COL_ACCENT_PRP;
    } else if (s.app_state == AppState::Recording) {
        hint   = "Release F1 to send";
        hint_c = COL_ACCENT_RED;
    } else {
        hint   = "Hold F1 to speak  •  F2 to stop";
        hint_c = COL_TEXT_DIM;
    }
    DrawText(hint, PAD, y0 + (FOOTER_H - FONT_STATUS) / 2, FONT_STATUS, hint_c);

    const bool is_ru    = (s.language == Language::Russian);
    const char* lbl_en  = "EN";
    const char* lbl_ru  = "RU";
    const int   btn_h   = FOOTER_H - 10;
    const int   btn_w   = 32;
    const int   btn_gap = 4;
    const int   btn_y   = y0 + 5;

    const int btn_ru_x = m_width - PAD - btn_w;
    const int btn_en_x = btn_ru_x - btn_gap - btn_w;

    const int gear_w  = 46;
    const int gear_x  = btn_en_x - btn_gap - gear_w;

    const int stop_w = 46;
    const int stop_x = gear_x - btn_gap - stop_w;

    if (s.app_state == AppState::Speaking || s.app_state == AppState::Thinking) {
        DrawRectangleRounded({(float)stop_x, (float)btn_y, (float)stop_w, (float)btn_h},
                             0.3f, 4, Color{255, 60, 60, 50});
        DrawRectangleRoundedLines({(float)stop_x, (float)btn_y, (float)stop_w, (float)btn_h},
                                  0.3f, 4, COL_ACCENT_RED);
        const char* stop_lbl = "STOP";
        const int   stw = MeasureText(stop_lbl, FONT_STATUS);
        DrawText(stop_lbl, stop_x + (stop_w - stw) / 2,
                 btn_y + (btn_h - FONT_STATUS) / 2, FONT_STATUS, COL_ACCENT_RED);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2  mp  = GetMousePosition();
            const Rectangle rc = {(float)stop_x, (float)btn_y, (float)stop_w, (float)btn_h};
            if (CheckCollisionPointRec(mp, rc))
                s.stop_clicked = true;
        }
    }

    {
        const bool open    = m_settings_open;
        const Color bg     = open ? Color{80, 160, 255, 60}  : Color{0, 0, 0, 0};
        const Color bdr    = open ? COL_ACCENT_BLUE          : COL_BORDER;
        const Color txt    = open ? COL_ACCENT_BLUE          : COL_TEXT_DIM;
        DrawRectangleRounded({(float)gear_x, (float)btn_y, (float)gear_w, (float)btn_h},
                             0.3f, 4, bg);
        DrawRectangleRoundedLines({(float)gear_x, (float)btn_y, (float)gear_w, (float)btn_h},
                                  0.3f, 4, bdr);
        const char* gear_lbl = "AUD";
        const int   tw = MeasureText(gear_lbl, FONT_STATUS);
        DrawText(gear_lbl, gear_x + (gear_w - tw) / 2,
                 btn_y + (btn_h - FONT_STATUS) / 2, FONT_STATUS, txt);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2  mp  = GetMousePosition();
            const Rectangle rc = {(float)gear_x, (float)btn_y, (float)gear_w, (float)btn_h};
            if (CheckCollisionPointRec(mp, rc))
                m_settings_open = !m_settings_open;
        }
    }

    auto draw_lang_btn = [&](int bx, const char* lbl, bool active) {
        const Color bg  = active ? Color{80, 160, 255, 60}  : Color{0, 0, 0, 0};
        const Color bdr = active ? COL_ACCENT_BLUE          : COL_BORDER;
        const Color txt = active ? COL_ACCENT_BLUE          : COL_TEXT_DIM;
        DrawRectangleRounded({(float)bx, (float)btn_y, (float)btn_w, (float)btn_h},
                             0.3f, 4, bg);
        DrawRectangleRoundedLines({(float)bx, (float)btn_y, (float)btn_w, (float)btn_h},
                                  0.3f, 4, bdr);
        const int tw = MeasureText(lbl, FONT_STATUS);
        DrawText(lbl, bx + (btn_w - tw) / 2,
                 btn_y + (btn_h - FONT_STATUS) / 2, FONT_STATUS, txt);
    };

    draw_lang_btn(btn_en_x, lbl_en, !is_ru);
    draw_lang_btn(btn_ru_x, lbl_ru,  is_ru);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mp = GetMousePosition();
        Rectangle rect_en = {(float)btn_en_x, (float)btn_y, (float)btn_w, (float)btn_h};
        Rectangle rect_ru = {(float)btn_ru_x, (float)btn_y, (float)btn_w, (float)btn_h};
        if ((!is_ru && CheckCollisionPointRec(mp, rect_ru)) ||
            ( is_ru && CheckCollisionPointRec(mp, rect_en))) {
            s.language_toggle_clicked = true;
        }
    }
}

static constexpr int SET_PANEL_W    = 500;
static constexpr int SET_ITEM_H     = 26;
static constexpr int SET_LIST_ROWS  = 5;
static constexpr int SET_HEADER_H   = 22;
static constexpr int SET_LIST_H     = SET_ITEM_H * SET_LIST_ROWS;
static constexpr int SET_SECTION_H  = SET_HEADER_H + SET_LIST_H + 8;
static constexpr int SET_SLIDER_H   = 40;
static constexpr int SET_PANEL_H    = SET_SECTION_H * 3 + SET_SLIDER_H + 48;

static bool draw_device_list(
    const char*                       title,
    const std::vector<AudioDeviceInfo>& devices,
    int                               selected,
    int&                              scroll,
    int                               px, int py, int pw,
    int&                              new_selected_out)
{
    const int lx = px + 8;
    const int lw = pw - 16;

    DrawText(title, lx, py, FONT_LABEL, COL_TEXT_DIM);
    py += SET_HEADER_H;

    const int total_rows = static_cast<int>(devices.size()) + 1;
    if (scroll < 0) scroll = 0;
    if (scroll > std::max(0, total_rows - SET_LIST_ROWS))
        scroll = std::max(0, total_rows - SET_LIST_ROWS);

    const Vector2 mp  = GetMousePosition();
    const Rectangle list_rect = {(float)lx, (float)py, (float)lw, (float)SET_LIST_H};
    if (CheckCollisionPointRec(mp, list_rect)) {
        const float wheel = GetMouseWheelMove();
        scroll -= static_cast<int>(wheel);
        scroll = std::max(0, std::min(scroll, std::max(0, total_rows - SET_LIST_ROWS)));
    }

    bool clicked = false;
    new_selected_out = selected;

    for (int row = 0; row < SET_LIST_ROWS; ++row) {
        const int idx = row + scroll - 1;
        const int ry  = py + row * SET_ITEM_H;

        const bool is_active = (idx == selected);
        const Color item_bg  = is_active ? Color{80, 160, 255, 40} : Color{0, 0, 0, 0};
        DrawRectangle(lx, ry, lw, SET_ITEM_H - 1, item_bg);

        const char* label;
        std::string label_owned;
        if (idx < 0) {
            label = "(default)";
        } else if (idx < static_cast<int>(devices.size())) {
            label_owned = devices[static_cast<size_t>(idx)].name;
            while (!label_owned.empty() &&
                   MeasureText(label_owned.c_str(), FONT_STATUS) > lw - 8)
                label_owned.resize(label_owned.size() - 1);
            label = label_owned.c_str();
        } else {
            break;
        }

        const Color text_c = is_active ? COL_ACCENT_BLUE : COL_TEXT;
        DrawText(label, lx + 6, ry + (SET_ITEM_H - FONT_STATUS) / 2, FONT_STATUS, text_c);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mp, {(float)lx, (float)ry, (float)lw, (float)(SET_ITEM_H - 1)}))
        {
            new_selected_out = idx;
            clicked = true;
        }
    }

    DrawRectangleLines(lx, py, lw, SET_LIST_H, COL_BORDER);

    if (total_rows > SET_LIST_ROWS) {
        const float sb_frac  = static_cast<float>(SET_LIST_ROWS) / static_cast<float>(total_rows);
        const float sb_top   = static_cast<float>(scroll) / static_cast<float>(total_rows);
        const int   sb_x     = lx + lw - 4;
        const int   sb_h     = static_cast<int>(sb_frac * SET_LIST_H);
        const int   sb_y     = py + static_cast<int>(sb_top * SET_LIST_H);
        DrawRectangle(sb_x, sb_y, 3, sb_h, COL_BORDER);
    }

    return clicked;
}

void AppUI::draw_settings(UIState& s) {
    const int px = m_width - PAD - SET_PANEL_W;
    const int py = m_height - FOOTER_H - SET_PANEL_H - 4;

    DrawRectangle(px - 4, py - 4, SET_PANEL_W + 8, SET_PANEL_H + 8,
                  {0, 0, 0, 120});

    DrawRectangleRounded({(float)px, (float)py, (float)SET_PANEL_W, (float)SET_PANEL_H},
                         0.04f, 4, COL_SURFACE);
    DrawRectangleRoundedLines({(float)px, (float)py, (float)SET_PANEL_W, (float)SET_PANEL_H},
                               0.04f, 4, COL_BORDER);

    const int title_y = py + 10;
    DrawText("Audio Device Settings", px + 12, title_y, FONT_LABEL, COL_TEXT);

    const int close_sz = 18;
    const int close_x  = px + SET_PANEL_W - close_sz - 8;
    const int close_y  = py + 8;
    DrawText("x", close_x + 4, close_y + 1, FONT_LABEL, COL_TEXT_DIM);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Rectangle close_rc = {(float)close_x, (float)close_y,
                                    (float)close_sz, (float)close_sz};
        if (CheckCollisionPointRec(GetMousePosition(), close_rc))
                m_settings_open = false;
    }

    int cy = py + 36;

    int new_cap = s.selected_capture;
    draw_device_list("Microphone (capture)", s.capture_devices,
                     s.selected_capture, m_scroll_capture,
                     px, cy, SET_PANEL_W, new_cap);
    if (new_cap != s.selected_capture) {
        s.selected_capture         = new_cap;
        s.device_selection_changed = true;
    }
    cy += SET_SECTION_H;

    int new_pb = s.selected_playback;
    draw_device_list("Speaker (local playback)", s.playback_devices,
                     s.selected_playback, m_scroll_playback,
                     px, cy, SET_PANEL_W, new_pb);
    if (new_pb != s.selected_playback) {
        s.selected_playback        = new_pb;
        s.device_selection_changed = true;
    }
    cy += SET_SECTION_H;

    int new_lb = s.selected_loopback;
    draw_device_list("Loopback output (e.g. VB-CABLE Input)", s.playback_devices,
                     s.selected_loopback, m_scroll_loopback,
                     px, cy, SET_PANEL_W, new_lb);
    if (new_lb != s.selected_loopback) {
        s.selected_loopback        = new_lb;
        s.device_selection_changed = true;
    }
    cy += SET_SECTION_H;

    {
        const int lx        = px + 8;
        const int lw        = SET_PANEL_W - 16;
        const int label_y   = cy + 2;

        char vol_label[48];
        snprintf(vol_label, sizeof(vol_label),
                 "Loopback volume: %.0f%%",
                 static_cast<double>(s.loopback_volume * 100.0f));
        DrawText(vol_label, lx, label_y, FONT_LABEL, COL_TEXT_DIM);

        const int track_y  = label_y + SET_HEADER_H;
        const int track_h  = 6;
        const int track_cx = track_y + track_h / 2;
        DrawRectangle(lx, track_cx - track_h / 2, lw, track_h, COL_BORDER);

        const float clamped_vol = std::clamp(s.loopback_volume, 0.0f, 2.0f);
        const int   fill_w     = static_cast<int>(clamped_vol / 2.0f * static_cast<float>(lw));
        DrawRectangle(lx, track_cx - track_h / 2, fill_w, track_h, COL_ACCENT_BLUE);

        const int knob_r  = 8;
        const int knob_cx = lx + fill_w;
        DrawCircle(knob_cx, track_cx, knob_r, COL_ACCENT_BLUE);
        DrawCircleLines(knob_cx, track_cx, knob_r, COL_TEXT);

        const Vector2    mp        = GetMousePosition();
        const Rectangle  track_rc  = { (float)lx, (float)(track_cx - knob_r),
                                        (float)lw, (float)(knob_r * 2) };
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, track_rc)) {
            const float ratio     = std::clamp((mp.x - static_cast<float>(lx))
                                               / static_cast<float>(lw), 0.0f, 1.0f);
            s.loopback_volume     = ratio * 2.0f;
            s.device_selection_changed = true;
        }
    }
}
