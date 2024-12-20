#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "vk_common.glsli"

void main() {
    uint index = gl_VertexIndex;
    float x = (index & 1) == 1 ? ceil(max_bb.x) : floor(min_bb.x);
    float y = ((index >> (index / 3 + 1)) & 1) == 1 ? ceil(max_bb.y) : floor(min_bb.y);
    gl_Position.x = x * inv_viewport.x - 1.0f;
    gl_Position.y = y * inv_viewport.y - 1.0f;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
}
