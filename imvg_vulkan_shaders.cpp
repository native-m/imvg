#include <cstdint>
#include "shader/vk_fill.vs.h"
#include "shader/vk_fill.fs.h"
#include "shader/vk_polygon.vs.h"
#include "shader/vk_polygon.fs.h"

uint32_t __spirv_vulkan_fill_vs_size = sizeof(__spirv_vulkan_fill_vs);
uint32_t __spirv_vulkan_fill_fs_size = sizeof(__spirv_vulkan_fill_fs);
const uint32_t* __spirv_vulkan_fill_vs_shader = __spirv_vulkan_fill_vs;
const uint32_t* __spirv_vulkan_fill_fs_shader = __spirv_vulkan_fill_fs;

uint32_t __spirv_vulkan_polygon_vs_size = sizeof(__spirv_vulkan_polygon_vs);
uint32_t __spirv_vulkan_polygon_fs_size = sizeof(__spirv_vulkan_polygon_fs);
const uint32_t* __spirv_vulkan_polygon_vs_shader = __spirv_vulkan_polygon_vs;
const uint32_t* __spirv_vulkan_polygon_fs_shader = __spirv_vulkan_polygon_fs;
