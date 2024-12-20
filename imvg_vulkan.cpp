#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "imvg_vulkan.h"
#include <deque>
#include <unordered_map>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifndef NDEBUG
#define IVG_VK_CHECK(x) IVG_ASSERT((x) >= VK_SUCCESS)
#else
#define IVG_VK_CHECK(x) x
#endif
#define IVG_VK_FAILED(x) ((x) < VK_SUCCESS)
#define MAX_FRAMES_IN_FLIGHT 3

extern uint32_t __spirv_vulkan_fill_vs_size;
extern uint32_t __spirv_vulkan_fill_fs_size;
extern const uint32_t* __spirv_vulkan_fill_vs_shader;
extern const uint32_t* __spirv_vulkan_fill_fs_shader;

extern uint32_t __spirv_vulkan_polygon_vs_size;
extern uint32_t __spirv_vulkan_polygon_fs_size;
extern const uint32_t* __spirv_vulkan_polygon_vs_shader;
extern const uint32_t* __spirv_vulkan_polygon_fs_shader;

struct IvgBackendVulkanBuffer
{
    VkDeviceMemory allocation;
    VkBuffer buffer;
    VkDeviceSize size;
    void* mapped_ptr;
    uint32_t count;
    uint32_t offset;
};

struct IvgBackendVulkanImage
{
    VkDeviceMemory allocation;
    VkImage image;
};

struct IvgBackendVulkanDispose
{
    enum Type
    {
        Buffer,
        Image,
    };

    uint64_t frame_stamp;
    uint32_t type;

    union
    {
        IvgBackendVulkanBuffer buffer;
        IvgBackendVulkanImage image;
    };
};

struct IvgBackendVulkanDrawArgs
{
    IvgV2 inv_viewport;
    IvgV2 min_bb;
    IvgV2 max_bb;
    uint32_t vtx_offset;
    uint32_t winding_stride;
    uint32_t winding_offset;
    uint32_t fill_mode;
    uint32_t paint_type;
    uint32_t color;
};

struct IvgBackendVulkanPipeline
{
    VkPipeline polygon;
    VkPipeline fill;
};

struct IvgBackendVulkanDescriptorStream
{
    VkDescriptorPool pool;
    VkDescriptorSet current_descriptor_set;
};

struct IvgBackendVulkan
{
    IvgBackendVulkanFn fn;
    uint32_t num_frames_in_flight;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkRenderPass compatible_render_pass;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    uint64_t frame_count = 0;
    uint32_t frame_id = 0;
    VkDeviceSize min_allocation_size;
    VkDeviceSize min_winding_buffer_size;
    VkDeviceSize buffer_alignment = 256;
    VkDeviceSize area_covered = 0;
    uint32_t winding_offset = 0;
    IvgBackendVulkanDescriptorStream descriptor_stream[MAX_FRAMES_IN_FLIGHT]{};
    IvgBackendVulkanBuffer vtx_buffer[MAX_FRAMES_IN_FLIGHT]{};
    IvgBackendVulkanBuffer winding_buffer[MAX_FRAMES_IN_FLIGHT]{};
    std::unordered_map<VkRenderPass, IvgBackendVulkanPipeline> pipeline_cache;
    std::deque<IvgBackendVulkanDispose> resource_disposal_queue;
};

