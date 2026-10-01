#include "pyrowave_frame.h"

#include <cstring>
#include <new>
#include <algorithm>

namespace PyroWave {
bool parseLegacyOfflineFrame(const std::uint8_t* data, std::size_t size,
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
    frame.framing = Framing::LegacyOffline;
    return true;
}

namespace {
// Decode bytes explicitly: no alignment, host endian or C++ bitfield assumptions.
std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
bool reject(Frame& frame, std::string& error, const char* reason) {
    frame.clear(); error = reason; return false;
}
bool envelope(const std::uint8_t* data, std::size_t size, std::size_t declared,
              Frame& frame, std::string& error) {
    frame.clear(); error.clear();
    if (size != declared) return reject(frame,error,"reassembled length mismatch");
    if (size > MaxFrameBytes) return reject(frame,error,"frame exceeds P0 safety limit");
    if (!data || size < 4) return reject(frame,error,"truncated framing word");
    return true;
}
// Mirrors WaveletBuffers::init_block_meta at CodecCommit: five levels, 32x32
// blocks, minimum aligned image 128, omit level-zero Cb/Cr in 420 mode.
std::uint32_t blockCapacity(const Sequence& s) {
    const auto w = std::max(128u,(s.width + 31u) & ~31u);
    const auto h = std::max(128u,(s.height + 31u) & ~31u);
    std::uint32_t count = 0;
    for (unsigned level = 0; level < 5; ++level) {
        const auto x = ((w >> (level + 1)) + 31) / 32;
        const auto y = ((h >> (level + 1)) + 31) / 32;
        const auto components = level == 0 && s.chroma == Chroma::Yuv420 ? 1u : 3u;
        count += x * y * components * (level == 4 ? 4u : 3u);
    }
    return count; // widths/heights are 14-bit + 1, arithmetic fits u32
}

bool validBlockPayload(const std::uint8_t* p, std::size_t bytes,
                       const Sequence& s, std::uint32_t index) {
    // Bounds-checked equivalent of the pinned encoder's validate_bitstream().
    // Find this block's partial-edge 8x8 mapping without an image-sized table.
    const auto w = std::max(128u,(s.width + 31u) & ~31u);
    const auto h = std::max(128u,(s.height + 31u) & ~31u);
    unsigned bw = 0, bh = 0;
    bool found = false;
    for (int level = 4; level >= 0 && !found; --level) {
        const auto x8 = ((w >> (level + 1)) + 7) / 8;
        const auto y8 = ((h >> (level + 1)) + 7) / 8;
        const auto x32 = (x8 + 3) / 4, y32 = (y8 + 3) / 4;
        const auto groups = (level == 0 && s.chroma == Chroma::Yuv420 ? 1u : 3u) *
                            (level == 4 ? 4u : 3u);
        const auto groupSize = x32 * y32;
        if (index >= groupSize * groups) { index -= groupSize * groups; continue; }
        const auto local = index % groupSize;
        bw = std::min(4u,x8 - 4 * (local % x32));
        bh = std::min(4u,y8 - 4 * (local / x32));
        found = true;
    }
    if (!found) return false;
    const auto ballot = le32(p) & 0xffffu;
    unsigned active = 0;
    for (unsigned bit = 0; bit < 16; ++bit) if (ballot & (1u << bit)) {
        if ((bit & 3u) >= bw || (bit >> 2) >= bh) return false;
        ++active;
    }
    if (!active || 8 + active * 3 > bytes) return false;
    std::size_t cursor = 8 + active * 3;
    unsigned signs = 0;
    for (unsigned i = 0; i < active; ++i) {
        const auto control = unsigned(p[8 + 2*i]) | (unsigned(p[9 + 2*i]) << 8);
        const auto q = p[8 + 2*active + i] & 15u;
        for (unsigned shift = 0; shift < 16; shift += 2) {
            const auto planes = q + ((control >> shift) & 3u);
            if (planes > bytes - cursor) return false;
            unsigned significance = 0;
            for (unsigned n = 0; n < planes; ++n) significance |= p[cursor++];
            for (unsigned bit = 0; bit < 8; ++bit) signs += (significance >> bit) & 1u;
        }
    }
    const auto signBytes = (signs + 7) / 8;
    if (signBytes > bytes - cursor) return false;
    cursor += signBytes;
    return ((cursor + 3) & ~std::size_t(3)) == bytes;
}

// Spans contain codec packets (compatibility) or the entire assembled record
// frame. No RTP payload boundaries are consulted. Padding is never sent to C API.
bool scanRecords(const std::uint8_t* data, const std::vector<Packet>& spans,
                 Frame& frame, std::string& error, const StreamContext* context,
                 bool collect) {
    Sequence sequence;
    bool haveSequence = false;
    std::size_t blocks = 0, records = 0;
    for (const auto& span : spans) {
        std::size_t cursor = span.offset;
        const auto end = span.offset + span.size; // spans already bounds-checked
        while (cursor < end) {
            if (end - cursor < 8) return reject(frame,error,"truncated record header");
            const auto a = le32(data + cursor), b = le32(data + cursor + 4);
            std::size_t bytes = 8;
            RecordKind kind;
            std::uint32_t index = 0;
            if (a == 0xffffffffu) {
                if (!haveSequence) return reject(frame,error,"padding before sequence header");
                if (b > (end - cursor - 8) / 4) return reject(frame,error,"padding runs off frame");
                bytes += std::size_t(b) * 4;
                for (std::size_t p = cursor + 8; p < cursor + bytes; p += 4)
                    if (le32(data + p)) return reject(frame,error,"nonzero padding word");
                kind = RecordKind::Padding;
            } else if (a & 0x80000000u) {
                if (haveSequence) return reject(frame,error,"second sequence header");
                if ((b >> 24) & 3u) return reject(frame,error,"unsupported sequence code");
                sequence.width = (a & 0x3fffu) + 1;
                sequence.height = ((a >> 14) & 0x3fffu) + 1;
                sequence.number = std::uint8_t((a >> 28) & 7u);
                sequence.totalBlocks = b & 0xffffffu;
                sequence.chroma = b & (1u << 26) ? Chroma::Yuv444 : Chroma::Yuv420;
                sequence.blockCapacity = blockCapacity(sequence);
                if (sequence.totalBlocks > sequence.blockCapacity ||
                    sequence.totalBlocks > MaxRecords)
                    return reject(frame,error,"impossible total block count");
                if (context && (context->width != sequence.width || context->height != sequence.height ||
                                context->chroma != sequence.chroma))
                    return reject(frame,error,"negotiated extent or chroma mismatch");
                // Color primaries, transfer and matrix bits; range/siting can vary in SDR.
                if (context && context->requireSdr && (b & (7u << 27)))
                    return reject(frame,error,"P0 requires SDR BT.709 metadata");
                haveSequence = true;
                kind = RecordKind::Sequence;
            } else {
                if (!haveSequence) return reject(frame,error,"block before sequence header");
                const auto words = (a >> 16) & 0xfffu;
                if (words < 2) return reject(frame,error,"payload_words smaller than block header");
                // Vibepollo sends no conditional header-only zero blocks.
                if (words == 2) return reject(frame,error,"header-only block is not a complete-frame record");
                bytes = std::size_t(words) * 4;
                if (bytes > end - cursor) return reject(frame,error,"block runs off frame");
                if (((a >> 28) & 7u) != sequence.number)
                    return reject(frame,error,"block sequence mismatch");
                index = b >> 8;
                if (index >= sequence.blockCapacity) return reject(frame,error,"impossible block index");
                if (!validBlockPayload(data + cursor,bytes,sequence,index))
                    return reject(frame,error,"malformed block ballot or coefficient payload");
                if (++blocks > sequence.totalBlocks) return reject(frame,error,"excess block records");
                kind = RecordKind::Block;
            }
            if (++records > MaxRecords) return reject(frame,error,"record count exceeds safety limit");
            if (collect) frame.records.push_back({kind,cursor,bytes,index});
            cursor += bytes;
        }
    }
    if (!haveSequence || blocks != sequence.totalBlocks)
        return reject(frame,error,"incomplete frame block count");
    frame.sequence = sequence;
    return true;
}
bool validateRecords(const std::uint8_t* data, const std::vector<Packet>& spans,
                     Frame& frame, std::string& error, const StreamContext* context) {
    // Validate lengths/counts before allocating record metadata. Every allocation
    // is bounded by frame bytes; never reserve from an untrusted total_blocks.
    if (!scanRecords(data,spans,frame,error,context,false)) return false;
    if (!scanRecords(data,spans,frame,error,context,true)) return false;
    std::vector<std::uint32_t> indices;
    for (const auto& r : frame.records)
        if (r.kind == RecordKind::Block) indices.push_back(r.blockIndex);
    std::sort(indices.begin(),indices.end());
    if (std::adjacent_find(indices.begin(),indices.end()) != indices.end())
        return reject(frame,error,"duplicate block index");
    return true;
}
}

bool parseCompatibilityFrame(const std::uint8_t* data, std::size_t size,
                             std::size_t declaredSize, Frame& frame, std::string& error) {
    if (!envelope(data,size,declaredSize,frame,error)) return false;
    const auto count = le32(data);
    if (!count || count > MaxPackets) return reject(frame,error,"packet count outside P0 safety limit");
    if (count > (size - 4) / 5) return reject(frame,error,"packet count cannot fit in frame");
    std::size_t cursor = 4, payload = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (size - cursor < 4) return reject(frame,error,"truncated packet length");
        const auto n = le32(data + cursor); cursor += 4;
        if (!n) return reject(frame,error,"zero packet length");
        if (n > size - cursor) return reject(frame,error,"truncated packet data or pathological length");
        cursor += n; payload += n; // <= bounded input size
    }
    if (cursor != size) return reject(frame,error,"trailing bytes or packet count mismatch");
    try {
        frame.packets.reserve(count);
        cursor = 4;
        for (std::size_t i = 0; i < count; ++i) {
            const auto n = le32(data + cursor); cursor += 4;
            frame.packets.push_back({cursor,n}); cursor += n;
        }
        frame.payloadBytes = payload;
    } catch (const std::bad_alloc&) { return reject(frame,error,"packet metadata allocation failed"); }
    return true;
}

