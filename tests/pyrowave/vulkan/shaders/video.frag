#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(set=0,binding=0) uniform sampler2D planeY;
layout(set=0,binding=1) uniform sampler2D planeU;
layout(set=0,binding=2) uniform sampler2D planeV;
layout(push_constant) uniform Parameters { ivec4 rect; int limited; int nearest; } params;
float plane(sampler2D image) {
    ivec2 size = textureSize(image, 0);
    // Integer rational mapping fixes nearest ties and avoids interpolator precision.
    ivec2 pixel = ivec2(gl_FragCoord.xy) - params.rect.xy;
    if (params.nearest != 0) {
        ivec2 index = ((2 * pixel + 1) * size) / (2 * params.rect.zw);
        return texelFetch(image, clamp(index, ivec2(0), size - 1), 0).r;
    }
    // Explicit bilinear reconstruction avoids device-dependent sampler fraction
    // precision at high-contrast chroma edges. All four taps clamp to edge.
    vec2 position = (vec2(pixel) + 0.5) * vec2(size) / vec2(params.rect.zw) - 0.5;
    ivec2 base = ivec2(floor(position));
    vec2 fraction = fract(position);
    float a = texelFetch(image, clamp(base, ivec2(0), size - 1), 0).r;
    float b = texelFetch(image, clamp(base + ivec2(1, 0), ivec2(0), size - 1), 0).r;
    float c = texelFetch(image, clamp(base + ivec2(0, 1), ivec2(0), size - 1), 0).r;
    float d = texelFetch(image, clamp(base + ivec2(1, 1), ivec2(0), size - 1), 0).r;
    return mix(mix(a, b, fraction.x), mix(c, d, fraction.x), fraction.y);
}
void main() {
    // Equal normalized UV implements CENTER, not LEFT chroma (see STAGE4.md).
    float y = plane(planeY);
    float cb = plane(planeU);
    float cr = plane(planeV);
    if (params.limited != 0) {
        y = (y * 255.0 - 16.0) / 219.0;
        cb = (cb * 255.0 - 128.0) / 224.0;
        cr = (cr * 255.0 - 128.0) / 224.0;
    } else {
        cb -= 0.5;
        cr -= 0.5;
    }
    vec3 rgb = vec3(y + 1.5748 * cr,
                   y - 0.187324 * cb - 0.468124 * cr,
                   y + 1.8556 * cb);
    color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
