#ifndef __IMVG_VULKAN_H__
#define __IMVG_VULKAN_H__

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#ifdef WIN32
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#include "imvg.h"
#include <vulkan/vulkan.h>

typedef PFN_vkVoidFunction(*IvgBackendVulkan_LoaderFunc)(const char* name, void* userdata);

struct IvgBackendVulkan;

struct IvgBackendVulkanFn
{
    PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkResetDescriptorPool vkResetDescriptorPool;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCmdFillBuffer vkCmdFillBuffer;
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
};

struct IvgBackendVulkanInit
{
    const IvgBackendVulkanFn* fn;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t num_frames_in_flight;
    VkDeviceSize min_allocation_size;
    VkDeviceSize min_winding_buffer_size;
};

void IvgBackendVulkan_LoadFunctions(IvgBackendVulkan_LoaderFunc instance_loader_fn, IvgBackendVulkan_LoaderFunc device_loader_fn, void* userdata, IvgBackendVulkanFn* fn);
bool IvgBackendVulkan_Initialize(const IvgBackendVulkanInit* init, IvgBackendVulkan** backend);
void IvgBackendVulkan_Shutdown(IvgBackendVulkan* backend);
void IvgBackendVulkan_BeginFrame(IvgBackendVulkan* backend);
bool IvgBackendVulkan_SubmitCommand(IvgBackendVulkan* backend, VkCommandBuffer cmd_buf, VkRenderPass render_pass, const VkExtent2D& fb_size, IvgContext* ctx);
void IvgBackendVulkan_EndFrame(IvgBackendVulkan* backend);

template <typename InstanceLoaderFn, typename DeviceLoaderFn>
inline static void IvgBackendVulkan_LoadFunctions(InstanceLoaderFn&& instance_loader_fn, DeviceLoaderFn&& device_loader_fn, IvgBackendVulkanFn* fn)
{
    struct Userdata
    {
        InstanceLoaderFn& instance_loader_userdata;
        DeviceLoaderFn& device_loader_userdata;
    };

    Userdata userdata{
        instance_loader_fn,
        device_loader_fn,
    };

    auto instance_loader_wrapper = [](const char* name, void* userdata) -> PFN_vkVoidFunction {
        InstanceLoaderFn& fn = ((Userdata*)userdata)->instance_loader_userdata;
        return fn(name);
        };

    auto device_loader_wrapper = [](const char* name, void* userdata) -> PFN_vkVoidFunction {
        DeviceLoaderFn& fn = ((Userdata*)userdata)->device_loader_userdata;
        return fn(name);
        };

    IvgBackendVulkan_LoadFunctions(instance_loader_wrapper, device_loader_wrapper, &userdata, fn);
}

#endif // __IMVG_VULKAN_H__
