// Run the complete production TCP handshake against an in-memory host. No socket,
// encryption, ENet, pairing or GPU implementation is exercised by this fixture.
#include "common_c_test_env.h"

SOCKADDR_LEN AddrLen = sizeof(struct sockaddr_in);
volatile bool ConnectionInterrupted;
bool ReferenceFrameInvalidationSupported;
uint16_t AudioPortNumber, ControlPortNumber;
SS_PING AudioPingPayload, VideoPingPayload;
uint32_t ControlConnectData, SunshineFeatureFlags;
OPUS_MULTISTREAM_CONFIGURATION NormalQualityOpusConfig, HighQualityOpusConfig;

static const char* hostSdp;
static const char* expectedBitstream;
static char reply[4096];
static int replyLength, replyOffset, exchanges, announces, openSockets, cryptoContexts;
static int fragmentSize = 7;
static int fixtureRecv(SOCKET socket, char* buffer, int size, int flags) {
    int count = replyLength - replyOffset;
    (void)socket; (void)flags;
    if (count > size) count = size;
    if (count > fragmentSize) count = fragmentSize;
    memcpy(buffer, reply + replyOffset, count);
    replyOffset += count;
    return count;
}
// Redirect just the OS receive operation after platform declarations are loaded.
#define recv fixtureRecv
#include "RtspConnection.c"
#undef recv

SOCKET connectTcpSocket(struct sockaddr_storage* addr, SOCKADDR_LEN len, unsigned short port, int timeout) {
    (void)addr; (void)len; (void)port; (void)timeout;
    CHECK(openSockets == 0);
    ++openSockets;
    return (SOCKET)42;
}
void closeSocket(SOCKET socket) { (void)socket; CHECK(openSockets == 1); --openSockets; }
int enableNoDelay(SOCKET socket) { (void)socket; return 0; }
int pollSockets(struct pollfd* fds, int count, int timeout) { (void)fds; (void)count; (void)timeout; return 1; }
void PltSleepMs(int ms) { (void)ms; CHECK(false); }
int notifyAudioPortNegotiationComplete(void) { return 0; }
void* extendBuffer(void* ptr, size_t size) { void* p = realloc(ptr, size); CHECK(p); return p; }

int sendMtuSafe(SOCKET socket, char* buffer, int size) {
    RTSP_MESSAGE request;
    const char* payload = "";
    const char* extra = "";
    const char* session;
    (void)socket;
    CHECK(parseRtspMessage(&request, buffer, size) == 0);
    ++exchanges;
    CHECK(request.sequenceNumber == exchanges);
    if (!strcmp(request.message.request.command, "DESCRIBE")) payload = hostSdp;
    if (!strcmp(request.message.request.command, "SETUP")) {
        extra = "sEsSiOn:\tDEADBEEF;timeout=90 \t\r\n"
                "tRaNsPoRt:unicast;server_port=48001-48002\r\n";
    }
    session = getOptionContent(request.options, "Session");
    if (exchanges > 3) CHECK(session && !strcmp(session, "DEADBEEF"));
    if (!strcmp(request.message.request.command, "ANNOUNCE")) {
        CHECK(request.payload && strstr(request.payload, expectedBitstream));
        CHECK(!strstr(request.payload, "pyrowave.features") && !strstr(request.payload, "adaptiveFec"));
        ++announces;
    }
    // Mixed-case names and no space after ':' exercise the new parser at each
    // actual handshake stage, including Session extraction and Transport parsing.
    replyLength = snprintf(reply, sizeof(reply), "RTSP/1.0 200 OK\r\ncSeQ:%d\r\n%s"
                           "content-length:%d\r\n\r\n%s", exchanges, extra, (int)strlen(payload), payload);
    CHECK(replyLength > 0 && replyLength < (int)sizeof(reply));
    replyOffset = 0;
    freeMessage(&request);
    return size;
}

PPLT_CRYPTO_CONTEXT PltCreateCryptoContext(void) { ++cryptoContexts; return calloc(1, sizeof(PLT_CRYPTO_CONTEXT)); }
void PltDestroyCryptoContext(PPLT_CRYPTO_CONTEXT ctx) { CHECK(ctx); --cryptoContexts; free(ctx); }
bool PltEncryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
    unsigned char* key, int keyLength, unsigned char* iv, int ivLength,
    unsigned char* tag, int tagLength, unsigned char* input, int inputLength,
    unsigned char* output, int* outputLength) { CHECK(false); return false; }
bool PltDecryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
    unsigned char* key, int keyLength, unsigned char* iv, int ivLength,
    unsigned char* tag, int tagLength, unsigned char* input, int inputLength,
    unsigned char* output, int* outputLength) { CHECK(false); return false; }

