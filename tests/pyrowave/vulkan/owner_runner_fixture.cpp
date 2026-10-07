#include <fstream>
#include <string>
#include <cstdlib>
#include <filesystem>
// GPU-free process fixture; evidence templates are supplied by the test script.
int main(int argc,char** argv) {
    if(std::string(argv[0]).find("-tests")!=std::string::npos) return 0;
    std::string log; bool diagnostic=false,factory=false;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if((arg=="--runtime" || arg=="--log" || arg=="--fail-at") && i+1<argc) {
            if(arg=="--log") log=argv[i+1]; ++i;
        } else if(arg=="--diagnostic-suite") diagnostic=true;
        else if(arg=="--factory-fault") factory=true;
        else if(arg!="--diagnostic-prefill" && arg!="--diagnostic-device-idle" && arg!="--force-compute") return 2;
    }
    if(log.empty()) return 2;
    std::ofstream evidence(log); if(!evidence) return 2;
    const auto data=std::getenv("STAGE3_FIXTURE_DATA");
    if(!data) { evidence<<"validation_status=SKIP\noverall=SKIP\n"; return 77; }
    const auto root=std::filesystem::path(data);
    if(diagnostic) {
        for(auto name:{"diagnostic-summary.json","auto-fragment.json","forced-compute.json","repeatability.json"}) {
            std::filesystem::copy_file(root/name,std::filesystem::path(log).parent_path()/name,std::filesystem::copy_options::overwrite_existing);
            std::filesystem::last_write_time(std::filesystem::path(log).parent_path()/name,std::filesystem::file_time_type::clock::now());
        }
        evidence<<"selected_device=GPU_FREE_FIXTURE\ndiagnostic_complete=YES validation_errors=0 validation_status=SKIP overall=DIAGNOSTIC_COMPLETE\n"; return 0;
    }
    if(factory) {
        const bool failed=std::filesystem::exists(root/"factory-fail");
        evidence<<"factory_failure_cleanup=PATCHED_SOURCE_TEST_REQUIRED owner_qualification=PENDING\n";
        evidence<<"factory_fault_probe=PASS factory_failure_cleanup="<<(failed ? "FAIL" : "PATCHED_AND_VERIFIED")<<" memory_growth_is_supporting_diagnostic=YES\nvalidation_status=SKIP\n"; return failed ? 1 : 0;
    }
    const bool failed=std::filesystem::exists(root/"decode-fail");
    evidence<<"comparison=NUMERIC_EQUIVALENCE stage3_tolerance=1 numerically_equivalent=true exact_equal=false max_absolute_error=1 frames_tested=144 slots_exercised=3 decoder_lifetimes=3 malformed_recovery=PASS\nvalidation_status=SKIP\n"; return failed ? 1 : 0;
}
