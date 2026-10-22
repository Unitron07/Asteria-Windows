// Compile the real upstream parser with allocation accounting. This catches the
// duplicate-option ownership regression without requiring a platform leak tool.
#include "Platform.h"
#include "PyroWave.h"
#include <errno.h>
#include <limits.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static size_t allocations;
static void* countedMalloc(size_t size) {
    void* p = malloc(size);
    CHECK(p != NULL);
    ++allocations;
    return p;
}
static void countedFree(void* p) {
    if (p) {
        CHECK(allocations > 0);
        --allocations;
        free(p);
    }
}
#define malloc countedMalloc
#define free countedFree
#include "RtspParser.c"
#undef malloc
#undef free

static const char matching[] =
    "v=0\r\na=rtpmap:99 PYROWAVE/90000\r\n"
    "a=x-ss-pyrowave.bitstream:186f0393\r\n";

static void describe(const char* headers, const char* sdp, int scm, int expected) {
    char wire[2048];
    RTSP_MESSAGE response, reparsed;
    PYROWAVE_SDP_INFO info;
    int serializedLength;
    char* serialized;
    int length = snprintf(wire, sizeof(wire), "%s%s", headers, sdp);
    CHECK(length > 0 && length < (int)sizeof(wire));
    CHECK(parseRtspMessage(&response, wire, length) == RTSP_ERROR_SUCCESS);
    CHECK(response.type == TYPE_RESPONSE && response.message.response.statusCode == 200);
    CHECK(response.sequenceNumber == 2);
    CHECK(response.payloadLength == (int)strlen(sdp));
    CHECK(memcmp(response.payload, sdp, strlen(sdp)) == 0);
    CHECK(validatePyroWaveSdp(VIDEO_FORMAT_PYROWAVE, scm, response.payload,
                             response.payloadLength, &info) == expected);
    serialized = serializeRtspMessage(&response, &serializedLength);
    CHECK(serialized != NULL && serialized[serializedLength] == '\0');
    CHECK(parseRtspMessage(&reparsed, serialized, serializedLength) == RTSP_ERROR_SUCCESS);
    CHECK(reparsed.payloadLength == response.payloadLength);
    CHECK(memcmp(reparsed.payload, response.payload, response.payloadLength) == 0);
    freeMessage(&reparsed);
    countedFree(serialized);
    freeMessage(&response);
    CHECK(allocations == 0);
}

static void malformed(const char* wire, int length) {
    RTSP_MESSAGE response;
    CHECK(parseRtspMessage(&response, (char*)wire, length) == RTSP_ERROR_MALFORMED);
    CHECK(allocations == 0);
}

static POPTION_ITEM allocatedOption(const char* name, const char* value) {
    POPTION_ITEM option = countedMalloc(sizeof(*option));
    option->flags = FLAG_ALLOCATED_OPTION_FIELDS;
    option->option = countedMalloc(strlen(name) + 1);
    option->content = countedMalloc(strlen(value) + 1);
    strcpy(option->option, name);
    strcpy(option->content, value);
    option->next = NULL;
    return option;
}

