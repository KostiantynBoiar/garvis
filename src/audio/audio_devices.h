#pragma once
#include "miniaudio.h"

#include <string>
#include <vector>

struct AudioDeviceInfo {
    std::string  name;
    ma_device_id id;
};

std::vector<AudioDeviceInfo> enumerate_capture_devices();
std::vector<AudioDeviceInfo> enumerate_playback_devices();
int find_virtual_cable(const std::vector<AudioDeviceInfo>& devs);
bool is_virtual_cable(const std::string& name);
int find_real_speaker(const std::vector<AudioDeviceInfo>& devs);
