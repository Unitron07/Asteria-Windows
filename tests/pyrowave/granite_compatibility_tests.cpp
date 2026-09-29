#include <cstdint>
#include <iostream>
#include "simd.hpp"
#include "bitops.hpp"

int main() {
    using namespace Granite;
    const vec4 rows[3] = {vec4(1,2,3,4),vec4(5,6,7,8),vec4(9,10,11,12)};
    mat4 result;
    muglm::transpose_from_affine(result,rows);
    for (int col=0;col<4;++col) for (int row=0;row<4;++row) {
        const float expected=row<3 ? rows[row][col] : (col==3 ? 1.0f : 0.0f);
        if (result[col][row]!=expected) return 1;
    }
    const AABB box(vec3(-1,-2,-3),vec3(1,2,3));
    vec4 planes[6]={vec4(1,0,0,2),vec4(-1,0,0,2),vec4(0,1,0,3),
                   vec4(0,-1,0,3),vec4(0,0,1,4),vec4(0,0,-1,4)};
    if (!SIMD::frustum_cull(box,planes)) return 1;
    planes[0]=vec4(1,0,0,-2);
    if (SIMD::frustum_cull(box,planes)) return 1;
    // Exercise the actual target compiler's bitops, including _WIN64 on ARM64.
    if (Util::popcount32(0)!=0 || Util::popcount32(0xffffffffu)!=32 ||
        Util::popcount64(0xffffffffffffffffull)!=64 || Util::popcount64(1ull<<63)!=1 ||
        Util::leading_zeroes64(0)!=64 || Util::trailing_zeroes64(0)!=64 ||
        Util::leading_zeroes64(1)!=63 || Util::trailing_zeroes64(1ull<<63)!=63) return 1;
    std::cout << "PASS: affine transpose, frustum cull, native MSVC popcount/bit scans\n";
}
