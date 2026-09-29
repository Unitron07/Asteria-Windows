#include "pyrowave_frame.h"

#include <cstring>
#include <new>

namespace PyroWave {
bool parseFrame(const std::uint8_t* data, std::size_t size,
                std::size_t declaredSize, Frame& frame, std::string& error)
{
    frame.clear();
    error.clear();
    auto fail = [&](const char* reason) { frame.clear(); error = reason; return false; };
    if (size != declaredSize) return fail("reassembled length does not match declared frame size");
    if (size > MaxFrameBytes) return fail("frame exceeds P0 safety limit");
    if (!data || size < 8) return fail("truncated frame header");
    if (std::memcmp(data, "PYRW", 4) != 0) return fail("bad PYRW magic");
    if (data[4] != 1) return fail("unsupported PYRW version");
    if (data[7] != 0) return fail("nonzero reserved byte");
    const std::size_t count = (std::size_t(data[5]) << 8) | data[6];
    if (!count || count > MaxPackets) return fail("packet count outside P0 safety limit");
    if (count > (size - 8) / 5) return fail("packet count cannot fit in frame");

    // Validate the entire container before allocating even the bounded offset table.
    std::size_t cursor = 8;
    std::size_t payload = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (size - cursor < 4) return fail("truncated packet length");
        const std::uint32_t length = (std::uint32_t(data[cursor]) << 24) |
            (std::uint32_t(data[cursor + 1]) << 16) |
            (std::uint32_t(data[cursor + 2]) << 8) | data[cursor + 3];
        cursor += 4;
        if (!length) return fail("zero packet length");
        // Subtraction check avoids overflowing cursor + length, even on 32-bit.
        if (length > size - cursor) return fail("truncated packet payload or pathological length");
        cursor += length;
        payload += length; // <= size, already bounded to MaxFrameBytes.
    }
    if (cursor != size) return fail("trailing bytes or packet count mismatch");
    if (payload != size - 8 - count * 4) return fail("packet length sum mismatch");

    try {
        frame.packets.reserve(count);
        cursor = 8;
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t length = (std::uint32_t(data[cursor]) << 24) |
                (std::uint32_t(data[cursor + 1]) << 16) |
                (std::uint32_t(data[cursor + 2]) << 8) | data[cursor + 3];
            cursor += 4;
            frame.packets.push_back({cursor, length});
            cursor += length;
        }
        frame.payloadBytes = payload;
    } catch (const std::bad_alloc&) {
        return fail("packet table allocation failed");
    }
    return true;
}
}
