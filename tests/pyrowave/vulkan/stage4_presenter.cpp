#include "stage4_presenter.h"
#include "stage4_policy.h"
#include <cstring>
using PyroWaveVulkan::check;
namespace Stage4 {
void Presenter::exactUpload(const PyroWave::Pixels& p) {
    void* mapped=nullptr; check(native.MapMemory(owner.deviceHandle(),upload.allocation,0,VK_WHOLE_SIZE,0,&mapped),"test exact-plane upload map");
    size_t offset=0; for(const auto& plane:p.planes) { std::memcpy(static_cast<uint8_t*>(mapped)+offset,plane.data(),plane.size()); offset+=plane.size(); }
    VkResult flushed=VK_SUCCESS;
    if(!upload.coherent) { VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=upload.allocation; range.size=VK_WHOLE_SIZE; flushed=gpu.FlushMappedMemoryRanges(owner.deviceHandle(),1,&range); }
    native.UnmapMemory(owner.deviceHandle(),upload.allocation); check(flushed,"test upload flush");
    begin();
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    const auto planes=Stage3::planes(1920,1080); offset=0;
    for(unsigned i=0;i<3;++i) { VkBufferImageCopy copy{}; copy.bufferOffset=offset; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={planes[i].width,planes[i].height,1};
        gpu.CmdCopyBufferToImage(command,upload.buffer,slots[3].planes[i].image,VK_IMAGE_LAYOUT_GENERAL,1,&copy); offset+=planes[i].bytes; }
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    finish();
}
void Presenter::verify(const std::filesystem::path& evidence) {
    std::ofstream results(evidence/"shader-verification.json"),points(evidence/"color-reference.json");
    if(!results || !points) throw std::runtime_error("verification evidence unavailable");
    results << "{\"path\":\"TEST_VERIFICATION_ONLY\",\"rgbTolerance\":1,\"cases\":[";
    points << "{\"reference\":\"independent Kr/Kb double arithmetic\",\"observations\":[";
    bool first=true,firstPoint=true;
    for(auto range:{PyroWave::YuvRange::Full,PyroWave::YuvRange::Limited})
    for(auto pattern:{Presentation::Pattern::Range,Presentation::Pattern::Bars,Presentation::Pattern::Chroma,Presentation::Pattern::Geometry,Presentation::Pattern::Gradient}) {
        const auto p=fixture(pattern,range); exactUpload(p);
        for(auto filter:{Filter::Nearest,Filter::Linear}) for(auto extent:{VkExtent2D{1920,1080},VkExtent2D{1001,751},VkExtent2D{1801,700},VkExtent2D{127,93}}) {
            begin(); VkClearValue black{}; black.color.float32[3]=1;
            VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; pass.renderPass=verifyPass; pass.framebuffer=verifyFramebuffer; pass.renderArea.extent=extent; pass.clearValueCount=1; pass.pClearValues=&black;
            vk.CmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE); draw(command,verifyPass,VK_FORMAT_R8G8B8A8_UNORM,extent,3,filter,range); vk.CmdEndRenderPass(command);
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={extent.width,extent.height,1};
            native.CmdCopyImageToBuffer(command,target.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging.buffer,1,&copy);
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr); finish();
            void* mapped=nullptr; check(native.MapMemory(owner.deviceHandle(),staging.allocation,0,VK_WHOLE_SIZE,0,&mapped),"test RGB readback map");
            VkResult invalidated=VK_SUCCESS;
            if(!staging.coherent) { VkMappedMemoryRange mr{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; mr.memory=staging.allocation; mr.size=VK_WHOLE_SIZE; invalidated=native.InvalidateMappedMemoryRanges(owner.deviceHandle(),1,&mr); }
            std::vector<uint8_t> rgb;
            if(invalidated==VK_SUCCESS) rgb.assign(static_cast<uint8_t*>(mapped),static_cast<uint8_t*>(mapped)+size_t(extent.width)*extent.height*4);
            native.UnmapMemory(owner.deviceHandle(),staging.allocation); check(invalidated,"test RGB invalidate");
            const auto fit=Presentation::fit(int(extent.width),int(extent.height)); unsigned maximum=0; uint64_t mismatches=0,leftDifferences=0;
            for(unsigned y=0;y<extent.height;++y) for(unsigned x=0;x<extent.width;++x) {
                Rgb expected{}; bool inside=int(x)>=fit.x && int(y)>=fit.y && int(x)<fit.x+fit.w && int(y)<fit.y+fit.h;
                const double u=inside ? (double(x)-fit.x+0.5)/fit.w : 0, v=inside ? (double(y)-fit.y+0.5)/fit.h : 0;
                if(inside && filter==Filter::Nearest) {
                    const auto lx=(uint64_t(2*(int(x)-fit.x)+1)*1920)/(2*fit.w),ly=(uint64_t(2*(int(y)-fit.y)+1)*1080)/(2*fit.h);
                    const auto cx=(uint64_t(2*(int(x)-fit.x)+1)*960)/(2*fit.w),cy=(uint64_t(2*(int(y)-fit.y)+1)*540)/(2*fit.h);
                    expected=reference(p.planes[0].at(ly*1920+lx),p.planes[1].at(cy*960+cx),p.planes[2].at(cy*960+cx),p.range);
                } else if(inside) expected=referencePixel(p,u,v,filter);
                const size_t offset=(size_t(y)*extent.width+x)*4;
                for(unsigned c=0;c<3;++c) { const unsigned error=unsigned(std::abs(int(rgb[offset+c])-expected[c])); maximum=std::max(maximum,error); if(error>RgbTolerance) ++mismatches; }
                if(rgb[offset+3]!=255) ++mismatches;
                if(inside && pattern==Presentation::Pattern::Chroma) {
                    const auto left=referencePixel(p,u,v,filter,true);
                    for(unsigned c=0;c<3;++c) if(std::abs(int(left[c])-expected[c])>2*int(RgbTolerance)) { ++leftDifferences; break; }
                }
                if(inside && y==unsigned(fit.y+fit.h/2) && x%std::max(1u,extent.width/16)==0) {
                    if(!firstPoint) points<<','; firstPoint=false;
                    points<<"{\"pattern\":\""<<Presentation::name(pattern)<<"\",\"range\":\""<<(range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")<<"\",\"x\":"<<x<<",\"y\":"<<y<<",\"expected\":["<<int(expected[0])<<','<<int(expected[1])<<','<<int(expected[2])<<"],\"observed\":["<<int(rgb[offset])<<','<<int(rgb[offset+1])<<','<<int(rgb[offset+2])<<"]}";
                }
            }
            maxRgbError=std::max(maxRgbError,maximum); verifiedPixels+=uint64_t(extent.width)*extent.height; chromaNegativeControls+=leftDifferences;
            if(!first) results<<','; first=false;
            results<<"{\"pattern\":\""<<Presentation::name(pattern)<<"\",\"range\":\""<<(range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")<<"\",\"filter\":\""<<(filter==Filter::Linear ? "LINEAR" : "NEAREST")<<"\",\"width\":"<<extent.width<<",\"height\":"<<extent.height<<",\"maxError\":"<<maximum<<",\"failedComponents\":"<<mismatches<<",\"leftNegativeControlPixels\":"<<leftDifferences<<"}";
            log("TEST_VERIFICATION pattern="+std::string(Presentation::name(pattern))+" max_rgb_error="+std::to_string(maximum)+" failed_components="+std::to_string(mismatches));
            if(mismatches) { results<<"],\"pass\":false}"; points<<"]}"; throw std::runtime_error("shader RGB tolerance failure; STOP"); }
        }
    }
    results<<"],\"maxError\":"<<maxRgbError<<",\"verifiedPixels\":"<<verifiedPixels<<",\"chromaNegativeControlPixels\":"<<chromaNegativeControls<<",\"pass\":"<<(chromaNegativeControls ? "true" : "false")<<"}"; points<<"]}";
    if(!chromaNegativeControls) throw std::runtime_error("ambiguous CENTER/LEFT test; STOP");
}
}
