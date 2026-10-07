#pragma once
#include "shared_device.h"
#include "shared_diagnostics.h"
#include "../presentation_patterns.h"
#include <fstream>
#include <functional>

namespace Stage3 {
using Hash = std::function<std::string(const std::vector<uint8_t>&)>;
inline void diagnosticFile(const std::filesystem::path& path,const std::string& text) {
    std::ofstream f(path,std::ios::binary); f<<text<<'\n'; if(!f) throw std::runtime_error("cannot write diagnostic JSON");
}
inline void diagnosticBinary(const std::filesystem::path& path,const std::vector<uint8_t>& bytes) {
    std::ofstream f(path,std::ios::binary); f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    if(!f) throw std::runtime_error("cannot write bounded diagnostic plane dump");
}
inline std::string jsonArray(const std::vector<std::string>& entries) {
    std::string result="["; for(size_t i=0;i<entries.size();++i) { if(i) result+=','; result+=entries[i]; } return result+"]";
}
inline std::string diagnosticLines(const std::vector<std::string>& entries) {
    std::string result; for(const auto& entry:entries) result+=entry+"\n"; return result;
}
struct DiagnosticFixture {
    std::string name,pattern,framing="compatibility",range="full";
    std::vector<uint8_t> encoded;
    PyroWave::Pixels cpu;
};
inline std::vector<DiagnosticFixture> diagnosticFixtures(PyroWave::Runtime& reference) {
    std::vector<DiagnosticFixture> fixtures;
    for(auto name:{"flat-low","flat-mid","flat-high","gradient","bt709-bars"}) {
        DiagnosticFixture f; f.pattern=name; f.name=std::string(name)+"-compatibility-full";
        auto pixels=Presentation::pattern(std::string(name)=="bt709-bars" ? Presentation::Pattern::Bars : Presentation::Pattern::Gradient);
        if(std::string(name).find("flat-")==0) {
            const uint8_t value=std::string(name)=="flat-low" ? 16 : std::string(name)=="flat-mid" ? 128 : 235;
            std::fill(pixels.planes[0].begin(),pixels.planes[0].end(),value);
        }
        if(!reference.encodeProofPixels(pixels,f.encoded)) throw std::runtime_error(reference.error());
        fixtures.push_back(std::move(f));
    }
    const auto gradient=fixtures[3];
    PyroWave::Frame parsed; std::string error;
    const PyroWave::StreamContext context{1920,1080,PyroWave::Chroma::Yuv420,true};
    if(!PyroWave::parseFrame(gradient.encoded.data(),gradient.encoded.size(),gradient.encoded.size(),parsed,error,&context)) throw std::runtime_error(error);
    if(parsed.records.empty()) throw std::runtime_error("diagnostic gradient has no sequence record");
    auto limited=gradient; limited.name="gradient-compatibility-limited"; limited.range="limited";
    limited.encoded.at(parsed.records[0].offset+7)|=0x40; fixtures.push_back(std::move(limited));
    auto records=gradient; records.name="gradient-records-full"; records.framing="records"; records.encoded.clear();
    for(auto packet:parsed.packets) records.encoded.insert(records.encoded.end(),gradient.encoded.begin()+packet.offset,gradient.encoded.begin()+packet.offset+packet.size);
    fixtures.push_back(std::move(records));
    for(auto& fixture:fixtures) if(!reference.decode(fixture.encoded,fixture.cpu)) throw std::runtime_error(reference.error());
    return fixtures;
}
inline std::string diagnosticPlane(const DiagnosticFixture& fixture,unsigned frame,unsigned plane,const std::string& mode,
    const std::string& path,const std::vector<uint8_t>& a,const std::vector<uint8_t>& b,const Hash& hash) {
    const auto extent=planes(1920,1080)[plane];
    std::ostringstream s;
    s<<"{\"fixture\":\""<<fixture.name<<"\",\"frame\":"<<frame<<",\"pattern\":\""<<fixture.pattern
     <<"\",\"framing\":\""<<fixture.framing<<"\",\"range\":\""<<fixture.range<<"\",\"requested_decoder_mode\":\""<<mode
     <<"\",\"decoder_path\":\""<<path<<"\",\"plane\":"<<plane<<",\"width\":"<<extent.width<<",\"height\":"<<extent.height
     <<",\"format\":\"R8_UNORM\",\"encoded_sha256\":\""<<hash(fixture.encoded)<<"\",\"cpu_sha256\":\""<<hash(a)
     <<"\",\"gpu_sha256\":\""<<hash(b)<<"\",\"exact_equal\":"<<(a==b ? "true" : "false")<<",\"metrics\":"
     <<differenceJson(differences(a,b,extent.width,extent.height))<<'}'; return s.str();
}
// Both modes borrow the SAME caller device and use fixtures generated ONCE.
// Wrapper recreation preserves the required decoder -> wrapper -> outputs order.
inline bool runDiagnostics(PyroWave::Runtime& candidate,PyroWave::Runtime& reference,
    const pyrowave_device_create_info& info,const std::filesystem::path& runtimeDir,PyroWaveVulkan::Probe& owner,
    PyroWaveVulkan::Dispatch& vk,NativeDispatch& native,QueueLock& lock,const PyroWaveVulkan::Log& log,
    const std::filesystem::path& directory,const Hash& hash,bool prefill,bool deviceIdle) {
    auto fixtures=diagnosticFixtures(reference);
    std::array<std::vector<PyroWave::Pixels>,2> baseline;
    std::array<bool,2> exact{true,true}; std::array<std::string,2> paths;
    std::vector<std::string> repeats,cross;
    bool stable=true,experimentsExact=true;
    candidate.close();
    for(unsigned mode=0;mode<2;++mode) {
        const std::string requested=mode ? "FORCE_COMPUTE" : "AUTO",file=mode ? "forced-compute" : "auto-fragment";
        if(!candidate.load(runtimeDir) || !candidate.borrowDevice(info)) throw std::runtime_error(candidate.error());
        const bool preferred=candidate.nativePrefersFragment();
        if(!candidate.createDecoder(1920,1080,mode==0)) throw std::runtime_error(candidate.error());
        paths[mode]=candidate.decoderPath();
        const auto selected=selectedPath(preferred,mode!=0);
        if(paths[mode]!=(selected==Path::Fragment ? "fragment" : "compute")) throw std::runtime_error("diagnostic decoder path assertion failed");
        log("requested_decoder_mode="+requested+" preferred_decoder_path="+(preferred ? "fragment" : "compute")+" actual_decoder_path="+paths[mode]);
        log("borrowed_instance_match=YES borrowed_physical_match=YES borrowed_device_match=YES diagnostic_same_caller_device=YES");
        auto outputs=std::make_unique<Outputs>(owner,vk,native,lock,log,selected);
        std::vector<std::string> comparisons; bool dumped=false;
        try {
            outputs->initialize("");
            auto decode=[&](const DiagnosticFixture& f,bool idle=false) {
                auto views=outputs->views(0);
                auto acquire=outputs->acquire(0),release=outputs->release(0);
                if(!candidate.decodeNative(f.encoded,views,acquire,release)) throw std::runtime_error(candidate.error());
                outputs->submitted(0); return outputs->read(0,idle);
            };
            for(unsigned frame=0;frame<fixtures.size();++frame) {
                const auto& f=fixtures[frame]; auto gpu=decode(f); bool mismatch=false;
                for(unsigned p=0;p<3;++p) {
                    const auto record=diagnosticPlane(f,frame,p,requested,paths[mode],f.cpu.planes[p],gpu.planes[p],hash);
                    comparisons.push_back(record); log("diagnostic_metrics="+record);
                    mismatch|=f.cpu.planes[p]!=gpu.planes[p];
                }
                exact[mode]=exact[mode] && !mismatch;
                if(mismatch && !dumped) {
                    // At most one frame (all three planes) per mode, ~6 MiB.
                    std::vector<std::string> metadata;
                    for(unsigned p=0;p<3;++p) {
                        diagnosticBinary(directory/(file+"-cpu-plane"+std::to_string(p)+".bin"),f.cpu.planes[p]);
                        diagnosticBinary(directory/(file+"-gpu-plane"+std::to_string(p)+".bin"),gpu.planes[p]);
                        metadata.push_back(comparisons[comparisons.size()-3+p]);
                    }
                    diagnosticFile(directory/(file+"-dump-metadata.json"),jsonArray(metadata)); dumped=true;
                }
                if(mismatch) {
                    std::array<std::vector<std::string>,3> hashes;
                    for(unsigned p=0;p<3;++p) hashes[p].push_back(hash(gpu.planes[p]));
                    for(unsigned repeat=1;repeat<=4;++repeat) {
                        auto again=decode(f);
                        for(unsigned p=0;p<3;++p) {
                            hashes[p].push_back(hash(again.planes[p]));
                            const auto record=diagnosticPlane(f,frame,p,requested,paths[mode],f.cpu.planes[p],again.planes[p],hash);
                            repeats.push_back("{\"repeat\":"+std::to_string(repeat)+",\"comparison\":"+record+"}");
                            exact[mode]=exact[mode] && f.cpu.planes[p]==again.planes[p];
                        }
                    }
                    for(unsigned p=0;p<3;++p) {
                        const bool same=repeatHashStable(hashes[p]); stable=stable && same;
                        std::vector<std::string> quoted; for(const auto& h:hashes[p]) quoted.push_back("\""+h+"\"");
                        repeats.push_back("{\"fixture\":\""+f.name+"\",\"mode\":\""+requested+"\",\"plane\":"+std::to_string(p)+
                            ",\"slot\":0,\"identical_decodes\":5,\"repeat_hash_stable\":\""+(same ? "YES" : "NO")+"\",\"gpu_sha256\":"+jsonArray(quoted)+"}");
                        log("repeat_hash_stable="+std::string(same ? "YES" : "NO")+" fixture="+f.name+" requested_decoder_mode="+requested+" plane="+std::to_string(p));
                    }
                }
                if(deviceIdle) {
                    auto idle=decode(f,true);
                    for(unsigned p=0;p<3;++p) {
                        const auto record=diagnosticPlane(f,frame,p,requested,paths[mode],f.cpu.planes[p],idle.planes[p],hash);
                        repeats.push_back("{\"diagnostic_device_idle\":true,\"normal_gpu_hash_equal\":"+std::string(gpu.planes[p]==idle.planes[p] ? "true" : "false")+",\"comparison\":"+record+"}");
                        experimentsExact=experimentsExact && f.cpu.planes[p]==idle.planes[p];
                        log("device_idle_changed_bytes="+std::string(gpu.planes[p]==idle.planes[p] ? "NO" : "YES")+" fixture="+f.name+" requested_decoder_mode="+requested+" plane="+std::to_string(p));
                    }
                }
                baseline[mode].push_back(std::move(gpu));
            }
            if(prefill) {
                // Keep baseline output usages EXACTLY 9/17. Recreate diagnostic
                // images separately with TRANSFER_DST for the clear experiment.
                outputs->drain(); candidate.close(); outputs->close(); outputs.reset();
                if(!candidate.load(runtimeDir) || !candidate.borrowDevice(info) || !candidate.createDecoder(1920,1080,mode==0))
                    throw std::runtime_error(candidate.error());
                outputs=std::make_unique<Outputs>(owner,vk,native,lock,log,selected,true); outputs->initialize("");
                log("diagnostic_prefill_resources=SEPARATE_WITH_TRANSFER_DST base_output_usage="+std::to_string(usage(selected)));
                for(unsigned frame=0;frame<fixtures.size();++frame) for(uint8_t value:{uint8_t(0xa5),uint8_t(0x5a)}) {
                    const auto& f=fixtures[frame]; outputs->prefill(0,value); auto filled=decode(f);
                    for(unsigned p=0;p<3;++p) {
                        uint64_t remains=0,unexpected=0;
                        for(size_t i=0;i<filled.planes[p].size();++i) if(filled.planes[p][i]==value) { ++remains; if(f.cpu.planes[p][i]!=value) ++unexpected; }
                        const auto record=diagnosticPlane(f,frame,p,requested,paths[mode],f.cpu.planes[p],filled.planes[p],hash);
                        repeats.push_back("{\"prefill_value\":"+std::to_string(value)+",\"remaining_prefill_bytes\":"+std::to_string(remains)+
                            ",\"prefill_candidates_not_equal_to_cpu\":"+std::to_string(unexpected)+",\"normal_gpu_hash_equal\":"+(baseline[mode][frame].planes[p]==filled.planes[p] ? "true" : "false")+",\"comparison\":"+record+"}");
                        experimentsExact=experimentsExact && f.cpu.planes[p]==filled.planes[p];
                    }
                }
            }
            diagnosticFile(directory/(file+".json"),"{\"requested_decoder_mode\":\""+requested+"\",\"preferred_decoder_path\":\""+(preferred ? "fragment" : "compute")+
                "\",\"actual_decoder_path\":\""+paths[mode]+"\",\"exact_equal\":"+(exact[mode] ? "true" : "false")+",\"planes\":"+jsonArray(comparisons)+"}");
            diagnosticFile(directory/(file+".log"),"requested_decoder_mode="+requested+" actual_decoder_path="+paths[mode]+"\n"+diagnosticLines(comparisons));
            outputs->drain(); candidate.close(); outputs->close();
        } catch(...) {
            if(outputs) { try { outputs->drain(); } catch(const std::exception& e) { log(std::string("cleanup_drain_error=")+e.what()); } }
            candidate.close(); if(outputs) outputs->close(); throw;
        }
    }
    bool autoComputeEqual=true;
    for(unsigned f=0;f<fixtures.size();++f) for(unsigned p=0;p<3;++p) {
        autoComputeEqual=autoComputeEqual && baseline[0][f].planes[p]==baseline[1][f].planes[p];
        auto record=diagnosticPlane(fixtures[f],f,p,"AUTO_VS_FORCE_COMPUTE",paths[0]+"_vs_"+paths[1],baseline[0][f].planes[p],baseline[1][f].planes[p],hash);
        cross.push_back("{\"left_output\":\"AUTO\",\"right_output\":\"FORCE_COMPUTE\",\"comparison\":"+record+"}");
    }
    diagnosticFile(directory/"cross-comparison.json",jsonArray(cross));
    diagnosticFile(directory/"repeatability.json","{\"repeat_hash_stable\":\""+std::string(stable ? "YES" : "NO")+"\",\"records\":"+jsonArray(repeats)+"}");
    diagnosticFile(directory/"repeatability.log",diagnosticLines(repeats));
    const bool okay=exact[0] && exact[1] && stable && experimentsExact;
    const std::string qualification=okay ? "TARGETED_EXACT_MATCH_FULL_SUITE_PENDING" : !stable ? "FAIL_NONDETERMINISTIC_OUTPUT" :
        !exact[0] ? (paths[0]=="fragment" ? "FAIL_FRAGMENT_MISMATCH" : "FAIL_AUTO_COMPUTE_MISMATCH") : !exact[1] ? "FAIL_FORCE_COMPUTE_MISMATCH" : "FAIL_DIAGNOSTIC_EXPERIMENT";
    diagnosticFile(directory/"diagnostic-summary.json","{\"sourceRevision\":\"" STAGE3_SOURCE_REVISION "\",\"overall\":\"DIAGNOSTIC_COMPLETE\",\"stage3Qualification\":\""+qualification+
        "\",\"autoExact\":"+(exact[0] ? "true" : "false")+",\"forcedComputeExact\":"+(exact[1] ? "true" : "false")+
        ",\"autoVsForcedComputeExact\":"+(autoComputeEqual ? "true" : "false")+",\"repeatHashStable\":"+(stable ? "true" : "false")+
        ",\"experimentsExact\":"+(experimentsExact ? "true" : "false")+",\"sameCallerDevice\":true,\"identicalEncodedFixtures\":true,\"fixtureCount\":7}");
    log("diagnostic_complete=YES stage3_qualification="+qualification+" comparison_criterion=EXACT_BYTES");
    return okay;
}
}
