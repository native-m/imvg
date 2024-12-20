#include "imvg.h"
#include "imvg_vulkan.h"
#include <iostream>
#include <random>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#define IVG_VK_ERROR(x) ((x) < VK_SUCCESS)
#define MAX_FRAMES_IN_FLIGHT 3

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
static PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
static PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
static PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
static PFN_vkCreateDevice vkCreateDevice;
static PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
static PFN_vkCreateFence vkCreateFence;
static PFN_vkCreateSemaphore vkCreateSemaphore;
static PFN_vkCreateImageView vkCreateImageView;
static PFN_vkCreateRenderPass vkCreateRenderPass;
static PFN_vkCreateFramebuffer vkCreateFramebuffer;
static PFN_vkCreateCommandPool vkCreateCommandPool;
static PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
static PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
static PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
static PFN_vkQueuePresentKHR vkQueuePresentKHR;
static PFN_vkQueueSubmit vkQueueSubmit;
static PFN_vkResetCommandPool vkResetCommandPool;
static PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer vkEndCommandBuffer;
static PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
static PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
static PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
static PFN_vkWaitForFences vkWaitForFences;
static PFN_vkResetFences vkResetFences;
static PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
static PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
static PFN_vkDestroyDevice vkDestroyDevice;
static PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
static PFN_vkDestroySemaphore vkDestroySemaphore;
static PFN_vkDestroyFence vkDestroyFence;
static PFN_vkDestroyImageView vkDestroyImageView;
static PFN_vkDestroyRenderPass vkDestroyRenderPass;
static PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
static PFN_vkDestroyCommandPool vkDestroyCommandPool;
static PFN_vkDestroyInstance vkDestroyInstance;
static PFN_vkGetDeviceQueue vkGetDeviceQueue;
static IvgBackendVulkanFn bfn;

struct SwapchainImage
{
    VkImage image;
    VkImageView view;
    VkFramebuffer fb;
    VkExtent2D size;
};

struct FrameData
{
    VkFence fence;
    VkSemaphore render_semaphore;
    VkSemaphore acquire_semaphore;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buf;
};

static VkInstance instance;
static VkPhysicalDevice physical_device;
static VkSurfaceKHR surface;
static VkDevice device;
static uint32_t graphics_queue_index = UINT32_MAX;
static VkQueue queue;
static VkSwapchainKHR swapchain;
static VkFormat swapchain_format;
static uint32_t num_swapchain_images;
static SwapchainImage swapchain_images[MAX_FRAMES_IN_FLIGHT];
static FrameData frame_data[MAX_FRAMES_IN_FLIGHT];
static VkRenderPass render_pass;
static bool rebuild_swapchain;
static IvgBackendVulkan* backend;

static bool InitializeVulkanInstance();
static bool InitializeDevice();
static bool InitializeSurface(HWND hwnd);
static bool InitializeSwapchain(HWND hwnd, uint32_t image_count);
static bool InitializeRenderPass();
static bool InitializeResource();
static int DestroyAll(int ret);
static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

