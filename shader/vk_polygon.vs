#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

layout(set = 0, binding = 0) readonly buffer VertexBuffer {
    vec2 points[];
} vertex_input;

layout(location = 0) out vec4 v2point;
layout(location = 1) out vec2 tangent;
layout(location = 2) out float dir;

void main() {
    uint instance = gl_VertexIndex / 6 + vtx_offset;
    uint index = gl_VertexIndex % 6;
    vec2 v0 = vertex_input.points[instance];
    vec2 v1 = vertex_input.points[instance + 1];
    
    if (v1.x < v0.x) {
        vec2 tmp_v0 = v0;
        v0 = v1;
        v1 = tmp_v0;
        dir = 1.0;
    } else {
        dir = -1.0;
    }

    float x = (index & 1) == 1 ? ceil(v1.x) : floor(v0.x);
    float y = ((index >> (index / 3 + 1)) & 1) == 1 ? ceil(max(v1.y, v0.y)) : floor(min_bb.y);

    tangent = normalize(v1 - v0);
    v2point = vec4(v0, v1);

    gl_Position.x = x * inv_viewport.x - 1.0f;
    gl_Position.y = y * inv_viewport.y - 1.0f;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
}
