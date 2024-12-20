#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

layout(location = 0) in vec4 v2point;
layout(location = 1) in vec2 tangent;
layout(location = 2) in float dir;

layout(set = 0, binding = 1) restrict coherent buffer WindingBuffer {
    int coverage[];
};

float signed_area(vec2 p, vec2 t)
{
    vec2 v0 = v2point.xy;
    vec2 v1 = v2point.zw;
    vec2 v = p - v0;
    float d = t.y*v.x - t.x*v.y;
    vec2 window = clamp(vec2(v0.x - p.x, v1.x - p.x), -0.5f, 0.5f);
    float width = window.y - window.x;
    d = clamp(d + 0.5, 0.0, 1.0);
    return d * width * dir;
}

void main() {
    vec2 frag_coord = gl_FragCoord.xy;
    uvec2 orig_coord = uvec2(frag_coord - min_bb);
    uint winding_idx = orig_coord.x + orig_coord.y * winding_stride;
    int area = int(signed_area(frag_coord, tangent) * 256.0);
    atomicAdd(coverage[winding_offset + winding_idx], area);
}
