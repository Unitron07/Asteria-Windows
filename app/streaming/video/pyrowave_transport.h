#pragma once
#include "pyrowave_frame.h"
#include <Limelight.h>
#include <new>

namespace PyroWave {
// common-c supplies complete decode units after its ordinary RTP/FEC assembly.
// These fragments are transport buffers, not PyroWave codec packet boundaries.
inline bool assembleLiveDecodeUnit(const DECODE_UNIT& du, std::vector<std::uint8_t>& bytes,
                                   std::string& error) {
    bytes.clear(); error.clear();
    if (du.fullLength <= 0 || std::size_t(du.fullLength) > LiveLimits.frameBytes) {
        error = "decode unit length outside live 8 MiB bound"; return false;
    }
    try {
        bytes.reserve(du.fullLength);
        unsigned fragments = 0;
        for (auto entry=du.bufferList; entry; entry=entry->next) {
            if (++fragments > 4000 || entry->bufferType != BUFFER_TYPE_PICDATA ||
                !entry->data || entry->length <= 0 ||
                std::size_t(entry->length) > std::size_t(du.fullLength) - bytes.size()) {
                error = "invalid complete decode-unit fragment chain"; bytes.clear(); return false;
            }
            bytes.insert(bytes.end(),entry->data,entry->data+entry->length);
        }
        if (bytes.size() != std::size_t(du.fullLength)) {
            error = "truncated complete decode unit"; bytes.clear(); return false;
        }
    } catch (const std::bad_alloc&) {
        error = "live frame allocation failed"; bytes.clear(); return false;
    }
    return true;
}
}