int main()
{
    if (!InitializeVulkanInstance())
        return -1;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"vg_window";
    wc.hIconSm = wc.hIcon;
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"vg", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!InitializeSurface(hwnd))
        return DestroyAll(-1);

    if (!InitializeDevice())
        return DestroyAll(-1);

    IvgBackendVulkan_LoadFunctions([](const char* name) -> PFN_vkVoidFunction { return vkGetInstanceProcAddr(instance, name); },
                                   [](const char* name) -> PFN_vkVoidFunction { return vkGetDeviceProcAddr(device, name); }, &bfn);

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    if (!InitializeRenderPass())
        return DestroyAll(-1);

    uint32_t image_count = 2;
    if (!InitializeSwapchain(hwnd, image_count))
        return DestroyAll(-1);

    if (!InitializeResource())
        return DestroyAll(-1);

    IvgBackendVulkanInit ivg_backend;
    ivg_backend.fn = &bfn;
    ivg_backend.physical_device = physical_device;
    ivg_backend.device = device;
    ivg_backend.num_frames_in_flight = num_swapchain_images;
    ivg_backend.min_allocation_size = 4096; // 4KiB is recommended
    ivg_backend.min_winding_buffer_size = 2048 * 1024 * 4; // 8MiB is recommended

    if (!IvgBackendVulkan_Initialize(&ivg_backend, &backend))
        return DestroyAll(-1);

    IvgContext ctx{};
    IvgPaint paint(0, 255, 255, 255);
    IvgPaint paint2(0, 0, 255, 255);
    IvgPaint paint3(0xFF277FFF);
    uint32_t image_index;
    uint32_t frame_id = 0;
    bool running = true;

    std::random_device rd;
    std::uniform_real_distribution<float> dist_x(50.0f, 1200.0f);
    std::uniform_real_distribution<float> dist_y(70.0f, 700.0f);
    IvgVector<IvgV2> points;
    points.resize(10);

    for (uint32_t i = 0; i < 10; i++) {
        points[i].x = dist_x(rd);
        points[i].y = dist_y(rd);
    }

    while (running) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }

        if (rebuild_swapchain) {
            vkDeviceWaitIdle(device);
            if (!InitializeSwapchain(hwnd, image_count))
                break;
            rebuild_swapchain = false;
        }

        FrameData& current_frame = frame_data[frame_id];
        vkWaitForFences(device, 1, &current_frame.fence, VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, current_frame.acquire_semaphore, VK_NULL_HANDLE, &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            rebuild_swapchain = true;
        }

        SwapchainImage& swapchain_image = swapchain_images[image_index];
        uint32_t width = swapchain_image.size.width;
        uint32_t height = swapchain_image.size.height;
        ctx.Begin();
        ctx.SetFramebufferSize(width, height);
        ctx.SetClipRect(IvgRect{ 0.0f, 0.0f, (float)width, (float)height });
        ctx.SetPaint(&paint);
        ctx.FillTriangle(IvgV2(10.0f, 20.0f) * 5.0f, IvgV2(90.0f, 20.0f) * 5.0f,
                         IvgV2(100.0f, 90.0f) * 5.0f);
        ctx.SetPaint(&paint2);
        ctx.FillTriangle(IvgV2(30.0f, 10.0f) * 5.0f, IvgV2(90.0f, 15.0f) * 5.0f,
                         IvgV2(50.0f, 80.0f) * 5.0f);
        ctx.SetPaint(&paint3);
        ctx.FillRect(IvgV2(500.5f, 40.5f), IvgV2(699.5f, 99.5f));
        ctx.FillPolygon(points.Data, points.Size);
        ctx.End();

        IvgBackendVulkan_BeginFrame(backend);

        VkCommandBuffer cmd_buf = current_frame.cmd_buf;
        VkCommandBufferBeginInfo cmd_begin{};
        cmd_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetFences(device, 1, &current_frame.fence);
        vkResetCommandPool(device, current_frame.cmd_pool, 0);
        vkBeginCommandBuffer(cmd_buf, &cmd_begin);

        VkClearValue clear;
        clear.color.float32[0] = 0.0f;
        clear.color.float32[1] = 0.0f;
        clear.color.float32[2] = 0.0f;
        clear.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo rp_begin{};
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass = render_pass;
        rp_begin.framebuffer = swapchain_image.fb;
        rp_begin.renderArea.extent = swapchain_image.size;
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = &clear;

        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = {};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = graphics_queue_index;
        barrier.dstQueueFamilyIndex = graphics_queue_index;
        barrier.image = swapchain_image.image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        
        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        IvgBackendVulkan_SubmitCommand(backend, cmd_buf, render_pass, { width, height }, &ctx);
        vkCmdEndRenderPass(cmd_buf);
        vkEndCommandBuffer(cmd_buf);

        VkPipelineStageFlags wait_dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &current_frame.acquire_semaphore;
        submit.pWaitDstStageMask = &wait_dst_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd_buf;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &current_frame.render_semaphore;
        vkQueueSubmit(queue, 1, &submit, current_frame.fence);

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &current_frame.render_semaphore;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &image_index;
        result = vkQueuePresentKHR(queue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            rebuild_swapchain = true;
            continue;
        }

        IvgBackendVulkan_EndFrame(backend);
        frame_id = (frame_id + 1) % num_swapchain_images;
    }

    DestroyAll(0);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool InitializeVulkanInstance()
{
    void* vulkan_dll = ImVG::Utils::DllLoad("vulkan-1.dll");
    if (vulkan_dll == nullptr)
        return false;

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)ImVG::Utils::DllFindFunction(vulkan_dll, "vkGetInstanceProcAddr");
    if (vkGetInstanceProcAddr == nullptr)
        return false;

    PFN_vkEnumerateInstanceVersion fn_vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
    PFN_vkCreateInstance fn_vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(nullptr, "vkCreateInstance");

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "ImVG Vulkan Demo";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName = "ImVG Vulkan Demo";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

    if (fn_vkEnumerateInstanceVersion)
        fn_vkEnumerateInstanceVersion(&app_info.apiVersion);
    else
        app_info.apiVersion = VK_API_VERSION_1_0;

    const char* validation_layer = "VK_LAYER_KHRONOS_validation";
    const char* extensions[] = {
        "VK_KHR_surface",
#ifdef VK_USE_PLATFORM_WIN32_KHR
        "VK_KHR_win32_surface",
#endif
    };

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = 1;
    instance_info.ppEnabledLayerNames = &validation_layer;
    instance_info.enabledExtensionCount = 2;
    instance_info.ppEnabledExtensionNames = extensions;

    VkResult result = fn_vkCreateInstance(&instance_info, nullptr, &instance);
    if (IVG_VK_ERROR(result))
        return false;

    vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures");
    vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
    vkCreateDevice = (PFN_vkCreateDevice)vkGetInstanceProcAddr(instance, "vkCreateDevice");
    vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(instance, "vkDestroyInstance");
    vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");

    // Select physical device
    IvgVector<VkPhysicalDevice> physical_devices;
    uint32_t num_physical_device;
    vkEnumeratePhysicalDevices(instance, &num_physical_device, nullptr);
    physical_devices.resize(num_physical_device);
    vkEnumeratePhysicalDevices(instance, &num_physical_device, physical_devices.Data);
    physical_device = physical_devices[1];
    return true;
}

