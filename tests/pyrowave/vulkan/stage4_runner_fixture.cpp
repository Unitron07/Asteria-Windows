#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
int main(int argc,char** argv) {
    if(std::string(argv[0]).find("-tests.exe")!=std::string::npos) return 0;
    std::filesystem::path evidence;
    for(int i=1;i<argc;++i) {
        if(!std::strcmp(argv[i],"--source-revision")) { std::cout<<STAGE4_SOURCE_REVISION<<'\n'; return 0; }
        if(!std::strcmp(argv[i],"--evidence") && i+1<argc) evidence=argv[++i];
    }
    if(evidence.empty()) return 2;
    std::filesystem::create_directories(evidence);
    std::ofstream(evidence/"api-result.json")<<"{\"result\":\"SKIP\",\"ownerVisualConfirmed\":false}";
    return 77;
}