static uint32_t GetMemoryType(IvgBackendVulkanFn& fn, VkPhysicalDevice physical_device, VkMemoryPropertyFlags properties,
                              uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    fn.vkGetPhysicalDeviceMemoryProperties(physical_device, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}

static inline VkDeviceSize AlignBufferSize(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

static void CreateOrResizeBuffer(IvgBackendVulkan* backend, IvgBackendVulkanBuffer& buffer, VkDeviceSize new_size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props)
{
    IvgBackendVulkanFn& fn = backend->fn;
    if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != VK_NULL_HANDLE) {
        IvgBackendVulkanDispose& disposal = backend->resource_disposal_queue.emplace_back();
        disposal.frame_stamp = backend->frame_count;
        disposal.type = IvgBackendVulkanDispose::Buffer;
        disposal.buffer = buffer;
    }

    // Prefer aligned buffer size
    VkDeviceSize buffer_size_aligned = AlignBufferSize(IvgMax(backend->min_allocation_size, new_size), backend->buffer_alignment);
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = fn.vkCreateBuffer(backend->device, &buffer_info, nullptr, &buffer.buffer);
    IVG_VK_CHECK(result);

    VkMemoryRequirements req;
    fn.vkGetBufferMemoryRequirements(backend->device, buffer.buffer, &req);
    backend->buffer_alignment = (backend->buffer_alignment > req.alignment) ? backend->buffer_alignment : req.alignment;

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = GetMemoryType(backend->fn, backend->physical_device, mem_props, req.memoryTypeBits);
    IVG_VK_CHECK(fn.vkAllocateMemory(backend->device, &alloc_info, nullptr, &buffer.allocation));
    IVG_VK_CHECK(fn.vkBindBufferMemory(backend->device, buffer.buffer, buffer.allocation, 0));
    buffer.size = buffer_size_aligned;
}

static void DestroyResource(IvgBackendVulkan* backend, uint64_t inflight_frames, uint64_t frame_count)
{
    VkDevice device = backend->device;
    IvgBackendVulkanFn& fn = backend->fn;
    while (!backend->resource_disposal_queue.empty()) {
        auto& item = backend->resource_disposal_queue.front();
        if (item.frame_stamp + inflight_frames < frame_count) {
            switch (item.type) {
                case IvgBackendVulkanDispose::Buffer:
                    fn.vkDestroyBuffer(device, item.buffer.buffer, nullptr);
                    fn.vkFreeMemory(device, item.buffer.allocation, nullptr);
                    break;
                case IvgBackendVulkanDispose::Image:
                    fn.vkDestroyImage(device, item.image.image, nullptr);
                    fn.vkFreeMemory(device, item.image.allocation, nullptr);
                    break;
            }
            backend->resource_disposal_queue.pop_front();
        }
        else {
            break;
        }
    }
}

static VkPipeline CreatePipeline(IvgBackendVulkan* backend, const uint32_t* vs_bytecode, uint32_t vs_size,
                                 const uint32_t* fs_bytecode, uint32_t fs_size, bool enable_blend, bool color_write,
                                 VkPrimitiveTopology topology, VkPipelineLayout layout, VkRenderPass render_pass)
{
    IvgBackendVulkanFn& fn = backend->fn;
    VkShaderModule vs_module;
    VkShaderModule fs_module;

    VkShaderModuleCreateInfo vs_info;
    vs_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vs_info.pNext = nullptr;
    vs_info.flags = 0;
    vs_info.codeSize = vs_size;
    vs_info.pCode = vs_bytecode;
    if (IVG_VK_FAILED(fn.vkCreateShaderModule(backend->device, &vs_info, nullptr, &vs_module)))
        return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo fs_info;
    fs_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fs_info.pNext = nullptr;
    fs_info.flags = 0;
    fs_info.codeSize = fs_size;
    fs_info.pCode = fs_bytecode;
    if (IVG_VK_FAILED(fn.vkCreateShaderModule(backend->device, &fs_info, nullptr, &fs_module))) {
        fn.vkDestroyShaderModule(backend->device, vs_module, nullptr);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stages[2];
    // vs
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = nullptr;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs_module;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = nullptr;
    // fs
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = nullptr;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs_module;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = nullptr;

    VkPipelineVertexInputStateCreateInfo vtx_input{};
    vtx_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = topology;

    VkPipelineViewportStateCreateInfo viewport;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.pNext = nullptr;
    viewport.flags = 0;
    viewport.viewportCount = 1;
    viewport.pViewports = nullptr;
    viewport.scissorCount = 1;
    viewport.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.pNext = nullptr;
    rasterizer.flags = 0;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = enable_blend;

    if (enable_blend) {
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    if (color_write)
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    else
        attachment.colorWriteMask = 0;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;

    static const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };

    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 4;
    dynamic.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = {};
    pipeline_info.flags = {};
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vtx_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = {};
    pipeline_info.pViewportState = &viewport;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = {};
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.pDynamicState = &dynamic;
    pipeline_info.layout = layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = {};
    pipeline_info.basePipelineIndex = {};

    VkPipeline pipeline;
    if (IVG_VK_FAILED(backend->fn.vkCreateGraphicsPipelines(backend->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline))) {
        fn.vkDestroyShaderModule(backend->device, vs_module, nullptr);
        fn.vkDestroyShaderModule(backend->device, fs_module, nullptr);
        return VK_NULL_HANDLE;
    }

    fn.vkDestroyShaderModule(backend->device, vs_module, nullptr);
    fn.vkDestroyShaderModule(backend->device, fs_module, nullptr);
    return pipeline;
}

static void PushResourceDescriptors(IvgBackendVulkan* backend, VkCommandBuffer vk_cmd_buf, VkBuffer vtx_buffer, VkBuffer winding_buffer)
{
    IvgBackendVulkanFn& fn = backend->fn;
    IvgBackendVulkanDescriptorStream& ds = backend->descriptor_stream[backend->frame_id];

    VkDescriptorSetAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = {};
    alloc_info.descriptorPool = ds.pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &backend->descriptor_set_layout;

    VkDescriptorSet descriptor_set;
    IVG_VK_CHECK(fn.vkAllocateDescriptorSets(backend->device, &alloc_info, &descriptor_set));

    VkDescriptorBufferInfo descriptor[2];
    descriptor[0].buffer = vtx_buffer;
    descriptor[0].offset = 0;
    descriptor[0].range = VK_WHOLE_SIZE;
    descriptor[1].buffer = winding_buffer;
    descriptor[1].offset = 0;
    descriptor[1].range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write[2];
    // vtx_buffer descriptor
    write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[0].pNext = {};
    write[0].dstSet = descriptor_set;
    write[0].dstBinding = 0;
    write[0].dstArrayElement = 0;
    write[0].descriptorCount = 1;
    write[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write[0].pImageInfo = {};
    write[0].pBufferInfo = &descriptor[0];
    write[0].pTexelBufferView = {};
    // winding_buffer descriptor
    write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[1].pNext = {};
    write[1].dstSet = descriptor_set;
    write[1].dstBinding = 1;
    write[1].dstArrayElement = 0;
    write[1].descriptorCount = 1;
    write[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write[1].pImageInfo = {};
    write[1].pBufferInfo = &descriptor[1];
    write[1].pTexelBufferView = {};
    fn.vkUpdateDescriptorSets(backend->device, 2, write, 0, nullptr);
    fn.vkCmdBindDescriptorSets(vk_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    ds.current_descriptor_set = descriptor_set;
}

void IvgBackendVulkan_LoadFunctions(IvgBackendVulkan_LoaderFunc instance_loader_fn, IvgBackendVulkan_LoaderFunc device_loader_fn, void* userdata, IvgBackendVulkanFn* fn)
{
    fn->vkGetPhysicalDeviceImageFormatProperties = (PFN_vkGetPhysicalDeviceImageFormatProperties)instance_loader_fn("vkGetPhysicalDeviceImageFormatProperties", userdata);
    fn->vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)instance_loader_fn("vkGetPhysicalDeviceMemoryProperties", userdata);
    fn->vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)device_loader_fn("vkGetBufferMemoryRequirements", userdata);
    fn->vkAllocateMemory = (PFN_vkAllocateMemory)device_loader_fn("vkAllocateMemory", userdata);
    fn->vkFreeMemory = (PFN_vkFreeMemory)device_loader_fn("vkFreeMemory", userdata);
    fn->vkMapMemory = (PFN_vkMapMemory)device_loader_fn("vkMapMemory", userdata);
    fn->vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)device_loader_fn("vkFlushMappedMemoryRanges", userdata);
    fn->vkCreateBuffer = (PFN_vkCreateBuffer)device_loader_fn("vkCreateBuffer", userdata);
    fn->vkCreateImage = (PFN_vkCreateImage)device_loader_fn("vkCreateImage", userdata);
    fn->vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)device_loader_fn("vkCreateDescriptorSetLayout", userdata);
    fn->vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)device_loader_fn("vkCreateDescriptorPool", userdata);
    fn->vkCreateShaderModule = (PFN_vkCreateShaderModule)device_loader_fn("vkCreateShaderModule", userdata);
    fn->vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)device_loader_fn("vkCreatePipelineLayout", userdata);
    fn->vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)device_loader_fn("vkCreateGraphicsPipelines", userdata);
    fn->vkBindBufferMemory = (PFN_vkBindBufferMemory)device_loader_fn("vkBindBufferMemory", userdata);
    fn->vkBindImageMemory = (PFN_vkBindImageMemory)device_loader_fn("vkBindImageMemory", userdata);
    fn->vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)device_loader_fn("vkAllocateDescriptorSets", userdata);
    fn->vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)device_loader_fn("vkUpdateDescriptorSets", userdata);
    fn->vkResetDescriptorPool = (PFN_vkResetDescriptorPool)device_loader_fn("vkResetDescriptorPool", userdata);
    fn->vkDestroyBuffer = (PFN_vkDestroyBuffer)device_loader_fn("vkDestroyBuffer", userdata);
    fn->vkDestroyImage = (PFN_vkDestroyImage)device_loader_fn("vkDestroyImage", userdata);
    fn->vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)device_loader_fn("vkDestroyDescriptorSetLayout", userdata);
    fn->vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)device_loader_fn("vkDestroyDescriptorPool", userdata);
    fn->vkDestroyShaderModule = (PFN_vkDestroyShaderModule)device_loader_fn("vkDestroyShaderModule", userdata);
    fn->vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)device_loader_fn("vkDestroyPipelineLayout", userdata);
    fn->vkDestroyPipeline = (PFN_vkDestroyPipeline)device_loader_fn("vkDestroyPipeline", userdata);
    fn->vkCmdBindPipeline = (PFN_vkCmdBindPipeline)device_loader_fn("vkCmdBindPipeline", userdata);
    fn->vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)device_loader_fn("vkCmdBindDescriptorSets", userdata);
    fn->vkCmdPushConstants = (PFN_vkCmdPushConstants)device_loader_fn("vkCmdPushConstants", userdata);
    fn->vkCmdDraw = (PFN_vkCmdDraw)device_loader_fn("vkCmdDraw", userdata);
    fn->vkCmdSetScissor = (PFN_vkCmdSetScissor)device_loader_fn("vkCmdSetScissor", userdata);
    fn->vkCmdSetViewport = (PFN_vkCmdSetViewport)device_loader_fn("vkCmdSetViewport", userdata);
    fn->vkCmdFillBuffer = (PFN_vkCmdFillBuffer)device_loader_fn("vkCmdFillBuffer", userdata);
    fn->vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)device_loader_fn("vkCmdPipelineBarrier", userdata);
}