bool InitializeSurface(HWND hwnd)
{
    VkWin32SurfaceCreateInfoKHR surface_info{};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hinstance = GetModuleHandle(nullptr);
    surface_info.hwnd = hwnd;

    if (IVG_VK_ERROR(vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface)))
        return DestroyAll(-1);

    uint32_t num_surface_formats;
    IvgVector<VkSurfaceFormatKHR> surface_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, nullptr);
    surface_formats.resize(num_surface_formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, surface_formats.Data);

    VkSurfaceFormatKHR surface_format{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    for (auto [format, colorspace] : surface_formats) {
        if ((format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM) && colorspace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = { format, colorspace };
            break;
        }
    }

    swapchain_format = surface_format.format;
    return true;
}

bool InitializeDevice()
{
    IvgVector<VkQueueFamilyProperties> queue_families;
    uint32_t num_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, nullptr);
    queue_families.resize(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, queue_families.Data);

    for (uint32_t i = 0; i < num_queue_families; i++) {
        VkBool32 surface_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &surface_supported);
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT && surface_supported) {
            graphics_queue_index = i;
            break;
        }
    }

    if (graphics_queue_index == UINT32_MAX)
        return false;

    static const char* swapchain_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    static const float queue_priorities = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue_info{};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = graphics_queue_index;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priorities;

    VkPhysicalDeviceFeatures supported_device_features;
    vkGetPhysicalDeviceFeatures(physical_device, &supported_device_features);
    if (!supported_device_features.fragmentStoresAndAtomics) {
        return false;
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.fullDrawIndexUint32 = supported_device_features.fullDrawIndexUint32;
    device_features.fragmentStoresAndAtomics = true;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &graphics_queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = &swapchain_ext;
    device_info.pEnabledFeatures = &device_features;

    VkResult result = vkCreateDevice(physical_device, &device_info, nullptr, &device);
    if (IVG_VK_ERROR(result))
        return false;

    vkGetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetDeviceProcAddr(device, "vkGetDeviceQueue");
    vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
    vkCreateFence = (PFN_vkCreateFence)vkGetDeviceProcAddr(device, "vkCreateFence");
    vkCreateSemaphore = (PFN_vkCreateSemaphore)vkGetDeviceProcAddr(device, "vkCreateSemaphore");
    vkCreateImageView = (PFN_vkCreateImageView)vkGetDeviceProcAddr(device, "vkCreateImageView");
    vkCreateRenderPass = (PFN_vkCreateRenderPass)vkGetDeviceProcAddr(device, "vkCreateRenderPass");
    vkCreateFramebuffer = (PFN_vkCreateFramebuffer)vkGetDeviceProcAddr(device, "vkCreateFramebuffer");
    vkCreateCommandPool = (PFN_vkCreateCommandPool)vkGetDeviceProcAddr(device, "vkCreateCommandPool");
    vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
    vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
    vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
    vkQueuePresentKHR = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
    vkQueueSubmit = (PFN_vkQueueSubmit)vkGetDeviceProcAddr(device, "vkQueueSubmit");
    vkResetCommandPool = (PFN_vkResetCommandPool)vkGetDeviceProcAddr(device, "vkResetCommandPool");
    vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
    vkEndCommandBuffer = (PFN_vkEndCommandBuffer)vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
    vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass");
    vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)vkGetDeviceProcAddr(device, "vkCmdEndRenderPass");
    vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier");
    vkWaitForFences = (PFN_vkWaitForFences)vkGetDeviceProcAddr(device, "vkWaitForFences");
    vkResetFences = (PFN_vkResetFences)vkGetDeviceProcAddr(device, "vkResetFences");
    vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
    vkDestroyDevice = (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
    vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR");
    vkDestroyFence = (PFN_vkDestroyFence)vkGetDeviceProcAddr(device, "vkDestroyFence");
    vkDestroySemaphore = (PFN_vkDestroySemaphore)vkGetDeviceProcAddr(device, "vkDestroySemaphore");
    vkDestroyImageView = (PFN_vkDestroyImageView)vkGetDeviceProcAddr(device, "vkDestroyImageView");
    vkDestroyRenderPass = (PFN_vkDestroyRenderPass)vkGetDeviceProcAddr(device, "vkDestroyRenderPass");
    vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)vkGetDeviceProcAddr(device, "vkDestroyFramebuffer");
    vkDestroyCommandPool = (PFN_vkDestroyCommandPool)vkGetDeviceProcAddr(device, "vkDestroyCommandPool");

    vkGetDeviceQueue(device, graphics_queue_index, 0, &queue);

    return true;
}

