#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PyroWave {
// P0 policy, including container overhead. Not a universal protocol limit.
constexpr std::size_t MaxFrameBytes = 850000;
constexpr std::size_t MaxPackets = 1024;
constexpr std::size_t MaxRecords = MaxFrameBytes / 8;
constexpr char BitstreamId[] = "186f0393";
constexpr char CodecCommit[] = "186f0393b77f7755953b5ecde994bb1cec2e4155";

// Live compatibility transport: 4000 payloads at up to 2048-byte negotiated MTU
// fit below 8 MiB. Codec packets are not transport packets; allow up to 65536.
struct Limits { std::size_t frameBytes, packets; };
constexpr Limits OfflineLimits{MaxFrameBytes, MaxPackets};
constexpr Limits LiveLimits{8 * 1024 * 1024, 65536};

enum class Framing { Compatibility, Records, LegacyOffline };
enum class RecordKind { Sequence, Block, Padding };
enum class Chroma { Yuv420, Yuv444 };
enum class YuvRange { Full, Limited };

struct StreamContext {
    std::uint32_t width;
    std::uint32_t height;
    Chroma chroma;
    bool requireSdr = false;
};

struct Sequence {
    std::uint32_t width = 0, height = 0, totalBlocks = 0, blockCapacity = 0;
    std::uint8_t number = 0;
    Chroma chroma = Chroma::Yuv420;
    YuvRange range = YuvRange::Full;
};

struct Record {
    RecordKind kind;
    std::size_t offset, size;
    std::uint32_t blockIndex = 0; // meaningful for Block only
};

// Future P1b input contract, deliberately unused by the full-frame parser.
// Transport supplies byte ranges after FEC, preserving original frame offsets.
// No zero-filled lost bytes may be passed as a complete frame.
struct PayloadMetadata {
    std::size_t offset, size;
    bool lost = false; // BUFFER_TYPE_LOST, after parity recovery
    bool recordStart = false; // BUFFER_TYPE_RECORD_START
};
struct RecoveryMetadata {
    std::vector<PayloadMetadata> payloads;
    std::uint32_t criticalPackets = 0;
};

struct Packet {
    std::size_t offset;
    std::size_t size;
};

struct Frame {
    // Offsets into the caller-owned input; no payload copies or GPU work.
    std::vector<Packet> packets;
    std::vector<Record> records; // includes padding; packets exclude it
    std::vector<std::uint32_t> blockIndices; // Reusable duplicate-validation scratch.
    Sequence sequence;
    Framing framing = Framing::Compatibility;
    std::size_t payloadBytes = 0;
    void clear() { packets.clear(); records.clear(); blockIndices.clear(); sequence = {}; payloadBytes = 0;
                   framing = Framing::Compatibility; }
};

// Caller must keep input alive and unchanged while consuming the offsets.
// declaredSize is the reassembly/caller length; mismatch is rejected before parsing.
bool parseFrame(const std::uint8_t* data, std::size_t size,
                std::size_t declaredSize, Frame& frame, std::string& error,
                const StreamContext* context = nullptr);
bool parseCompatibilityFrame(const std::uint8_t* data, std::size_t size,
                             std::size_t declaredSize, Frame& frame, std::string& error);
bool parseRecordFrame(const std::uint8_t* data, std::size_t size,
                      std::size_t declaredSize, Frame& frame, std::string& error,
                      const StreamContext* context = nullptr);
// P1a only: compatibility envelope plus complete codec-record validation.
// Accepts BT.709 full/limited SDR; rejects record framing, extent/chroma changes and HDR.
bool parseLiveCompatibilityFrame(const std::uint8_t* data, std::size_t size,
                                 std::size_t declaredSize, Frame& frame,
                                 std::string& error, const StreamContext& context);
// Historical regression fixtures only. Never selected by parseFrame detection.
bool parseLegacyOfflineFrame(const std::uint8_t* data, std::size_t size,
                             std::size_t declaredSize, Frame& frame, std::string& error);
}
