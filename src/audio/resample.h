#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

inline std::vector<int16_t> resample_s16(const std::vector<int16_t>& src,
                                          uint32_t src_rate, uint32_t dst_rate)
{
    if (src_rate == dst_rate || src.empty()) return src;

    const double ratio   = static_cast<double>(dst_rate) / static_cast<double>(src_rate);
    const size_t out_len = static_cast<size_t>(std::ceil(src.size() * ratio));
    std::vector<int16_t> out(out_len);

    for (size_t i = 0; i < out_len; ++i) {
        const double src_idx = i / ratio;
        const size_t idx0 = static_cast<size_t>(src_idx);
        const size_t idx1 = std::min(idx0 + 1, src.size() - 1);
        const double frac = src_idx - static_cast<double>(idx0);
        const double val  = src[idx0] * (1.0 - frac) + src[idx1] * frac;
        out[i] = static_cast<int16_t>(std::clamp(val, -32768.0, 32767.0));
    }
    return out;
}
