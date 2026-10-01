#pragma once
#include <cstdint>
#include <vector>

namespace PyroWave {
struct Pixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> planes[3]; // tightly packed I420: Y, U, V
};

// This offline qualification contract deliberately accepts only 1080p 4:2:0.
inline bool validProofPixels(const Pixels& p) {
    return p.width == 1920 && p.height == 1080 &&
        p.planes[0].size() == 2073600 && p.planes[1].size() == 518400 &&
        p.planes[2].size() == 518400;
}
}
