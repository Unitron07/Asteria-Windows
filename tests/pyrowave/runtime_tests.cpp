#include "pyrowave_runtime.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc!=3) return 2;
    PyroWave::Runtime runtime;
    if (runtime.load("relative-directory")) return 1;
    if (runtime.error().find("absolute")==std::string::npos) return 1;
    const auto dir=std::filesystem::absolute(argv[1]);
    for (int i=0;i<3;++i) {
        if (runtime.load(dir)) return 1;
        if (runtime.error().find(argv[2])==std::string::npos) return 1;
        // Failed partial export resolution/version checks must release the module.
        runtime.close();
    }
    PyroWave::Pixels pixels;
    pixels.planes[0].resize(5);
    if (runtime.decode({},pixels) || !pixels.planes[0].empty()) return 1;
    if (runtime.createDecoder(1920,1080)) return 1;
    std::cout << "PASS: relative-path rejection, rejected DLL reason, repeated cleanup, no decoder state\n";
}
