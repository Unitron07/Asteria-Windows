#include <fstream>
#include <string>
#include <cstdlib>
#include <filesystem>
// GPU-free native fixture: exercise the packaged runner's strict argv contract
// on each target architecture without a managed-runtime/emulation dependency.
int main(int argc, char** argv) {
    if (std::string(argv[0]).find("-tests") != std::string::npos) return 0;
    std::string log;
    bool diagnostic=false,factory=false;
    for (int i=1; i<argc; ++i) {
        const std::string arg=argv[i];
        if ((arg=="--runtime" || arg=="--log") && i+1<argc) {
            if (arg=="--log") log=argv[i+1];
            ++i;
        } else if(arg=="--diagnostic-suite") diagnostic=true;
        else if(arg=="--factory-fault") factory=true;
        else if(arg!="--diagnostic-prefill" && arg!="--diagnostic-device-idle") return 2;
    }
    if (log.empty()) return 2;
    std::ofstream evidence(log);
    if (!evidence) return 2;
    if(std::getenv("STAGE3_FIXTURE_DIAGNOSTIC")) {
        if(factory) { evidence<<"fixture=GPU_FREE validation_errors=0 overall=FACTORY_FAULT_DIAGNOSTIC\n"; return 0; }
        if(!diagnostic) return 2; // A failed AUTO diagnostic must never run qualification.
        std::ofstream summary(std::filesystem::path(log).parent_path()/"diagnostic-summary.json");
        summary<<"{\"sourceRevision\":\"GPU_FREE_FIXTURE\",\"overall\":\"DIAGNOSTIC_COMPLETE\",\"stage3Qualification\":\"FAIL_FRAGMENT_MISMATCH\",\"autoExact\":false,\"forcedComputeExact\":true}";
        evidence<<"fixture=GPU_FREE diagnostic_complete=YES validation_errors=0 overall=DIAGNOSTIC_COMPLETE\n"; return 1;
    }
    evidence << "validation_status=SKIP\noverall=SKIP\n";
    return 77;
}