// Modern Sunshine/Vibepollo uses TCP. Fail if a fixture accidentally enters ENet.
int enet_address_set_address(ENetAddress* a, struct sockaddr* b, socklen_t c) { CHECK(false); return -1; }
int enet_address_set_port(ENetAddress* a, enet_uint16 b) { CHECK(false); return -1; }
ENetHost* enet_host_create(int a, const ENetAddress* b, size_t c, size_t d, enet_uint32 e, enet_uint32 f) { CHECK(false); return NULL; }
ENetPeer* enet_host_connect(ENetHost* a, const ENetAddress* b, size_t c, enet_uint32 d) { CHECK(false); return NULL; }
void enet_host_destroy(ENetHost* a) { CHECK(false); }
void enet_host_flush(ENetHost* a) { CHECK(false); }
ENetPacket* enet_packet_create(const void* a, size_t b, enet_uint32 c) { CHECK(false); return NULL; }
void enet_packet_destroy(ENetPacket* a) { CHECK(false); }
void enet_peer_disconnect_now(ENetPeer* a, enet_uint32 b) { CHECK(false); }
void enet_peer_reset(ENetPeer* a) { CHECK(false); }
int enet_peer_send(ENetPeer* a, enet_uint8 b, ENetPacket* c) { CHECK(false); return -1; }
int serviceEnetHost(ENetHost* a, ENetEvent* b, enet_uint32 c) { CHECK(false); return -1; }

static void handshake(int formats, int scm, const char* sdp, int expected, int negotiated, const char* bitstream) {
    SERVER_INFORMATION host = {0};
    host.serverCodecModeSupport = scm;
    exchanges = announces = 0;
    AudioPortNumber = VideoPortNumber = ControlPortNumber = 0;
    NegotiatedVideoFormat = 0;
    StreamConfig.supportedVideoFormats = formats;
    hostSdp = sdp;
    expectedBitstream = bitstream;
    CHECK(performRtspHandshake(&host) == expected);
    CHECK(openSockets == 0 && cryptoContexts == 0);
    if (!expected) {
        CHECK(exchanges == 7 && announces == 1 && NegotiatedVideoFormat == negotiated);
        CHECK(AudioPortNumber == 48001 && VideoPortNumber == 48001 && ControlPortNumber == 48001);
    }
    else CHECK(exchanges == 2 && announces == 0 && NegotiatedVideoFormat == 0);
}

int main(void) {
    const char* pyro = "v=0\r\na=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:186f0393\r\n";
    const char* av1 = "v=0\r\na=rtpmap:98 AV1/90000\r\n";
    const char* hevc = "v=0\r\na=fmtp:96 sprop-parameter-sets=AAAAAUabcdef\r\n";
    int standard = VIDEO_FORMAT_AV1_MAIN8 | VIDEO_FORMAT_H265 | VIDEO_FORMAT_H264;
    RemoteAddr.ss_family = AF_INET;
    StreamConfig.width = 1920; StreamConfig.height = 1080; StreamConfig.fps = 60;
    StreamConfig.bitrate = 20000; StreamConfig.packetSize = 1392;
    StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
    StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    StreamConfig.colorSpace = COLORSPACE_REC_709; StreamConfig.colorRange = COLOR_RANGE_LIMITED;
    handshake(VIDEO_FORMAT_PYROWAVE, SCM_PYROWAVE, pyro, 0, VIDEO_FORMAT_PYROWAVE, "bitStreamFormat:3 \r\n");
    fragmentSize = 1;
    handshake(standard, SCM_MASK_AV1 | SCM_MASK_HEVC | SCM_MASK_H264 | SCM_PYROWAVE,
              av1, 0, VIDEO_FORMAT_AV1_MAIN8, "bitStreamFormat:2 \r\n");
    handshake(standard, SCM_MASK_HEVC | SCM_MASK_H264, hevc, 0, VIDEO_FORMAT_H265, "bitStreamFormat:1 \r\n");
    handshake(standard, SCM_MASK_H264, "v=0\r\na=rtpmap:96 H264/90000\r\n",
              0, VIDEO_FORMAT_H264, "bitStreamFormat:0 \r\n");
    // Explicit standard choice cannot be overridden by a PyroWave host offer.
    handshake(VIDEO_FORMAT_H264, SCM_PYROWAVE | SCM_MASK_H264, pyro,
              0, VIDEO_FORMAT_H264, "bitStreamFormat:0 \r\n");
    handshake(VIDEO_FORMAT_PYROWAVE, SCM_MASK_AV1, pyro, ML_ERROR_PYROWAVE_SCM, 0, NULL);
    handshake(VIDEO_FORMAT_PYROWAVE, SCM_PYROWAVE, av1, ML_ERROR_PYROWAVE_RTPMAP, 0, NULL);
    handshake(VIDEO_FORMAT_PYROWAVE | VIDEO_FORMAT_AV1_MAIN8, SCM_PYROWAVE, pyro,
              ML_ERROR_PYROWAVE_PROFILE, 0, NULL);
    handshake(VIDEO_FORMAT_PYROWAVE, SCM_PYROWAVE,
              "a=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:deadbeef\r\n",
              ML_ERROR_PYROWAVE_MISMATCH, 0, NULL);
    handshake(VIDEO_FORMAT_PYROWAVE, SCM_PYROWAVE,
              "a=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:186f0393\r\na=x-ss-pyrowave.bitstream:186f0393\r\n",
              ML_ERROR_PYROWAVE_BITSTREAM, 0, NULL);
    // A normal reconnect after failed PyroWave negotiation must still succeed.
    handshake(standard, SCM_MASK_AV1, av1, 0, VIDEO_FORMAT_AV1_MAIN8, "bitStreamFormat:2 \r\n");
    puts("PASS: production TCP handshake, PyroWave/AV1/HEVC/H.264 selection, rejection and reconnect (mock transport)");
    return 0;
}