bool InitializeSwapchain(HWND hwnd, uint32_t image_count)
{
    if (swapchain) {
        for (uint32_t i = 0; i < num_swapchain_images; i++) {
            SwapchainImage& img = swapchain_images[i];
            vkDestroyFramebuffer(device, img.fb, nullptr);
            vkDestroyImageView(device, img.view, nullptr);
        }
    }

    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps);
    num_swapchain_images = IvgClamp(image_count, surface_caps.minImageCount, surface_caps.maxImageCount);

    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = num_swapchain_images;
    swapchain_info.imageFormat = swapchain_format;
    swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent = surface_caps.currentExtent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.queueFamilyIndexCount = 1;
    swapchain_info.pQueueFamilyIndices = &graphics_queue_index;
    swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = swapchain;

    if (IVG_VK_ERROR(vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain)))
        return false;

    IvgVector<VkImage> images;
    images.resize(num_swapchain_images);
    vkGetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, images.Data);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = swapchain_format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkFramebufferCreateInfo fb_info{};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = render_pass;
    fb_info.attachmentCount = 1;
    fb_info.width = surface_caps.currentExtent.width;
    fb_info.height = surface_caps.currentExtent.height;
    fb_info.layers = 1;

    for (uint32_t i = 0; i < num_swapchain_images; i++) {
        SwapchainImage& img = swapchain_images[i];
        view_info.image = images[i];
        img.image = images[i];
        vkCreateImageView(device, &view_info, nullptr, &img.view);
        fb_info.pAttachments = &img.view;
        vkCreateFramebuffer(device, &fb_info, nullptr, &img.fb);
        img.size = surface_caps.currentExtent;
    }

    if (swapchain_info.oldSwapchain)
        vkDestroySwapchainKHR(device, swapchain_info.oldSwapchain, nullptr);

    return true;
}

bool InitializeRenderPass()
{
    VkAttachmentDescription att_desc{};
    att_desc.format = swapchain_format;
    att_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference att_ref{};
    att_ref.attachment = 0;
    att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_desc{};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &att_ref;

    VkSubpassDependency subpass_dependency;
    subpass_dependency.srcSubpass = 0;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpass_dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    subpass_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &att_desc;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass_desc;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &subpass_dependency;
    vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);
    return true;
}

bool InitializeResource()
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < num_swapchain_images; i++) {
        vkCreateFence(device, &fence_info, nullptr, &frame_data[i].fence);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_data[i].render_semaphore);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_data[i].acquire_semaphore);

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = graphics_queue_index;
        vkCreateCommandPool(device, &pool_info, nullptr, &frame_data[i].cmd_pool);

        VkCommandBufferAllocateInfo cmd_buf_info{};
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buf_info.commandPool = frame_data[i].cmd_pool;
        cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buf_info.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &cmd_buf_info, &frame_data[i].cmd_buf);
    }

    return true;
}

int DestroyAll(int ret)
{
    if (device) vkDeviceWaitIdle(device);
    if (backend) IvgBackendVulkan_Shutdown(backend);

    for (uint32_t i = 0; i < num_swapchain_images; i++) {
        FrameData& data = frame_data[i];
        SwapchainImage& img = swapchain_images[i];
        if (data.fence) vkDestroyFence(device, data.fence, nullptr);
        if (data.acquire_semaphore) vkDestroySemaphore(device, data.acquire_semaphore, nullptr);
        if (data.render_semaphore) vkDestroySemaphore(device, data.render_semaphore, nullptr);
        if (data.cmd_pool) vkDestroyCommandPool(device, data.cmd_pool, nullptr);
        if (img.view) vkDestroyImageView(device, img.view, nullptr);
        if (img.fb) vkDestroyFramebuffer(device, img.fb, nullptr);
    }

    if (render_pass) vkDestroyRenderPass(device, render_pass, nullptr);
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    return ret;
}

static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_SIZE:
            return 0;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}
