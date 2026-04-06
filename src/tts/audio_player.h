#pragma once
#include "miniaudio.h"

#include <cstdint>
#include <vector>

void play_pcm(const std::vector<int16_t>& samples,
              uint32_t                    sample_rate,
              const ma_device_id*         speaker_id     = nullptr,
              float                       speaker_volume = 3.0f);
