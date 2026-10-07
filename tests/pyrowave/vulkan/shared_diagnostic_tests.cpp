#include "shared_diagnostics.h"
#include "shared_policy.h"
#include <cmath>
#include <iostream>
using namespace Stage3;
static void require(bool okay) { if(!okay) throw std::runtime_error("diagnostic unit test failure"); }
int main() {
    try {
        std::vector<uint8_t> a{0,1,2,3,4,5,6,255};
        auto equal=differences(a,a,4,2);
        require(equal.matching==8 && !equal.mismatching && equal.first==-1 && equal.last==-1 && equal.histogram[4]==8);
        require(equal.cpuMin==0 && equal.cpuMax==255 && equal.gpuMin==0 && equal.gpuMax==255 && equal.longestMatching==8 && !equal.rows);
        auto b=a; b[5]=7; auto one=differences(a,b,4,2);
        require(one.mismatching==1 && one.matching==7 && one.percentage()==12.5 && one.maxAbsoluteError==2 && one.meanAbsoluteError()==0.25);
        require(one.first==5 && one.last==5 && one.samples[0].x==1 && one.samples[0].y==1 && one.samples[0].difference==2);
        require(one.firstRow==0 && one.lastRow==1 && one.rows==1 && one.longestMatching==5 && one.longestMismatching==1 && one.regions[2]==1);
        std::vector<uint8_t> cpu(9,128),gpu{124,125,126,127,128,129,130,131,132};
        auto histogram=differences(cpu,gpu,3,3); for(auto count:histogram.histogram) require(count==1);
        require(histogram.firstRow==3 && histogram.lastRow==3 && histogram.firstColumn==3 && histogram.lastColumn==3 && histogram.rows==3);
        require(histogram.maxAbsoluteError==4 && std::abs(histogram.meanAbsoluteError()-20.0/9)<1e-12);
        auto different=differences(std::vector<uint8_t>(64,0),std::vector<uint8_t>(64,255),8,8);
        require(different.percentage()==100 && different.mismatching==64 && different.samples.size()==32 && different.maxAbsoluteError==255);
        require(different.longestMismatching==64 && !different.longestMatching && different.first==0 && different.last==63);
        for(auto count:different.regions) require(count==16);
        require(!repeatHashStable({}) && !repeatHashStable({"a"}) && repeatHashStable({"a","a","a"}) && !repeatHashStable({"a","b","a"}));
        require(selectedPath(true,false)==Path::Fragment && selectedPath(true,true)==Path::Compute && selectedPath(false,false)==Path::Compute);
        require(usage(selectedPath(true,false))==17 && usage(selectedPath(true,true))==9);
        require(differenceJson(one).find("\"x\":1,\"y\":1")!=std::string::npos);
        bool rejected=false; try { differences({1},{1},2,1); } catch(const std::exception&) { rejected=true; } require(rejected);
        std::cout<<"PASS: exact difference metrics, spatial counts, bounded samples, stable hashes, AUTO/FORCE_COMPUTE usages\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