bool IvgBackendVulkan_Initialize(const IvgBackendVulkanInit* init, IvgBackendVulkan** backend)
{
    IVG_ASSERT(init->fn);

    void* backend_ptr = IVG_ALLOC(sizeof(IvgBackendVulkan));
    if (!backend_ptr)
        return false;

    IvgBackendVulkan* new_backend = new(backend_ptr) IvgBackendVulkan();
    new_backend->fn = *init->fn;
    new_backend->physical_device = init->physical_device;
    new_backend->device = init->device;
    new_backend->num_frames_in_flight = init->num_frames_in_flight;
    new_backend->min_allocation_size = init->min_allocation_size;
    new_backend->min_winding_buffer_size = init->min_winding_buffer_size + (init->min_winding_buffer_size % 4);

    VkDescriptorSetLayoutBinding shader_binding[2]{
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr }, // Vertex buffer
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }, // Winding/coverage buffer
    };

    VkDescriptorSetLayoutCreateInfo set_layout_info;
    set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = {};
    set_layout_info.flags = {};
    set_layout_info.bindingCount = 2;
    set_layout_info.pBindings = shader_binding;
    if (IVG_VK_FAILED(new_backend->fn.vkCreateDescriptorSetLayout(init->device, &set_layout_info, nullptr, &new_backend->descriptor_set_layout))) {
        IVG_FREE(new_backend);
        return false;
    }

    VkPushConstantRange push_constant;
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(IvgBackendVulkanDrawArgs);

    VkPipelineLayoutCreateInfo layout_info;
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pNext = {};
    layout_info.flags = {};
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &new_backend->descriptor_set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;
    if (IVG_VK_FAILED(new_backend->fn.vkCreatePipelineLayout(init->device, &layout_info, nullptr, &new_backend->pipeline_layout))) {
        new_backend->fn.vkDestroyPipelineLayout(init->device, new_backend->pipeline_layout, nullptr);
        IVG_FREE(new_backend);
        return false;
    }

    VkDescriptorPoolSize descriptor_pool_size;
    descriptor_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_pool_size.descriptorCount = 2048;

    VkDescriptorPoolCreateInfo descriptor_pool_info;
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.pNext = {};
    descriptor_pool_info.flags = {};
    descriptor_pool_info.maxSets = 1024;
    descriptor_pool_info.poolSizeCount = 1;
    descriptor_pool_info.pPoolSizes = &descriptor_pool_size;

    for (uint32_t i = 0; i < init->num_frames_in_flight; i++) {
        IvgBackendVulkanDescriptorStream& ds = new_backend->descriptor_stream[i];
        if (IVG_VK_FAILED(new_backend->fn.vkCreateDescriptorPool(init->device, &descriptor_pool_info, nullptr, &ds.pool))) {
            new_backend->fn.vkDestroyPipelineLayout(init->device, new_backend->pipeline_layout, nullptr);
            for (uint32_t j = 0; j < i; j++)
                new_backend->fn.vkDestroyDescriptorPool(init->device, new_backend->descriptor_stream[j].pool, nullptr);
            IVG_FREE(new_backend);
            return false;
        }
    }

    /*VkPipeline pipeline = IvgBackendVulkan_CreatePipeline(new_backend, __spirv_vulkan_polygon_vs, sizeof(__spirv_vulkan_polygon_vs),
                                                          __spirv_vulkan_polygon_fs, sizeof(__spirv_vulkan_polygon_fs), false, false,
                                                          new_backend->pipeline_layout, new_backend->compatible_render_pass);*/

    *backend = new_backend;
    return true;
}

