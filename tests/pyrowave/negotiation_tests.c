#include "PyroWave.h"
#include <stdio.h>
#include <stdlib.h>

static void test(int formats, int scm, const char* sdp, int expected) {
    PYROWAVE_SDP_INFO info;
    int result = validatePyroWaveSdp(formats,scm,sdp,sdp ? strlen(sdp) : 0,&info);
    if (result != expected) {
        fprintf(stderr,"negotiation result %d, expected %d\n",result,expected); exit(1);
    }
}
int main(void) {
    const char* matching = "v=0\r\na=rtpmap:99 PYROWAVE/90000\r\na=x-ss-pyrowave.bitstream:186f0393\r\n";
    const int standard = VIDEO_FORMAT_MASK_H264 | VIDEO_FORMAT_MASK_H265 | VIDEO_FORMAT_MASK_AV1;
    const int standardScm = SCM_MASK_H264 | SCM_MASK_HEVC | SCM_MASK_AV1;
    if ((standard & VIDEO_FORMAT_MASK_PYROWAVE) || (standardScm & SCM_MASK_PYROWAVE) ||
        VIDEO_FORMAT_PYROWAVE != 0x010000 || SCM_PYROWAVE != 0x00800000 ||
        (VIDEO_FORMAT_PYROWAVE & (VIDEO_FORMAT_MASK_10BIT | VIDEO_FORMAT_MASK_YUV444))) return 1;
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,matching,0);
    test(VIDEO_FORMAT_PYROWAVE,standardScm,matching,ML_ERROR_PYROWAVE_SCM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE_444,matching,ML_ERROR_PYROWAVE_SCM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE_HDR10,matching,ML_ERROR_PYROWAVE_SCM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=x-ss-pyrowave.bitstream:186f0393\n",ML_ERROR_PYROWAVE_RTPMAP);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000\n",ML_ERROR_PYROWAVE_BITSTREAM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:deadbeef\n",ML_ERROR_PYROWAVE_MISMATCH);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:186f03930\n",ML_ERROR_PYROWAVE_BITSTREAM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:186f0393\na=x-ss-pyrowave.bitstream:186f0393\n",ML_ERROR_PYROWAVE_BITSTREAM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000extra\na=x-ss-pyrowave.bitstream:186f0393\n",ML_ERROR_PYROWAVE_RTPMAP);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"x-a=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:186f0393\n",ML_ERROR_PYROWAVE_RTPMAP);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,"a=rtpmap:99 PYROWAVE/90000\na=x-ss-pyrowave.bitstream:186f039g\n",ML_ERROR_PYROWAVE_BITSTREAM);
    test(VIDEO_FORMAT_PYROWAVE,SCM_PYROWAVE,NULL,ML_ERROR_PYROWAVE_RTPMAP);
    // Auto and each standard-codec candidate set cannot enter PyroWave policy.
    test(standard,SCM_MASK_PYROWAVE|standardScm,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(VIDEO_FORMAT_H264,SCM_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(VIDEO_FORMAT_H265,SCM_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(VIDEO_FORMAT_AV1_MAIN8,SCM_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(VIDEO_FORMAT_H265_MAIN10,SCM_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(VIDEO_FORMAT_PYROWAVE|VIDEO_FORMAT_H265_MAIN10,SCM_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(0x020000,SCM_MASK_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    test(0x040000,SCM_MASK_PYROWAVE,matching,ML_ERROR_PYROWAVE_PROFILE);
    puts("PASS: exact DESCRIBE, capability/profile exclusion, constants/collisions");
    return 0;
}
