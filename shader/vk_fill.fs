#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

layout(set = 0, binding = 1) restrict coherent buffer WindingBuffer {
    int coverage[];
};

layout(location = 0) out vec4 out_color;

void main() {
    uvec2 orig_coord = uvec2(gl_FragCoord.xy - floor(min_bb));
    uint winding_idx = orig_coord.x + orig_coord.y * winding_stride;
    int coverage = atomicExchange(coverage[winding_offset + winding_idx], 0);
    float a = float(coverage) / 256.0;
    out_color = unpackUnorm4x8(color);
    out_color.a *= min(abs(a), 1.0);
}
