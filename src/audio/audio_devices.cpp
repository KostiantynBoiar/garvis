#include "audio_devices.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

static std::vector<AudioDeviceInfo> enumerate(ma_device_type type) {
    std::vector<AudioDeviceInfo> result;

    ma_context ctx;
    if (ma_context_init(nullptr, 0, nullptr, &ctx) != MA_SUCCESS) {
        fprintf(stderr, "[audio_devices] Failed to init ma_context\n");
        return result;
    }

    ma_device_info* infos = nullptr;
    ma_uint32        count = 0;

    ma_result rc = (type == ma_device_type_capture)
        ? ma_context_get_devices(&ctx, nullptr, nullptr, &infos, &count)
        : ma_context_get_devices(&ctx, &infos, &count, nullptr, nullptr);

    if (rc == MA_SUCCESS && infos && count > 0) {
        result.reserve(count);
        for (ma_uint32 i = 0; i < count; ++i) {
            AudioDeviceInfo dev;
            dev.name = infos[i].name;
            dev.id   = infos[i].id;
            result.push_back(dev);
        }
    }

    ma_context_uninit(&ctx);
    return result;
}

std::vector<AudioDeviceInfo> enumerate_capture_devices() {
    return enumerate(ma_device_type_capture);
}

std::vector<AudioDeviceInfo> enumerate_playback_devices() {
    return enumerate(ma_device_type_playback);
}

static bool name_matches_virtual_cable(const std::string& device_name) {
    static const char* keywords[] = {
        "cable input", "voicemeeter input", "virtual cable",
        "virtual audio", "blackhole", "vb-audio",
    };

    std::string lower = device_name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const char* kw : keywords) {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
}

bool is_virtual_cable(const std::string& name) {
    return name_matches_virtual_cable(name);
}

int find_virtual_cable(const std::vector<AudioDeviceInfo>& devs) {
    for (size_t i = 0; i < devs.size(); ++i) {
        if (name_matches_virtual_cable(devs[i].name))
            return static_cast<int>(i);
    }
    return -1;
}

int find_real_speaker(const std::vector<AudioDeviceInfo>& devs) {
    for (size_t i = 0; i < devs.size(); ++i) {
        if (!name_matches_virtual_cable(devs[i].name))
            return static_cast<int>(i);
    }
    return -1;
}
