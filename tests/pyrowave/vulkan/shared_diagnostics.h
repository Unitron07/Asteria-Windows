#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stage3 {
struct DifferenceSample { uint64_t offset; uint32_t x,y; uint8_t cpu,gpu; int difference; };
struct Differences {
    uint64_t bytes=0,matching=0,mismatching=0;
    uint8_t cpuMin=255,cpuMax=0,gpuMin=255,gpuMax=0;
    unsigned maxAbsoluteError=0;
    uint64_t absoluteErrorSum=0;
    std::array<uint64_t,9> histogram{}; // GPU - CPU: <=-4,-3,-2,-1,0,1,2,3,>=4
    int64_t first=-1,last=-1;
    uint64_t firstRow=0,lastRow=0,firstColumn=0,lastColumn=0,rows=0;
    uint64_t longestMatching=0,longestMismatching=0;
    std::array<uint64_t,4> regions{}; // TL,TR,BL,BR
    std::vector<DifferenceSample> samples;
    double percentage() const { return bytes ? 100.0*double(mismatching)/double(bytes) : 0; }
    double meanAbsoluteError() const { return bytes ? double(absoluteErrorSum)/double(bytes) : 0; }
};
inline Differences differences(const std::vector<uint8_t>& cpu,const std::vector<uint8_t>& gpu,uint32_t width,uint32_t height) {
    if (!width || !height || uint64_t(width)*height!=cpu.size() || cpu.size()!=gpu.size())
        throw std::runtime_error("diagnostic plane extent/byte count mismatch");
    Differences d; d.bytes=cpu.size();
    uint64_t equalRun=0,differentRun=0; int64_t lastRowWithMismatch=-1;
    for (size_t i=0;i<cpu.size();++i) {
        d.cpuMin=std::min(d.cpuMin,cpu[i]); d.cpuMax=std::max(d.cpuMax,cpu[i]);
        d.gpuMin=std::min(d.gpuMin,gpu[i]); d.gpuMax=std::max(d.gpuMax,gpu[i]);
        const int delta=int(gpu[i])-int(cpu[i]); const unsigned absolute=unsigned(delta<0 ? -delta : delta);
        ++d.histogram[std::clamp(delta,-4,4)+4];
        d.absoluteErrorSum+=absolute; d.maxAbsoluteError=std::max(d.maxAbsoluteError,absolute);
        if (!delta) { ++d.matching; ++equalRun; differentRun=0; d.longestMatching=std::max(d.longestMatching,equalRun); continue; }
        ++d.mismatching; ++differentRun; equalRun=0; d.longestMismatching=std::max(d.longestMismatching,differentRun);
        const uint32_t x=uint32_t(i%width),y=uint32_t(i/width);
        if (d.first<0) d.first=int64_t(i); d.last=int64_t(i);
        if (!y) ++d.firstRow; if (y==height-1) ++d.lastRow;
        if (!x) ++d.firstColumn; if (x==width-1) ++d.lastColumn;
        if (int64_t(y)!=lastRowWithMismatch) { ++d.rows; lastRowWithMismatch=y; }
        ++d.regions[(y>=height/2 ? 2 : 0)+(x>=width/2 ? 1 : 0)];
        if (d.samples.size()<32) d.samples.push_back({i,x,y,cpu[i],gpu[i],delta});
    }
    return d;
}
inline bool repeatHashStable(const std::vector<std::string>& hashes) {
    return hashes.size()>=2 && std::all_of(hashes.begin()+1,hashes.end(),[&](const auto& h) { return h==hashes.front(); });
}
inline std::string differenceJson(const Differences& d) {
    std::ostringstream s; s<<std::setprecision(12);
    s<<"{\"byte_count\":"<<d.bytes<<",\"matching_bytes\":"<<d.matching<<",\"mismatching_bytes\":"<<d.mismatching
     <<",\"mismatch_percentage\":"<<d.percentage()<<",\"cpu_min\":"<<unsigned(d.cpuMin)<<",\"cpu_max\":"<<unsigned(d.cpuMax)
     <<",\"gpu_min\":"<<unsigned(d.gpuMin)<<",\"gpu_max\":"<<unsigned(d.gpuMax)<<",\"max_absolute_error\":"<<d.maxAbsoluteError
     <<",\"mean_absolute_error\":"<<d.meanAbsoluteError()<<",\"signed_difference\":\"GPU_MINUS_CPU\",\"signed_difference_histogram\":{";
    const char* labels[]={"<=-4","-3","-2","-1","0","+1","+2","+3",">=+4"};
    for (unsigned i=0;i<9;++i) { if(i) s<<','; s<<'\"'<<labels[i]<<"\":"<<d.histogram[i]; }
    s<<"},\"first_mismatch_offset\":"<<d.first<<",\"last_mismatch_offset\":"<<d.last
     <<",\"first_row_mismatches\":"<<d.firstRow<<",\"last_row_mismatches\":"<<d.lastRow
     <<",\"first_column_mismatches\":"<<d.firstColumn<<",\"last_column_mismatches\":"<<d.lastColumn
     <<",\"rows_containing_mismatches\":"<<d.rows<<",\"longest_matching_run\":"<<d.longestMatching
     <<",\"longest_mismatching_run\":"<<d.longestMismatching<<",\"regions\":{";
    const char* regions[]={"top_left","top_right","bottom_left","bottom_right"};
    for(unsigned i=0;i<4;++i) { if(i) s<<','; s<<'\"'<<regions[i]<<"\":"<<d.regions[i]; }
    s<<"},\"first_mismatches\":[";
    for(size_t i=0;i<d.samples.size();++i) { const auto& v=d.samples[i]; if(i) s<<',';
        s<<"{\"offset\":"<<v.offset<<",\"x\":"<<v.x<<",\"y\":"<<v.y<<",\"cpu_value\":"<<unsigned(v.cpu)
         <<",\"gpu_value\":"<<unsigned(v.gpu)<<",\"signed_difference\":"<<v.difference<<'}'; }
    s<<"]}"; return s.str();
}
}
