glslang -S vert -V100 -g -gVS -o vk_fill.vs.h --vn __spirv_vulkan_fill_vs vk_fill.vs
glslang -S frag -V100 -g -gVS -o vk_fill.fs.h --vn __spirv_vulkan_fill_fs vk_fill.fs

glslang -S vert -V100 -g -gVS -o vk_polygon.vs.h --vn __spirv_vulkan_polygon_vs vk_polygon.vs
glslang -S frag -V100 -g -gVS -o vk_polygon.fs.h --vn __spirv_vulkan_polygon_fs vk_polygon.fs
