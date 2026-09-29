#include "pyrowave_frame.h"

#include <cstdlib>
#include <iostream>
#include <limits>

using Bytes = std::vector<std::uint8_t>;
static int cases = 0;
static void require(bool value, const char* label) {
    if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
static Bytes make(std::initializer_list<Bytes> packets) {
    Bytes b{'P','Y','R','W',1,0,static_cast<std::uint8_t>(packets.size()),0};
    for (const auto& p : packets) {
        const auto n = std::uint32_t(p.size());
        for (int s : {24,16,8,0}) b.push_back(std::uint8_t(n >> s));
        b.insert(b.end(), p.begin(), p.end());
    }
    return b;
}
static void test(const char* label, const Bytes& b, bool valid,
                 std::size_t declared = std::numeric_limits<std::size_t>::max()) {
    PyroWave::Frame f;
    f.packets.push_back({999,999}); f.payloadBytes = 999;
    std::string error;
    const bool ok = PyroWave::parseFrame(b.data(), b.size(),
        declared == std::numeric_limits<std::size_t>::max() ? b.size() : declared, f, error);
    require(ok == valid, label);
    require(valid ? error.empty() && !f.packets.empty() :
        !error.empty() && f.packets.empty() && f.payloadBytes == 0, label);
    if (valid) {
        std::size_t sum = 0;
        for (const auto& p : f.packets) {
            require(p.size && p.offset <= b.size() && p.size <= b.size() - p.offset, label);
            sum += p.size;
        }
        require(sum == f.payloadBytes, label);
    }
    ++cases;
}
int main() {
    const auto one = make({{1,2,3}});
    const auto multi = make({{1},{2,3},{4,5,6}});
    test("one packet", one, true); test("multiple packets", multi, true);
    auto b = one; b[0] = 'X'; test("magic", b, false);
    b = one; b[4] = 2; test("version", b, false);
    b = one; b[7] = 1; test("reserved", b, false);
    b = one; b[6] = 0; test("zero count", b, false);
    b = one; b[11] = 0; test("zero length", b, false);
    b = multi; b.resize(14); test("truncated length", b, false);
    b = one; b.pop_back(); test("truncated data", b, false);
    b = one; b[6] = 2; test("count too high", b, false);
    b = multi; b[6] = 2; test("count too low", b, false);
    b = one; b.push_back(0); test("trailing bytes", b, false);
    b = one; b.resize(PyroWave::MaxFrameBytes + 1); test("oversized", b, false);
    b = one; for (int i=8;i<12;++i) b[i]=255; test("UINT32_MAX length", b, false);
    b = one; b[8]=128; test("high bit length", b, false);
    b = one; b[5]=255; b[6]=255; test("pathological count", b, false);
    test("declared sum mismatch", one, false, one.size()+1);
    test("huge declared length", one, false, std::numeric_limits<std::size_t>::max()-1);
    PyroWave::Frame f; std::string error;
    require(!PyroWave::parseFrame(nullptr, 9, 9, f, error), "null input");
    require(!PyroWave::parseFrame(one.data(), std::numeric_limits<std::size_t>::max(),
        std::numeric_limits<std::size_t>::max(), f, error), "size overflow before read");
    for (std::size_t n=0;n<one.size();++n) test("every truncation", Bytes(one.begin(),one.begin()+n),false);
    // Exact application safety boundary and packet offset/payload contents.
    b = make({Bytes(PyroWave::MaxFrameBytes - 12, 42)}); test("exact size limit", b, true);
    require(PyroWave::parseFrame(multi.data(),multi.size(),multi.size(),f,error), "offset parsing");
    require(f.packets.size()==3 && f.payloadBytes==6 && multi[f.packets[1].offset]==2, "offset contents");
    // Deterministic mutations exercise bounds without Vulkan or unbounded allocations.
    std::uint32_t seed=12345;
    for (int i=0;i<10000;++i) {
        b=multi; seed=seed*1664525u+1013904223u;
        b[seed%b.size()]^=std::uint8_t(seed>>24);
        PyroWave::parseFrame(b.data(),b.size(),b.size(),f,error);
        if (!error.empty()) require(f.packets.empty() && f.payloadBytes==0,"mutation clears state");
    }
    std::cout << "PASS: " << cases << " parser cases + 10000 bounded mutations\n";
}
