#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

layout(set = 0, binding = 0) readonly buffer VertexBuffer {
    vec2 points[];
} vertex_input;

layout(location = 0) out vec2 va;
layout(location = 1) out vec2 vb;

void main() {
    uint instance = gl_VertexIndex / 6 + vtx_offset;
    uint index = gl_VertexIndex % 6;
    vec2 v0 = vertex_input.points[instance];
    vec2 v1 = vertex_input.points[instance + 1];
    vec2 window = vec2(v0.x, v1.x);
    va = v0;
    vb = v1;

    if (v1.x < v0.x) {
        vec2 tmp_v0 = v0;
        v0 = v1;
        v1 = tmp_v0;
    }

    float x = (index & 1) == 1 ? ceil(v1.x) : floor(v0.x);
    float y = ((index >> (index / 3 + 1)) & 1) == 1 ? ceil(max(v1.y, v0.y)) : floor(min_bb.y);

    gl_Position.x = x * inv_viewport.x - 1.0f;
    gl_Position.y = y * inv_viewport.y - 1.0f;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
}
