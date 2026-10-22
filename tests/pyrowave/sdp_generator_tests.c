// Real ANNOUNCE generation and RTSP roundtrip. Only environment/decoder queries
// are stubbed; SDP generation and RTSP parsing/serialization are production code.
#include "common_c_test_env.h"
#include "Rtsp.h"

static void announce(int format, const char* bitstream, int hevc) {
    RTSP_MESSAGE request, parsed;
    int length, wireLength;
    char contentLength[32];
    OPTION_ITEM option;
    char* sdp;
    char* wire;
    NegotiatedVideoFormat = format;
    sdp = getSdpPayloadForStreamConfig(14, &length);
    CHECK(sdp != NULL && (int)strlen(sdp) == length);
    CHECK(strstr(sdp, bitstream) != NULL);
    CHECK(strstr(sdp, "a=x-nv-video[0].dynamicRangeMode:0 \r\n") != NULL);
    CHECK(strstr(sdp, "a=x-ss-video[0].chromaSamplingType:0 \r\n") != NULL);
    CHECK(strstr(sdp, "pyrowave.features") == NULL && strstr(sdp, "adaptiveFec") == NULL);
    if (hevc >= 0)
        CHECK(strstr(sdp, hevc ? "a=x-nv-clientSupportHevc:1 \r\n" :
                                 "a=x-nv-clientSupportHevc:0 \r\n") != NULL);
    snprintf(contentLength, sizeof(contentLength), "%d", length);
    memset(&option, 0, sizeof(option));
    option.option = "Content-Length";
    option.content = contentLength;
    createRtspRequest(&request, NULL, 0, "ANNOUNCE", "rtsp://127.0.0.1:48010",
                      "RTSP/1.0", 5, &option, sdp, length);
    wire = serializeRtspMessage(&request, &wireLength);
    CHECK(wire != NULL && wire[wireLength] == '\0');
    CHECK(parseRtspMessage(&parsed, wire, wireLength) == 0);
    CHECK(parsed.type == TYPE_REQUEST && !strcmp(parsed.message.request.command, "ANNOUNCE"));
    CHECK(!strcmp(getOptionContent(parsed.options, "content-length"), contentLength));
    CHECK(parsed.payloadLength == length && !memcmp(parsed.payload, sdp, length));
    freeMessage(&parsed);
    free(wire);
    free(sdp);
}

int main(void) {
    RemoteAddr.ss_family = AF_INET;
    StreamConfig.width = 1920; StreamConfig.height = 1080; StreamConfig.fps = 60;
    StreamConfig.bitrate = 20000; StreamConfig.packetSize = 1392;
    StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
    StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    StreamConfig.colorSpace = COLORSPACE_REC_709; StreamConfig.colorRange = COLOR_RANGE_LIMITED;
    announce(VIDEO_FORMAT_PYROWAVE, "a=x-nv-vqos[0].bitStreamFormat:3 \r\n", -1);
    announce(VIDEO_FORMAT_AV1_MAIN8, "a=x-nv-vqos[0].bitStreamFormat:2 \r\n", -1);
    announce(VIDEO_FORMAT_H265, "a=x-nv-vqos[0].bitStreamFormat:1 \r\n", 1);
    announce(VIDEO_FORMAT_H264, "a=x-nv-vqos[0].bitStreamFormat:0 \r\n", 0);
    puts("PASS: production PyroWave/AV1/HEVC/H.264 ANNOUNCE and real RTSP roundtrip");
    return 0;
}
