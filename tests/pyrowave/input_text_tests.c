// Run production UTF-8 text delivery with a synchronous fake send thread. No socket,
// encryption, ENet or host text handling is exercised by this fixture.
#include "common_c_test_env.h"

SOCKADDR_LEN AddrLen = sizeof(struct sockaddr_in);
uint32_t SunshineFeatureFlags;

#define MAX_SENT_PACKETS 128

static struct {
    char text[UTF8_TEXT_EVENT_MAX_COUNT];
    uint32_t size, flags;
    uint8_t channel;
    bool more;
} sent[MAX_SENT_PACKETS];
static int sentCount, flushCalls, sleptMs;
static ThreadEntry threadEntry;
static void* threadContext;

int sendInputPacketOnControlStream(unsigned char* data, int length, uint8_t channelId, uint32_t flags, bool moreData) {
    NV_INPUT_HEADER header;
    memcpy(&header, data, sizeof(header));
    // The enable-haptics packet from startInputStream() is not text.
    if (LE32(header.magic) != UTF8_TEXT_EVENT_MAGIC) return 0;

    uint32_t size = BE32(header.size) - sizeof(uint32_t);
    CHECK(sentCount < MAX_SENT_PACKETS);
    CHECK(size <= UTF8_TEXT_EVENT_MAX_COUNT);
    CHECK((uint32_t)length == sizeof(header) + size);
    memcpy(sent[sentCount].text, data + sizeof(header), size);
    sent[sentCount].size = size;
    sent[sentCount].flags = flags;
    sent[sentCount].channel = channelId;
    sent[sentCount].more = moreData;
    sentCount++;
    return 0;
}
void flushInputOnControlStream(void) { flushCalls++; }
bool isControlDataInTransit(void) { return false; }

// The queue runs on one thread, so a wait could only ever be a deadlock.
void PltWaitForConditionVariable(PLT_COND* cond, PLT_MUTEX* mutex) { (void)cond; (void)mutex; CHECK(false); }
int PltCreateConditionVariable(PLT_COND* cond, PLT_MUTEX* mutex) { (void)cond; (void)mutex; return 0; }
void PltDeleteConditionVariable(PLT_COND* cond) { (void)cond; }
void PltSignalConditionVariable(PLT_COND* cond) { (void)cond; }
int PltCreateMutex(PLT_MUTEX* mutex) { (void)mutex; return 0; }
void PltDeleteMutex(PLT_MUTEX* mutex) { (void)mutex; }
void PltLockMutex(PLT_MUTEX* mutex) { (void)mutex; }
void PltUnlockMutex(PLT_MUTEX* mutex) { (void)mutex; }

// startInputStream() records the thread; stopInputStream() runs it to drain the queue.
int PltCreateThread(const char* name, ThreadEntry entry, void* context, PLT_THREAD* thread) {
    (void)name; (void)thread;
    threadEntry = entry;
    threadContext = context;
    return 0;
}
void PltJoinThread(PLT_THREAD* thread) { (void)thread; threadEntry(threadContext); }
bool PltIsThreadInterrupted(PLT_THREAD* thread) { (void)thread; return false; }
void PltSleepMs(int ms) { sleptMs += ms; }
uint64_t PltGetMillis(void) { return 0; }

PPLT_CRYPTO_CONTEXT PltCreateCryptoContext(void) { return NULL; }
void PltDestroyCryptoContext(PPLT_CRYPTO_CONTEXT ctx) { (void)ctx; }
bool PltEncryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                       unsigned char* key, int keyLength,
                       unsigned char* iv, int ivLength,
                       unsigned char* tag, int tagLength,
                       unsigned char* inputData, int inputDataLength,
                       unsigned char* outputData, int* outputDataLength) {
    (void)ctx; (void)algorithm; (void)flags; (void)key; (void)keyLength; (void)iv; (void)ivLength;
    (void)tag; (void)tagLength; (void)inputData; (void)inputDataLength; (void)outputData; (void)outputDataLength;
    CHECK(false); // 7.1.431 and later encrypt on the control stream
    return false;
}