void IvgBackendVulkan_Shutdown(IvgBackendVulkan* backend)
{
    DestroyResource(backend, backend->num_frames_in_flight, ~0ull);
    for (uint32_t i = 0; i < backend->num_frames_in_flight; i++) {
        IvgBackendVulkanBuffer& vtx_buffer = backend->vtx_buffer[i];
        IvgBackendVulkanBuffer& winding_buffer = backend->winding_buffer[i];
        backend->fn.vkDestroyBuffer(backend->device, vtx_buffer.buffer, nullptr);
        backend->fn.vkFreeMemory(backend->device, vtx_buffer.allocation, nullptr);
        backend->fn.vkDestroyBuffer(backend->device, winding_buffer.buffer, nullptr);
        backend->fn.vkFreeMemory(backend->device, winding_buffer.allocation, nullptr);
        backend->fn.vkDestroyDescriptorPool(backend->device, backend->descriptor_stream[i].pool, nullptr);
    }
    for (auto [_, pipelines] : backend->pipeline_cache) {
        backend->fn.vkDestroyPipeline(backend->device, pipelines.polygon, nullptr);
        backend->fn.vkDestroyPipeline(backend->device, pipelines.fill, nullptr);
    }
    if (backend->pipeline_layout) backend->fn.vkDestroyPipelineLayout(backend->device, backend->pipeline_layout, nullptr);
    if (backend->descriptor_set_layout) backend->fn.vkDestroyDescriptorSetLayout(backend->device, backend->descriptor_set_layout, nullptr);
    backend->~IvgBackendVulkan();
    IVG_FREE(backend);
}