int main(void) {
    RTSP_MESSAGE response;
    POPTION_ITEM options = NULL;
    size_t i;
    const char* invalid[] = {
        "RTSP/1.0 200junk OK\r\n\r\n", "RTSP/1.0 0 OK\r\n\r\n",
        "RTSP/1.0 -200 OK\r\n\r\n", "RTSP/1.0 99999999999999999999 OK\r\n\r\n",
        "RTSP/1.0\r\n\r\n", "RTSP/2.0 200 OK\r\n\r\n",
        "RTSP/1.0 200 OK", "DESCRIBE\r\n\r\n",
        "RTSP/1.0 200 OK\r\nCSeq: 2\r\nBroken header\r\n\r\n",
        "RTSP/1.0 200 OK\r\nCSeq: 2\r\n : empty name\r\n\r\n",
        "RTSP/1.0 200 OK\r\nCSeq: 2\r\nTruncated"
    };
    const char* sequences[] = {"2junk", "-1", "99999999999999999999", ""};
    const char* endings[] = {"\r\n", "\r\n\r", "\r\n\r\n"};
    const char* standard[] = {
        "v=0\r\na=rtpmap:96 H264/90000\r\n",
        "v=0\r\na=fmtp:96 sprop-parameter-sets=AAAAAUabcdef\r\n",
        "v=0\r\na=rtpmap:98 AV1/90000\r\n"
    };
    const char* headers = "RTSP/1.0 200 OK\r\nCSeq: 2\r\nContent-Type: application/sdp\r\n\r\n";

    describe(headers, matching, SCM_PYROWAVE, 0);
    describe("RTSP/1.0\t200\tOK  \t\r\ncSeQ:\t2 \t\r\ncontent-type:application/sdp\r\n\r\n",
             matching, SCM_PYROWAVE, 0);
    describe("RTSP/1.0 200\r\nCSeq:2\r\n\r\n", matching, SCM_PYROWAVE, 0);
    describe("RTSP/1.0 200 OK\nCSeq:2\n\n",
             "v=0\na=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:186f0393\n",
             SCM_PYROWAVE, 0);
    describe(headers, matching, SCM_MASK_AV1 | SCM_MASK_HEVC | SCM_MASK_H264, ML_ERROR_PYROWAVE_SCM);
    describe(headers, "a=rtpmap:99 PYROWAVE/90000\r\n", SCM_PYROWAVE, ML_ERROR_PYROWAVE_BITSTREAM);
    describe(headers, "a=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:deadbeef\r\n",
             SCM_PYROWAVE, ML_ERROR_PYROWAVE_MISMATCH);
    describe(headers, "a=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:186f0393\r\n"
             "a=x-ss-pyrowave.bitstream:186f0393\r\n", SCM_PYROWAVE, ML_ERROR_PYROWAVE_BITSTREAM);
    // Header whitespace/case normalization must never normalize the SDP body.
    describe(headers, "a=rtpmap:99 pyrowave/90000\r\na=x-ss-pyrowave.bitstream:186f0393\r\n",
             SCM_PYROWAVE, ML_ERROR_PYROWAVE_RTPMAP);
    describe(headers, "a=rtpmap:99 PYROWAVE/90000 \r\na=x-ss-pyrowave.bitstream:186f0393\r\n",
             SCM_PYROWAVE, ML_ERROR_PYROWAVE_RTPMAP);
    for (i = 0; i < sizeof(standard)/sizeof(standard[0]); ++i)
        describe(headers, standard[i], SCM_PYROWAVE, ML_ERROR_PYROWAVE_RTPMAP);

    // Last case-insensitive header wins; duplicate SDP attributes above still fail.
    {
        char wire[] = "RTSP/1.0 200 OK\r\nCSeq: 1\r\ncseq: 2\r\nSession: old\r\n"
                      "sEsSiOn:\tnew;timeout=90 \t\r\nTransport: unicast;server_port=48000-48001\r\n\r\n";
        CHECK(parseRtspMessage(&response, wire, (int)strlen(wire)) == 0);
        CHECK(response.sequenceNumber == 2);
        CHECK(!strcmp(getOptionContent(response.options, "SESSION"), "new;timeout=90"));
        CHECK(!strcmp(getOptionContent(response.options, "transport"), "unicast;server_port=48000-48001"));
        CHECK(response.options->next->next->next == NULL);
        freeMessage(&response);
        CHECK(allocations == 0);
    }
    // Exercise both allocated and borrowed fields in duplicate replacement.
    insertOption(&options, allocatedOption("Session", "old"));
    insertOption(&options, allocatedOption("Transport", "port"));
    for (i = 0; i < 1000; ++i) {
        insertOption(&options, allocatedOption("sEsSiOn", "new"));
        CHECK(allocations == 6 && options->next && !options->next->next);
    }
    {
        POPTION_ITEM borrowed = countedMalloc(sizeof(*borrowed));
        borrowed->flags = 0; borrowed->option = "SESSION"; borrowed->content = "borrowed";
        borrowed->next = NULL;
        insertOption(&options, borrowed);
        CHECK(allocations == 4 && !strcmp(getOptionContent(options, "session"), "borrowed"));
        insertOption(&options, allocatedOption("Session", "owned again"));
        CHECK(allocations == 6);
    }
    freeOptionList(options);
    CHECK(allocations == 0);

    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
        malformed(invalid[i], (int)strlen(invalid[i]));
    malformed("", 0);
    malformed("", -1);
    for (i = 0; i < sizeof(sequences)/sizeof(sequences[0]); ++i) {
        char wire[128];
        int length = snprintf(wire, sizeof(wire), "RTSP/1.0 200 OK\r\nCSeq: %s\r\n\r\n", sequences[i]);
        CHECK(parseRtspMessage(&response, wire, length) == 0);
        CHECK(response.sequenceNumber == SEQ_INVALID);
        freeMessage(&response);
        CHECK(allocations == 0);
    }
    // Keep upstream's ENet incomplete-terminator compatibility.
    for (i = 0; i < sizeof(endings)/sizeof(endings[0]); ++i) {
        char wire[128];
        int length = snprintf(wire, sizeof(wire), "RTSP/1.0 200 OK\r\nCSeq: 2%s", endings[i]);
        CHECK(parseRtspMessage(&response, wire, length) == 0);
        CHECK(response.sequenceNumber == 2 && response.payload == NULL && response.payloadLength == 0);
        freeMessage(&response);
        CHECK(allocations == 0);
    }
    puts("PASS: real RTSP parser/SDP boundary, standard offers, malformed input, duplicate ownership, ENet endings");
    return 0;
}
