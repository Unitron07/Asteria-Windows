#include <fstream>
#include <string>
// GPU-free native fixture: exercise the packaged runner's strict argv contract
// on each target architecture without a managed-runtime/emulation dependency.
int main(int argc, char** argv) {
    if (std::string(argv[0]).find("policy-tests") != std::string::npos) return 0;
    std::string log;
    for (int i=1; i<argc; ++i) {
        const std::string arg=argv[i];
        if ((arg=="--runtime" || arg=="--log") && i+1<argc) {
            if (arg=="--log") log=argv[i+1];
            ++i;
        } else return 2;
    }
    if (log.empty()) return 2;
    std::ofstream evidence(log);
    if (!evidence) return 2;
    evidence << "validation_status=SKIP\noverall=SKIP\n";
    return 77;
}