void IvgBackendVulkan_BeginFrame(IvgBackendVulkan* backend)
{
    IvgBackendVulkanBuffer& buffer = backend->vtx_buffer[backend->frame_id];
    IvgBackendVulkanDescriptorStream& ds = backend->descriptor_stream[backend->frame_id];
    DestroyResource(backend, backend->num_frames_in_flight, backend->frame_count);
    buffer.offset = 0;
    backend->winding_offset = 0;
    backend->fn.vkResetDescriptorPool(backend->device, ds.pool, 0);
}

bool IvgBackendVulkan_SubmitCommand(IvgBackendVulkan* backend, VkCommandBuffer vk_cmd_buf, VkRenderPass render_pass, const VkExtent2D& fb_size, IvgContext* ctx)
{
    IvgBackendVulkanFn& fn = backend->fn;
    IvgBackendVulkanBuffer& vtx_buffer = backend->vtx_buffer[backend->frame_id];
    uint32_t required_vtx_count = ctx->vtx_offset_;
    uint32_t new_vtx_count = vtx_buffer.offset + required_vtx_count;
    bool buffer_resized = false;
    if (vtx_buffer.buffer == VK_NULL_HANDLE || new_vtx_count > vtx_buffer.count) {
        CreateOrResizeBuffer(backend, vtx_buffer, new_vtx_count * sizeof(IvgV2), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        IVG_VK_CHECK(fn.vkMapMemory(backend->device, vtx_buffer.allocation, 0, VK_WHOLE_SIZE, 0, &vtx_buffer.mapped_ptr));
        vtx_buffer.count = new_vtx_count;
        buffer_resized = true;
    }

    IvgV2* vtx_dst = (IvgV2*)vtx_buffer.mapped_ptr + vtx_buffer.offset;
    std::memcpy(vtx_dst, ctx->vtx_buf_, required_vtx_count * sizeof(IvgV2));

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.pNext = {};
    range.memory = vtx_buffer.allocation;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    fn.vkFlushMappedMemoryRanges(backend->device, 1, &range);

    VkViewport vp;
    vp.x = 0;
    vp.y = 0;
    vp.width = (float)fb_size.width;
    vp.height = (float)fb_size.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    fn.vkCmdSetViewport(vk_cmd_buf, 0, 1, &vp);

    IvgBackendVulkanBuffer& winding_buffer = backend->winding_buffer[backend->frame_id];
    IvgBackendVulkanDescriptorStream& ds = backend->descriptor_stream[backend->frame_id];

    if (vtx_buffer.buffer && winding_buffer.buffer)
        PushResourceDescriptors(backend, vk_cmd_buf, vtx_buffer.buffer, winding_buffer.buffer);

    uint32_t current_winding_offset = backend->winding_offset;
    IvgCmdBufPtr ctx_cmd_buf{ ctx->cmd_buf_ };
    IvgByte* ctx_cmd_buf_end = ctx->cmd_buf_ + ctx->cmd_offset_;
    uint32_t cmd_buf_size = ctx->cmd_offset_;
    uint32_t current_command_header;
    IvgBackendVulkanDrawArgs draw_args;
    draw_args.inv_viewport.x = 2.0f / vp.width;
    draw_args.inv_viewport.y = 2.0f / vp.height;

    // Find pipeline for this render pass in the cache
    auto pipeline = backend->pipeline_cache.find(render_pass);
    if (pipeline == backend->pipeline_cache.end()) {
        VkPipeline polygon = CreatePipeline(backend, __spirv_vulkan_polygon_vs_shader, __spirv_vulkan_polygon_vs_size,
                                            __spirv_vulkan_polygon_fs_shader, __spirv_vulkan_polygon_fs_size,
                                            false, false, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                            backend->pipeline_layout, render_pass);
        VkPipeline fill = CreatePipeline(backend, __spirv_vulkan_fill_vs_shader, __spirv_vulkan_fill_vs_size,
                                         __spirv_vulkan_fill_fs_shader, __spirv_vulkan_fill_fs_size,
                                         true, true, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                         backend->pipeline_layout, render_pass);
        pipeline = backend->pipeline_cache.emplace(render_pass, IvgBackendVulkanPipeline{ polygon, fill }).first;
    }

    auto [polygon, fill] = pipeline->second;
    while (ctx_cmd_buf.cmd_bytes != ctx_cmd_buf_end) {
        const IvgBackendCommand* command = ctx_cmd_buf.cmd_data;
        switch (command->header) {
            case IvgCommandHeader_SetDrawState:
            {
                draw_args.fill_mode = command->set_draw_state.fill_mode;
                draw_args.paint_type = command->set_draw_state.paint_type;
                draw_args.color = command->set_draw_state.color[0];
                ctx_cmd_buf.cmd_bytes += sizeof(IvgSetDrawStateCmd);
                break;
            }
            case IvgCommandHeader_SetClipRect:
            {
                VkRect2D rect;
                rect.offset = { command->set_clip_rect.x, command->set_clip_rect.y };
                rect.extent = { command->set_clip_rect.w, command->set_clip_rect.h };
                fn.vkCmdSetScissor(vk_cmd_buf, 0, 1, &rect);
                ctx_cmd_buf.cmd_bytes += sizeof(IvgSetClipRectCmd);
                break;
            }
            case IvgCommandHeader_Draw:
            {
                uint32_t tmp_winding_offset = current_winding_offset;
                IvgCmdBufPtr draw_cmd_ptr{ ctx_cmd_buf };
                bool draw_skipped = false;

                // 1. draw to pixel coverage
                fn.vkCmdBindPipeline(vk_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, polygon);
                while (ctx_cmd_buf.cmd_bytes != ctx_cmd_buf_end) {
                    const IvgBackendCommand* command = draw_cmd_ptr.cmd_data;
                    if (command->header != IvgCommandHeader_Draw)
                        break;

                    // Check buffer size first!
                    uint32_t width = command->draw.w;
                    uint32_t height = command->draw.h;
                    uint32_t size = width * height;
                    uint32_t next_offset = current_winding_offset + size;
                    if (winding_buffer.buffer != VK_NULL_HANDLE && next_offset > winding_buffer.count) {
                        draw_skipped = true;
                        break;
                    }
                    else if (size > winding_buffer.count) {
                        VkDeviceSize required_size = IvgMax(backend->min_winding_buffer_size, size * sizeof(int32_t));
                        CreateOrResizeBuffer(backend, winding_buffer, required_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                        winding_buffer.count = required_size / sizeof(int32_t);
                        buffer_resized = true;
                    }

                    if (buffer_resized) {
                        PushResourceDescriptors(backend, vk_cmd_buf, vtx_buffer.buffer, winding_buffer.buffer);
                        buffer_resized = false;
                    }

                    draw_args.min_bb = command->draw.rect.min;
                    draw_args.max_bb = command->draw.rect.max;
                    draw_args.vtx_offset = command->draw.vtx_offset;
                    draw_args.winding_stride = width;
                    draw_args.winding_offset = current_winding_offset;

                    fn.vkCmdPushConstants(vk_cmd_buf, backend->pipeline_layout,
                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          0, sizeof(IvgBackendVulkanDrawArgs), &draw_args);
                    fn.vkCmdDraw(vk_cmd_buf, command->draw.vtx_count * 6, 1, 0, 0);

                    current_winding_offset = next_offset;
                    draw_cmd_ptr.cmd_bytes += sizeof(IvgDrawCmd);
                }

                VkMemoryBarrier barrier;
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                fn.vkCmdPipelineBarrier(vk_cmd_buf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier, 0, nullptr, 0, nullptr);

                // 2. fill covered pixel
                draw_cmd_ptr = ctx_cmd_buf;
                current_winding_offset = tmp_winding_offset;
                fn.vkCmdBindPipeline(vk_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, fill);
                while (ctx_cmd_buf.cmd_bytes != ctx_cmd_buf_end) {
                    const IvgBackendCommand* command = draw_cmd_ptr.cmd_data;
                    if (command->header != IvgCommandHeader_Draw)
                        break;

                    uint32_t width = command->draw.w;
                    uint32_t height = command->draw.h;
                    uint32_t size = width * height;
                    uint32_t next_offset = current_winding_offset + size;
                    if (winding_buffer.buffer != VK_NULL_HANDLE && next_offset > winding_buffer.count) {
                        break;
                    }

                    draw_args.min_bb = command->draw.rect.min;
                    draw_args.max_bb = command->draw.rect.max;
                    draw_args.vtx_offset = command->draw.vtx_offset;
                    draw_args.winding_stride = width;
                    draw_args.winding_offset = current_winding_offset;

                    fn.vkCmdPushConstants(vk_cmd_buf, backend->pipeline_layout,
                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          0, sizeof(IvgBackendVulkanDrawArgs), &draw_args);
                    fn.vkCmdDraw(vk_cmd_buf, 6, 1, 0, 0);

                    current_winding_offset = next_offset;
                    draw_cmd_ptr.cmd_bytes += sizeof(IvgDrawCmd);
                }

                fn.vkCmdPipelineBarrier(vk_cmd_buf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier, 0, nullptr, 0, nullptr);

                if (draw_skipped)
                    current_winding_offset = 0;

                ctx_cmd_buf = draw_cmd_ptr;
                break;
            }
        }
    }

    vtx_buffer.offset = new_vtx_count;
    backend->winding_offset = current_winding_offset;
    return false;
}

void IvgBackendVulkan_EndFrame(IvgBackendVulkan* backend)
{
    backend->frame_id = (backend->frame_id + 1) % backend->num_frames_in_flight;
    backend->frame_count++;
}


