#pragma once
#include "miniaudio.h"

#include <cstdint>
#include <vector>

// Play a mono s16le PCM buffer on a single speaker device.
// Blocks until all samples have been played.
//
//   speaker_id     — target speaker device; nullptr uses the OS default.
//   speaker_volume — linear gain for local speaker (default 3.0 = 3x boost).
void play_pcm(const std::vector<int16_t>& samples,
              uint32_t                    sample_rate,
              const ma_device_id*         speaker_id     = nullptr,
              float                       speaker_volume = 3.0f);
