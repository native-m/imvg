#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

layout(location = 0) in vec2 point_a;
layout(location = 1) in vec2 point_b;

layout(set = 0, binding = 1) restrict coherent buffer WindingBuffer {
    int coverage[];
};

// This produce incorrect edge
// float signed_area(vec2 p, vec2 t) {
//     vec2 v0 = point_window.xy;
//     vec2 window = point_window.zw;
//     vec2 v = p - v0;
//     float d = t.y*v.x - t.x*v.y;
//     vec2 window_area = clamp(vec2(window.y - p.x, window.x - p.x), -0.5f, 0.5f);
//     float width = window_area.y - window_area.x;
//     d = clamp(d, 0.0, 1.0);
//     return d * width;
// }

// Referenced from nanovgXC with slight modifications
float signed_area2(vec2 p, vec2 va, vec2 vb) {
    vec2 v0 = va - p;
    vec2 v1 = vb - p;
    vec2 window = clamp(vec2(v0.x, v1.x), -0.5f, 0.5f);
    float width = window.y - window.x;
    vec2 dv = v1 - v0;
    if (abs(dv.y) > 0.0) {
        float slope = dv.y/dv.x;
        float midx = 0.5f*(window.x + window.y);
        float y = v0.y + (midx - v0.x) * slope;
        float dy = abs(slope*width);
        vec4 sides = vec4(y + 0.5f*dy, y - 0.5f*dy, (0.5f - y)/dy, (-0.5f - y)/dy);
        sides = clamp(sides + 0.5f, 0.0f, 1.0f);
        float area = 0.5f*(sides.z - sides.z*sides.y - 1.0f - sides.x + sides.x*sides.w);
        return width == 0.0f ? 0.0f : area * width;
    }
    return clamp(v0.y + 0.5, 0.0, 1.0) * width;
}

void main() {
    vec2 frag_coord = gl_FragCoord.xy;
    uvec2 orig_coord = uvec2(frag_coord - floor(min_bb));
    uint winding_idx = orig_coord.x + orig_coord.y * winding_stride;
    int area = int(signed_area2(frag_coord, point_b, point_a) * 256.0);
    atomicAdd(coverage[winding_offset + winding_idx], area);
}