bool parseRecordFrame(const std::uint8_t* data, std::size_t size,
                      std::size_t declaredSize, Frame& frame, std::string& error,
                      const StreamContext* context) {
    if (!envelope(data,size,declaredSize,frame,error)) return false;
    try {
        if (!validateRecords(data,{{0,size}},frame,error,context)) return false;
        for (const auto& r : frame.records) {
            if (r.kind == RecordKind::Padding) continue;
            frame.packets.push_back({r.offset,r.size});
            frame.payloadBytes += r.size;
        }
        frame.framing = Framing::Records;
    } catch (const std::bad_alloc&) { return reject(frame,error,"record metadata allocation failed"); }
    return true;
}

bool parseFrame(const std::uint8_t* data, std::size_t size,
                std::size_t declaredSize, Frame& frame, std::string& error,
                const StreamContext* context) {
    if (!envelope(data,size,declaredSize,frame,error)) return false;
    if (le32(data) & 0x80000000u)
        return parseRecordFrame(data,size,declaredSize,frame,error,context);
    if (!parseCompatibilityFrame(data,size,declaredSize,frame,error)) return false;
    if (context) {
        try {
            if (!validateRecords(data,frame.packets,frame,error,context)) return false;
        } catch (const std::bad_alloc&) { return reject(frame,error,"record metadata allocation failed"); }
    }
    return true;
}
}