// Only Gen 4 and older hosts use a separate input socket.
SOCKET connectTcpSocket(struct sockaddr_storage* dstaddr, SOCKADDR_LEN addrlen, unsigned short port, int timeoutSec) {
    (void)dstaddr; (void)addrlen; (void)port; (void)timeoutSec;
    CHECK(false);
    return INVALID_SOCKET;
}
int enableNoDelay(SOCKET s) { (void)s; CHECK(false); return 0; }
void shutdownTcpSocket(SOCKET s) { (void)s; CHECK(false); }
void closeSocket(SOCKET s) { (void)s; CHECK(false); }

static void deliver(bool sunshine, const char* text, unsigned int length) {
    sentCount = flushCalls = sleptMs = 0;
    AppVersionQuad[3] = sunshine ? -1 : 0;
    CHECK(initializeInputStream() == 0);
    CHECK(startInputStream() == 0);
    CHECK(LiSendUtf8TextEvent(text, length) == 0);
    CHECK(stopInputStream() == 0);
    destroyInputStream();
}

static void checkDelivered(const char* expected, unsigned int length) {
    unsigned int offset = 0;
    CHECK(sentCount > 0);
    for (int i = 0; i < sentCount; i++) {
        CHECK(sent[i].channel == CTRL_CHANNEL_KEYBOARD);
        CHECK(sent[i].flags == ENET_PACKET_FLAG_RELIABLE);
        CHECK(offset + sent[i].size <= length);
        CHECK(memcmp(sent[i].text, expected + offset, sent[i].size) == 0);
        offset += sent[i].size;
        CHECK(sent[i].more == (offset < length));
    }
    CHECK(offset == length);
}

int main(void) {
    // 30 ASCII, 3-byte euro sign, 4-byte emoji, 40 ASCII
    char mixed[77];
    memset(mixed, 'a', 30);
    memcpy(mixed + 30, "\xE2\x82\xAC\xF0\x9F\x98\x80", 7);
    memset(mixed + 37, 'b', 40);

    // Sunshine packs each packet, never splits a code point and needs no delay.
    deliver(true, mixed, sizeof(mixed));
    checkDelivered(mixed, sizeof(mixed));
    CHECK(sentCount == 3);
    CHECK(sent[0].size == 30);
    CHECK(sent[1].size == UTF8_TEXT_EVENT_MAX_COUNT);
    CHECK(sent[2].size == 15);
    CHECK(flushCalls == 0);
    CHECK(sleptMs == 0);

    // GFE still gets one code point per packet after the keyboard flush and delay.
    deliver(false, mixed, sizeof(mixed));
    checkDelivered(mixed, sizeof(mixed));
    CHECK(sentCount == 72);
    CHECK(sent[29].size == 1);
    CHECK(sent[30].size == 3);
    CHECK(sent[31].size == 4);
    CHECK(flushCalls == 1);
    CHECK(sleptMs == 50);

    // An exact fit stays in one packet.
    memset(mixed, 'c', UTF8_TEXT_EVENT_MAX_COUNT);
    deliver(true, mixed, UTF8_TEXT_EVENT_MAX_COUNT);
    checkDelivered(mixed, UTF8_TEXT_EVENT_MAX_COUNT);
    CHECK(sentCount == 1);

    // A truncated final code point is clamped, not read past the end of the text.
    deliver(true, "ab\xE2\x82", 4);
    checkDelivered("ab\xE2\x82", 4);
    CHECK(sentCount == 1);

    // An invalid start byte ends delivery after the text before it.
    deliver(true, "ab\xFF" "cd", 5);
    CHECK(sentCount == 1);
    CHECK(sent[0].size == 2);
    CHECK(memcmp(sent[0].text, "ab", 2) == 0);
    deliver(false, "ab\xFF" "cd", 5);
    CHECK(sentCount == 2);

    puts("PASS: UTF-8 text uses the keyboard channel, packing and legacy pacing");
    return 0;
}
