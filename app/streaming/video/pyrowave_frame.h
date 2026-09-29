#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PyroWave {
// P0 policy, including container overhead. Not a universal protocol limit.
constexpr std::size_t MaxFrameBytes = 850000;
constexpr std::size_t MaxPackets = 1024;

struct Packet {
    std::size_t offset;
    std::size_t size;
};

struct Frame {
    // Offsets into the caller-owned input; no payload copies or GPU work.
    std::vector<Packet> packets;
    std::size_t payloadBytes = 0;
    void clear() { packets.clear(); payloadBytes = 0; }
};

// Caller must keep input alive and unchanged while consuming the offsets.
// declaredSize is the reassembly/caller length; mismatch is rejected before parsing.
bool parseFrame(const std::uint8_t* data, std::size_t size,
                std::size_t declaredSize, Frame& frame, std::string& error);
}
