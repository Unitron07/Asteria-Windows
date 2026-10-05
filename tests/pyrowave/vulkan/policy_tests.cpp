#include "pyrowave_vulkan_policy.h"
#include <iostream>
using namespace PyroWaveVulkan;
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        expect(imageCount(1, 0) == 2 && imageCount(3, 0) == 3 && imageCount(1, 1) == 1, "legal bounded image counts");
        bool rejected = false;
        try { imageCount(3, 2); } catch (...) { rejected = true; }
        expect(rejected, "reject invalid limits");
        auto e = chooseExtent({UINT32_MAX, UINT32_MAX}, {1920, 10}, {64, 64}, {1024, 768});
        expect(e.width == 1024 && e.height == 64, "clamp drawable extent");
        e = chooseExtent({800, 600}, {0, 0}, {1, 1}, {4096, 4096});
        expect(!e.width && !e.height, "minimize suspends even fixed extent");
        e = chooseExtent({800, 600}, {1920, 1080}, {1, 1}, {4096, 4096});
        expect(e.width == 800 && e.height == 600, "fixed surface extent wins");
        expect(presentMode(true, {Mode::Immediate, Mode::Fifo}) == Mode::Fifo, "vsync FIFO");
        expect(presentMode(false, {Mode::Fifo, Mode::Immediate}) == Mode::Immediate, "no-vsync immediate");
        expect(presentMode(false, {Mode::Fifo, Mode::Mailbox, Mode::Relaxed}) == Mode::Relaxed, "relaxed before mailbox");
        expect(presentMode(false, {Mode::Fifo, Mode::Mailbox}) == Mode::Mailbox, "mailbox fallback");
        expect(presentMode(false, {Mode::Fifo}) == Mode::Fifo, "FIFO fallback");
        expect(queueFamily({{true,false,true,1},{true,true,false,1},{true,true,true,0}}) == -1, "reject incompatible queues");
        expect(queueFamily({{false,true,true,1},{true,true,true,1}}) == 1, "one combined queue");
        Progress p;
        expect(p.decide({0,0}, true, Acquire::Ready) == Action::Suspend && p.rebuild, "zero extent retains rebuild");
        expect(p.decide({800,600}, true, Acquire::Ready) == Action::Rebuild, "initial rebuild");
        p.rebuild = false;
        expect(p.decide({800,600}, false, Acquire::Ready) == Action::Retry && !p.rebuild, "pending command resource never reset");
        expect(p.decide({800,600}, true, Acquire::NotReady) == Action::Retry && p.frameResource == 0, "timeout never advances command resource");
        expect(p.decide({800,600}, true, Acquire::Ready) == Action::Submit, "acquired image submit");
        p.submitted(); p.submitted();
        expect(p.frameResource == 0, "two bounded resources");
        expect(p.decide({800,600}, true, Acquire::Suboptimal) == Action::Submit && p.rebuild, "suboptimal acquired image consumed before rebuild");
        p.rebuild = false;
        expect(p.decide({800,600}, true, Acquire::OutOfDate) == Action::Rebuild && p.rebuild, "out-of-date no-submit rebuild");
        p.rebuild = false;
        expect(p.decide({800,600}, true, Acquire::Failed) == Action::Stop && p.stopped, "surface/device error terminal");
        expect(p.decide({800,600}, true, Acquire::Ready) == Action::Stop, "stopped never resubmits");
        std::cout << "PASS GPU-free Vulkan extent/mode/queue/resource/retry policy\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << "\n"; return 1; }
}
