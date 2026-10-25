// Shared test environment for production RTSP/SDP code.
#pragma once
#include "Limelight-internal.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

struct sockaddr_storage RemoteAddr;
int AppVersionQuad[4] = {7, 1, 500, -1};
STREAM_CONFIGURATION StreamConfig;
CONNECTION_LISTENER_CALLBACKS ListenerCallbacks;
DECODER_RENDERER_CALLBACKS VideoCallbacks;
AUDIO_RENDERER_CALLBACKS AudioCallbacks;
int NegotiatedVideoFormat;
bool HighQualitySurroundSupported, HighQualitySurroundEnabled, AudioEncryptionEnabled;
int AudioPacketDuration;
uint16_t RtspPortNumber = 48010, VideoPortNumber = 47998;
uint32_t EncryptionFeaturesSupported, EncryptionFeaturesRequested, EncryptionFeaturesEnabled;

bool isReferenceFrameInvalidationSupportedByDecoder(void) { return false; }
bool PltSafeStrcpy(char* dest, size_t size, const char* src) {
    CHECK(strlen(src) < size);
    strcpy(dest, src);
    return true;
}
void addrToUrlSafeString(struct sockaddr_storage* addr, char* buffer, size_t size) {
    (void)addr;
    snprintf(buffer, size, "127.0.0.1");
}
