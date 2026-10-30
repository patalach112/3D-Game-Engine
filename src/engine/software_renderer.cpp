// Vulkan backend adapted to this project's Scene/EditorCamera API.
// Historical filename retained; there is no software rasterization in this file.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <windows.h>
#endif
#include <vulkan/vulkan.h>
#include "engine/render/vulkan_renderer.hpp"
#include "scene_shaders.hpp"
#include "geometry_shaders.hpp"
#include "engine/objects.hpp"
#include "engine/ui.hpp"
#include "backends/imgui_impl_vulkan.h"
#ifdef _WIN32
#include "backends/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif

#include "engine/log.hpp"
#include "engine/render/internal_types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace engine {
using namespace engine::internal;

namespace {
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " (VkResult " + std::to_string(result) + ")");
}

template<class T> bool named(const std::vector<T>& entries, const char* name) {
    return std::any_of(entries.begin(), entries.end(), [name](const T& entry) {
        if constexpr (requires { entry.extensionName; })
            return std::strcmp(entry.extensionName, name) == 0;
        else
            return std::strcmp(entry.layerName, name) == 0;
    });
}
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void*) {
    try {
        log_write((severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?LogLevel::Error:LogLevel::Warn,
            "vulkan",message->pMessage);
    } catch (...) {} // Never unwind through the driver's C callback.
    return VK_FALSE;
}
} // namespace

struct VulkanRenderer::Impl {
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug{};
    VkPhysicalDevice physical{};
    VkPhysicalDeviceProperties device_properties{};
    FrameGeometryCache frame_cache;
    RenderStatistics stats;
    bool descriptors_dirty{true};
    bool multi_draw_indirect{},batching_requested{true};
    VkDeviceSize descriptor_object_bytes{},descriptor_indirect_bytes{},descriptor_tint_bytes{};
    VkDevice device{};
    VkQueue graphics_queue{}, present_queue{};
    std::uint32_t graphics_family{}, present_family{};
    VkSurfaceKHR surface{};
    VkSwapchainKHR swapchain{};
    VkFormat swap_format{};
    VkExtent2D swap_extent{};
    std::vector<VkImage> swap_images;
    std::vector<VkSemaphore> finished;
    VkSemaphore acquired{};
#ifdef _WIN32
    HINSTANCE hinstance{};
    HWND hwnd{};
#endif
    std::string name;
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence fence{};
    VkRenderPass render_pass{};
    VkPipelineLayout layout{};
    VkPipeline wire{}, overlay_wire{}, overlay_solid{};
    VkPipeline object_pipeline{},hud_pipeline{},cull_pipeline{};
    VkPipelineLayout object_layout{},hud_layout{},cull_layout{};
    VkDescriptorSetLayout geometry_set_layout{};
    VkDescriptorPool geometry_pool{};
    VkDescriptorSet geometry_set{};
    std::vector<DrawIndexedIndirectCommand> last_commands;
    VkFormat depth_format{};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    ImGuiContext* ui{};
    bool ui_platform{}, ui_frame_open{}, ui_frame_ready{};
    struct Image { VkImage handle{}; VkDeviceMemory memory{}; VkImageView view{}; } color, depth, msaa;
    struct Buffer { VkBuffer handle{}; VkDeviceMemory memory{}; VkDeviceSize capacity{}; void* mapped{}; } vertices, readback, geometry, object_data, indirect, object_tints, ui_uv;
    VkFramebuffer framebuffer{};
    int target_width{}, target_height{};
    std::vector<std::uint32_t> scratch_hud_indices;
    std::vector<Vertex_hud> scratch_hud;
    std::vector<Vec2> scratch_uv;
    bool rendered{};
    bool recording{}; // Scene pass and presentation share one submission on Windows.

    ~Impl() { cleanup(); }
    void init();
    void cleanup() noexcept;
    void destroy_target() noexcept;
    void destroy_swapchain() noexcept;
    void create_swapchain();
    std::uint32_t memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags);
    void create_image(Image& image, int width, int height, VkFormat format,
                      VkImageUsageFlags usage, VkImageAspectFlags aspect,
                      VkSampleCountFlagBits image_samples = VK_SAMPLE_COUNT_1_BIT);
    void destroy_buffer(Buffer& buffer) noexcept;
    void ensure_buffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    void create_target(int width, int height);
    VkPipeline pipeline(VkPrimitiveTopology topology, bool depth_test, bool depth_write,int kind=0);
    void init_geometry();
    void begin();
    void submit(VkSemaphore wait = VK_NULL_HANDLE, VkSemaphore signal = VK_NULL_HANDLE);
    void draw(const FrameGeometry& frame, const EditorCamera& camera, int width, int height,
              std::vector<std::uint32_t>& pixels, RenderViewport scene_view, bool draw_ui,
              Vec3 clear_color);
    void present();
};

void VulkanRenderer::Impl::init() {
    if (device) return;
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
    std::uint32_t count=0;
    check(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr),"Enumerate instance extensions");
    std::vector<VkExtensionProperties> available(count);
    check(vkEnumerateInstanceExtensionProperties(nullptr,&count,available.data()),"Enumerate instance extensions");
#ifdef _WIN32
    if (hwnd) {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }
#endif
#ifndef NDEBUG
    check(vkEnumerateInstanceLayerProperties(&count,nullptr),"Enumerate validation layers");
    std::vector<VkLayerProperties> available_layers(count);
    check(vkEnumerateInstanceLayerProperties(&count,available_layers.data()),"Enumerate validation layers");
    if (named(available_layers,"VK_LAYER_KHRONOS_validation")) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        if (named(available,VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif
    for (const char* extension : extensions)
        if (!named(available,extension)) throw std::runtime_error(std::string("Missing Vulkan extension: ")+extension);
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="Starter 3D Engine";
    app.pEngineName="Starter Vulkan Renderer";
    app.applicationVersion=VK_MAKE_VERSION(0,3,0);
    app.engineVersion=app.applicationVersion;
    app.apiVersion=VK_API_VERSION_1_1;
    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
    debug_info.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback=debug_callback;
    const bool debug_enabled=std::find(extensions.begin(),extensions.end(),
        std::string(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))!=extensions.end();
    VkInstanceCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo=&app;
    info.enabledExtensionCount=static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames=extensions.data();
    info.enabledLayerCount=static_cast<std::uint32_t>(layers.size());
    info.ppEnabledLayerNames=layers.data();
    info.pNext=debug_enabled?&debug_info:nullptr;
    check(vkCreateInstance(&info,nullptr,&instance),"Create Vulkan instance (requires Vulkan 1.1)");
    if (debug_enabled) {
        auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));
        if (create) check(create(instance,&debug_info,nullptr,&debug),"Create validation messenger");
    }
#ifdef _WIN32
    if (hwnd) {
        VkWin32SurfaceCreateInfoKHR surface_info{};
        surface_info.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surface_info.hinstance=hinstance; surface_info.hwnd=hwnd;
        check(vkCreateWin32SurfaceKHR(instance,&surface_info,nullptr,&surface),"Create Win32 Vulkan surface");
    }
#endif
    check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Enumerate Vulkan devices");
    std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Enumerate Vulkan devices");
    int best=-1;
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate,&properties);
        VkPhysicalDeviceFeatures features{}; vkGetPhysicalDeviceFeatures(candidate,&features);
        if (properties.apiVersion<VK_API_VERSION_1_1 || !features.drawIndirectFirstInstance) continue;
        bool vertex_formats=true;
        for (auto format:{VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_A2B10G10R10_UNORM_PACK32}) {
            VkFormatProperties support{}; vkGetPhysicalDeviceFormatProperties(candidate,format,&support);
            vertex_formats&=(support.bufferFeatures&VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)!=0;
        }
        if (!vertex_formats) continue;
        std::uint32_t families_count=0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families_count,nullptr);
        std::vector<VkQueueFamilyProperties> families(families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families_count,families.data());
        std::optional<std::uint32_t> graphics, present;
        for (std::uint32_t i=0;i<families_count;++i) {
            if (!families[i].queueCount) continue;
            if (!graphics && ((families[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))) graphics=i;
            if (surface) {
                VkBool32 supported=VK_FALSE;
                check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate,i,surface,&supported),"Query presentation support");
                if (!present && supported) present=i;
                // Prefer a single graphics/presentation family when possible.
                if (supported && ((families[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))) { graphics=i; present=i; break; }
            }
        }
        if (!graphics || (surface && !present)) continue;
        if (surface) {
            std::uint32_t extension_count=0;
            check(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&extension_count,nullptr),"Enumerate device extensions");
            std::vector<VkExtensionProperties> device_extensions(extension_count);
            check(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&extension_count,device_extensions.data()),"Enumerate device extensions");
            if (!named(device_extensions,VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
            std::uint32_t formats=0, modes=0;
            check(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate,surface,&formats,nullptr),"Query surface formats");
            check(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate,surface,&modes,nullptr),"Query presentation modes");
            if (!formats || !modes) continue;
        }
        VkFormatProperties color_properties{};
        vkGetPhysicalDeviceFormatProperties(candidate,VK_FORMAT_R8G8B8A8_UNORM,&color_properties);
        const VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
            |(surface?VK_FORMAT_FEATURE_BLIT_SRC_BIT:0);
        if ((color_properties.optimalTilingFeatures&required)!=required) continue;
        VkFormat chosen_depth=VK_FORMAT_UNDEFINED;
        for (VkFormat format : {VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D16_UNORM}) {
            VkFormatProperties fp{}; vkGetPhysicalDeviceFormatProperties(candidate,format,&fp);
            if (fp.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { chosen_depth=format; break; }
        }
        if (chosen_depth==VK_FORMAT_UNDEFINED) continue;
        const int score=properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?100:
            properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?50:10;
        if (score>best) {
            best=score; physical=candidate; device_properties=properties; name=properties.deviceName; depth_format=chosen_depth;
            graphics_family=*graphics; present_family=present.value_or(*graphics);
        }
    }
    if (!physical) throw std::runtime_error("No suitable Vulkan 1.1 graphics+compute device with drawIndirectFirstInstance. Install a Vulkan-capable graphics driver.");
    const float priority=1;
    std::vector<VkDeviceQueueCreateInfo> queues;
    for (auto family : {graphics_family,present_family}) {
        if (!queues.empty() && queues.front().queueFamilyIndex==family) continue;
        VkDeviceQueueCreateInfo queue{};
        queue.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex=family; queue.queueCount=1; queue.pQueuePriorities=&priority;
        queues.push_back(queue);
    }
    const char* swap_extension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo device_info{};
    device_info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount=static_cast<std::uint32_t>(queues.size());
    device_info.pQueueCreateInfos=queues.data();
    device_info.enabledExtensionCount=surface?1:0;
    device_info.ppEnabledExtensionNames=surface?&swap_extension:nullptr;
    VkPhysicalDeviceFeatures supported_features{};vkGetPhysicalDeviceFeatures(physical,&supported_features);
    VkPhysicalDeviceFeatures enabled{}; enabled.drawIndirectFirstInstance=VK_TRUE;
    enabled.multiDrawIndirect=supported_features.multiDrawIndirect;
    multi_draw_indirect=enabled.multiDrawIndirect==VK_TRUE;
    stats.indirect_batching=multi_draw_indirect && batching_requested;
    device_info.pEnabledFeatures=&enabled;
    check(vkCreateDevice(physical,&device_info,nullptr,&device),"Create logical Vulkan device");
    vkGetDeviceQueue(device,graphics_family,0,&graphics_queue);
    vkGetDeviceQueue(device,present_family,0,&present_queue);
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex=graphics_family;
    pool_info.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vkCreateCommandPool(device,&pool_info,nullptr,&pool),"Create command pool");
    VkCommandBufferAllocateInfo command_info{};
    command_info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_info.commandPool=pool; command_info.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; command_info.commandBufferCount=1;
    check(vkAllocateCommandBuffers(device,&command_info,&command),"Allocate command buffer");
    VkFenceCreateInfo fence_info{};
    fence_info.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fence_info.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    check(vkCreateFence(device,&fence_info,nullptr,&fence),"Create frame fence");
    if (surface) {
        VkSemaphoreCreateInfo semaphore_info{}; semaphore_info.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        check(vkCreateSemaphore(device,&semaphore_info,nullptr,&acquired),"Create acquire semaphore");
    }

    // Intersect device limits AND the actual color/depth format capabilities.
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical,&properties);
    VkImageFormatProperties color_caps{}, depth_caps{};
    check(vkGetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,0,&color_caps),"Query MSAA color support");
    check(vkGetPhysicalDeviceImageFormatProperties(physical,depth_format,
        VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,0,&depth_caps),"Query MSAA depth support");
    const auto supported=properties.limits.framebufferColorSampleCounts
        &properties.limits.framebufferDepthSampleCounts&color_caps.sampleCounts&depth_caps.sampleCounts;
    if (supported&VK_SAMPLE_COUNT_4_BIT) samples=VK_SAMPLE_COUNT_4_BIT;
    else if (supported&VK_SAMPLE_COUNT_2_BIT) samples=VK_SAMPLE_COUNT_2_BIT;
    const bool resolve=samples!=VK_SAMPLE_COUNT_1_BIT;
    std::array<VkAttachmentDescription,3> attachments{};
    attachments[0].format=VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples=samples;
    attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp=resolve?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout=resolve?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachments[1].format=depth_format;
    attachments[1].samples=samples;
    attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[2]=attachments[0];
    attachments[2].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference resolve_ref{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference color_ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pResolveAttachments=resolve?&resolve_ref:nullptr;
    subpass.colorAttachmentCount=1; subpass.pColorAttachments=&color_ref; subpass.pDepthStencilAttachment=&depth_ref;
    std::array<VkSubpassDependency,2> dependencies{};
    dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL; dependencies[0].dstSubpass=0;
    dependencies[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass=0; dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo pass_info{};
    pass_info.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount=resolve?3:2; pass_info.pAttachments=attachments.data();
    pass_info.subpassCount=1; pass_info.pSubpasses=&subpass;
    pass_info.dependencyCount=2; pass_info.pDependencies=dependencies.data();
    check(vkCreateRenderPass(device,&pass_info,nullptr,&render_pass),"Create render pass");
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(CameraPush)+sizeof(Vec4)};
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount=1; layout_info.pPushConstantRanges=&push;
    check(vkCreatePipelineLayout(device,&layout_info,nullptr,&layout),"Create pipeline layout");
    init_geometry();
    wire=pipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST,true,false);
    overlay_wire=pipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST,false,false);
    overlay_solid=pipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,false,false);
}

VkPipeline VulkanRenderer::Impl::pipeline(VkPrimitiveTopology topology, bool depth_test, bool depth_write,int kind) {
    struct Module {
        VkDevice device;
        VkShaderModule handle{};
        ~Module() { if (handle) vkDestroyShaderModule(device,handle,nullptr); }
    } vertex{device}, fragment{device};
    const auto create_module=[&](const auto& code, VkShaderModule& module) {
        VkShaderModuleCreateInfo info{};
        info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize=sizeof(code); info.pCode=code;
        check(vkCreateShaderModule(device,&info,nullptr,&module),"Create shader module");
    };
    if (kind==1) create_module(packed_shaders::object_vert,vertex.handle);
    else if (kind==2) create_module(packed_shaders::hud_vert,vertex.handle);
    else create_module(packed_shaders::debug_vert,vertex.handle);
    if (kind==2) create_module(packed_shaders::hud_frag,fragment.handle);
    else create_module(shaders::frag,fragment.handle);
    std::array<VkPipelineShaderStageCreateInfo,2> stages{};
    for (auto& stage : stages) { stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName="main"; }
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertex.handle;
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragment.handle;
    VkVertexInputBindingDescription bindings[]{{0,sizeof(engine::Vertex),VK_VERTEX_INPUT_RATE_VERTEX},
        {1,sizeof(DebugAttributes),VK_VERTEX_INPUT_RATE_VERTEX}};
    std::array<VkVertexInputAttributeDescription,3> attributes{{
        {0,0,VK_FORMAT_R16G16B16A16_SFLOAT,0},
        {1,1,VK_FORMAT_R32G32B32_SFLOAT,0},
        {3,1,VK_FORMAT_R32_SFLOAT,12}}};
    std::uint32_t attribute_count=3, binding_count=2;
    if (kind==1) {
        binding_count=1; attribute_count=2;
        attributes[1]={1,0,VK_FORMAT_A2B10G10R10_UNORM_PACK32,8};
    } else if (kind==2) {
        bindings[0].stride=sizeof(Vertex_hud); binding_count=1; attribute_count=2;
        attributes[0]={0,0,VK_FORMAT_R32G32_SFLOAT,0};
        attributes[1]={1,0,VK_FORMAT_R32G32B32A32_SFLOAT,8};
    }
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount=binding_count; vertex_input.pVertexBindingDescriptions=bindings;
    vertex_input.vertexAttributeDescriptionCount=attribute_count;
    vertex_input.pVertexAttributeDescriptions=attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology=topology;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewport.viewportCount=1; viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL; raster.lineWidth=1;
    // Meshes have mixed winding; two-sided rendering also works inside objects
    // and with negative scales, without requiring geometryShader/non-solid fill.
    raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable=topology==VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    raster.depthBiasConstantFactor=-1;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; multisample.rasterizationSamples=samples;
    VkPipelineDepthStencilStateCreateInfo depth_state{};
    depth_state.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_state.depthTestEnable=depth_test; depth_state.depthWriteEnable=depth_write; depth_state.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    if (kind==2) {
        blend_attachment.blendEnable=VK_TRUE;
        blend_attachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp=VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp=VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount=1; blend.pAttachments=&blend_attachment;
    const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
    VkGraphicsPipelineCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount=2; info.pStages=stages.data(); info.pVertexInputState=&vertex_input;
    info.pInputAssemblyState=&assembly; info.pViewportState=&viewport; info.pRasterizationState=&raster;
    info.pMultisampleState=&multisample; info.pDepthStencilState=&depth_state;
    info.pColorBlendState=&blend; info.pDynamicState=&dynamic; info.layout=kind==1?object_layout:kind==2?hud_layout:layout; info.renderPass=render_pass;
    VkPipeline result{};
    const VkResult status=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&result);
    if (status!=VK_SUCCESS) {
        if (result) vkDestroyPipeline(device,result,nullptr);
        check(status,"Create graphics pipeline");
    }
    return result;
}

void VulkanRenderer::Impl::init_geometry() {
    static_assert(sizeof(DrawIndexedIndirectCommand)==sizeof(VkDrawIndexedIndirectCommand));
    static_assert(offsetof(DrawIndexedIndirectCommand,firstInstance)==offsetof(VkDrawIndexedIndirectCommand,firstInstance));
    const VkDescriptorSetLayoutBinding bindings[]{
        {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo set_info{}; set_info.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_info.bindingCount=3; set_info.pBindings=bindings;
    check(vkCreateDescriptorSetLayout(device,&set_info,nullptr,&geometry_set_layout),"Create geometry descriptor layout");
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3};
    VkDescriptorPoolCreateInfo pool_info{}; pool_info.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets=1; pool_info.poolSizeCount=1; pool_info.pPoolSizes=&size;
    check(vkCreateDescriptorPool(device,&pool_info,nullptr,&geometry_pool),"Create geometry descriptor pool");
    VkDescriptorSetAllocateInfo allocation{}; allocation.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool=geometry_pool; allocation.descriptorSetCount=1; allocation.pSetLayouts=&geometry_set_layout;
    check(vkAllocateDescriptorSets(device,&allocation,&geometry_set),"Allocate geometry descriptors");
    const auto make_layout=[&](VkPipelineLayout& target,VkShaderStageFlags stage,std::uint32_t bytes,bool descriptors) {
        VkPushConstantRange range{stage,0,bytes};
        VkPipelineLayoutCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pushConstantRangeCount=1; info.pPushConstantRanges=&range;
        info.setLayoutCount=descriptors?1:0; info.pSetLayouts=descriptors?&geometry_set_layout:nullptr;
        check(vkCreatePipelineLayout(device,&info,nullptr,&target),"Create packed geometry pipeline layout");
    };
    make_layout(object_layout,VK_SHADER_STAGE_VERTEX_BIT,sizeof(CameraPush),true);
    make_layout(hud_layout,VK_SHADER_STAGE_VERTEX_BIT,sizeof(Vec4),false);
    make_layout(cull_layout,VK_SHADER_STAGE_COMPUTE_BIT,sizeof(CameraPush)+sizeof(std::uint32_t),true);
    VkShaderModule module{};
    VkShaderModuleCreateInfo shader{}; shader.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pCode=packed_shaders::cull_comp; shader.codeSize=sizeof(packed_shaders::cull_comp);
    check(vkCreateShaderModule(device,&shader,nullptr,&module),"Create visibility compute shader");
    VkComputePipelineCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; info.stage.module=module; info.stage.pName="main";
    info.layout=cull_layout;
    const auto result=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&cull_pipeline);
    vkDestroyShaderModule(device,module,nullptr); check(result,"Create visibility compute pipeline");
    object_pipeline=pipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,true,1);
    hud_pipeline=pipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,false,false,2);
}

std::uint32_t VulkanRenderer::Impl::memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical,&properties);
    for (std::uint32_t i=0;i<properties.memoryTypeCount;++i)
        if ((bits&(1U<<i)) && (properties.memoryTypes[i].propertyFlags&flags)==flags) return i;
    throw std::runtime_error("No compatible Vulkan memory type");
}
void VulkanRenderer::Impl::create_image(Image& image, int width, int height, VkFormat format,
                                        VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                        VkSampleCountFlagBits image_samples) {
    VkImageCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType=VK_IMAGE_TYPE_2D; info.extent={static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),1};
    info.mipLevels=1; info.arrayLayers=1; info.format=format; info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; info.usage=usage; info.samples=image_samples;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateImage(device,&info,nullptr,&image.handle),"Create render image");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device,image.handle,&requirements);
    VkMemoryAllocateInfo allocation{};
    allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device,&allocation,nullptr,&image.memory),"Allocate render image memory");
    check(vkBindImageMemory(device,image.handle,image.memory,0),"Bind render image memory");
    VkImageViewCreateInfo view{};
    view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; view.image=image.handle;
    view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=format;
    view.subresourceRange={aspect,0,1,0,1};
    check(vkCreateImageView(device,&view,nullptr,&image.view),"Create render image view");
}
void VulkanRenderer::Impl::destroy_buffer(Buffer& buffer) noexcept {
    if (buffer.mapped) vkUnmapMemory(device,buffer.memory);
    if (buffer.handle) vkDestroyBuffer(device,buffer.handle,nullptr);
    if (buffer.memory) vkFreeMemory(device,buffer.memory,nullptr);
    buffer={};
}
void VulkanRenderer::Impl::ensure_buffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (buffer.capacity>=size) return;
    check(vkDeviceWaitIdle(device),"Wait before growing buffer");
    const VkDeviceSize capacity=std::max({size,buffer.capacity*2,VkDeviceSize{4096}});
    destroy_buffer(buffer);
    if(&buffer==&object_data || &buffer==&indirect || &buffer==&object_tints)descriptors_dirty=true; // Recreated handles may reuse numeric values.
    VkBufferCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; info.size=capacity;
    info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device,&info,nullptr,&buffer.handle),"Create host-visible buffer");
    VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device,buffer.handle,&requirements);
    VkMemoryAllocateInfo allocation{};
    allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkAllocateMemory(device,&allocation,nullptr,&buffer.memory),"Allocate buffer memory");
    check(vkBindBufferMemory(device,buffer.handle,buffer.memory,0),"Bind buffer memory");
    check(vkMapMemory(device,buffer.memory,0,capacity,0,&buffer.mapped),"Map buffer memory");
    buffer.capacity=capacity;
}
void VulkanRenderer::Impl::destroy_target() noexcept {
    if (framebuffer) vkDestroyFramebuffer(device,framebuffer,nullptr);
    framebuffer=VK_NULL_HANDLE;
    for (Image* image : {&color,&depth,&msaa}) {
        if (image->view) vkDestroyImageView(device,image->view,nullptr);
        if (image->handle) vkDestroyImage(device,image->handle,nullptr);
        if (image->memory) vkFreeMemory(device,image->memory,nullptr);
        *image={};
    }
    target_width=target_height=0; rendered=false; recording=false;
}
void VulkanRenderer::Impl::create_target(int width, int height) {
    if (target_width==width && target_height==height) return;
    VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical,&properties);
    if (static_cast<std::uint32_t>(width)>properties.limits.maxImageDimension2D ||
        static_cast<std::uint32_t>(height)>properties.limits.maxImageDimension2D)
        throw std::invalid_argument("Viewport exceeds Vulkan maxImageDimension2D");
    check(vkDeviceWaitIdle(device),"Wait before resizing render target");
    destroy_target();
    create_image(color,width,height,VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    create_image(depth,width,height,depth_format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,samples);
    const bool resolve=samples!=VK_SAMPLE_COUNT_1_BIT;
    if (resolve) create_image(msaa,width,height,VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_IMAGE_ASPECT_COLOR_BIT,samples);
    const VkImageView attachments[]{resolve?msaa.view:color.view,depth.view,color.view};
    VkFramebufferCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass=render_pass; info.attachmentCount=resolve?3:2; info.pAttachments=attachments;
    info.width=static_cast<std::uint32_t>(width); info.height=static_cast<std::uint32_t>(height); info.layers=1;
    check(vkCreateFramebuffer(device,&info,nullptr,&framebuffer),"Create render framebuffer");
    target_width=width; target_height=height;
}
void VulkanRenderer::Impl::begin() {
    check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"Wait for frame fence");
    check(vkResetCommandBuffer(command,0),"Reset command buffer");
    VkCommandBufferBeginInfo info{}; info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command,&info),"Begin command buffer");
    recording=true;
}
void VulkanRenderer::Impl::submit(VkSemaphore wait, VkSemaphore signal) {
    check(vkEndCommandBuffer(command),"End command buffer");
    const VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo info{}; info.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount=1; info.pCommandBuffers=&command;
    info.waitSemaphoreCount=wait?1:0; info.pWaitSemaphores=wait?&wait:nullptr;
    info.pWaitDstStageMask=wait?&wait_stage:nullptr;
    info.signalSemaphoreCount=signal?1:0; info.pSignalSemaphores=signal?&signal:nullptr;
    // Reset only immediately before submission, never before acquire/out-of-date.
    check(vkResetFences(device,1,&fence),"Reset frame fence");
    check(vkQueueSubmit(graphics_queue,1,&info,fence),"Submit Vulkan frame");
    recording=false;
}
void VulkanRenderer::Impl::draw(const FrameGeometry& frame, const EditorCamera& camera,
                                int width, int height, std::vector<std::uint32_t>& pixels,
                                RenderViewport scene_view, bool draw_ui, Vec3 clear_color) {
    // Import once and pack once, rather than rebuilding the entire buffer after
    // debug geometry and then importing/rebuilding it a second time for ImGui.
    auto builder=GeometryBuilder::from_buffer(frame.geometry);
    const auto custom_hud_indices=frame.geometry.layout.hudIndexCount;
    const auto custom_hud_vertices=frame.geometry.layout.hudVertexCount;
    const auto debug_base=frame.geometry.layout.sceneVertexCount;
    const auto debug_first=frame.geometry.layout.sceneIndexCount;
    const auto& debug_attributes=frame.debug_attributes;
    const float debug_scale=frame.debug_scale;
    if(!frame.debug_vertices.empty())builder.add_mesh(frame.debug_vertices,frame.debug_indices);
    auto& uv=scratch_uv;uv.clear();
    if (draw_ui) {
        const auto* data=ImGui::GetDrawData();
        auto& hud=scratch_hud;hud.clear();
        auto& indices=scratch_hud_indices;indices.clear();
        hud.reserve(static_cast<std::size_t>(data->TotalVtxCount)); uv.reserve(hud.capacity());
        indices.reserve(static_cast<std::size_t>(data->TotalIdxCount));
        for (const auto* list:data->CmdLists) {
            for (const auto& v:list->VtxBuffer) {
                const auto tint=ImGui::ColorConvertU32ToFloat4(v.col);
                hud.push_back({{v.pos.x,v.pos.y},{tint.x,tint.y,tint.z,tint.w}}); uv.push_back({v.uv.x,v.uv.y});
            }
            for (auto index:list->IdxBuffer) indices.push_back(index);
        }
        builder.append_ui(hud,indices);
    }
    const auto packed=builder.build();
    const auto object_count=static_cast<std::uint32_t>(packed.objects.size());
    const auto& limits=device_properties;
    const auto object_bytes=packed.objects.size()*sizeof(ObjectData);
    const auto indirect_bytes=packed.commands.size()*sizeof(DrawIndexedIndirectCommand);
    const auto tint_bytes=packed.tints.size()*sizeof(Vec4);
    if (object_bytes>limits.limits.maxStorageBufferRange || indirect_bytes>limits.limits.maxStorageBufferRange || tint_bytes>limits.limits.maxStorageBufferRange
        || (static_cast<std::uint64_t>(object_count)+63)/64>limits.limits.maxComputeWorkGroupCount[0])
        throw std::length_error("Geometry exceeds Vulkan storage/dispatch limits");
    ensure_buffer(geometry,std::max<std::size_t>(packed.bytes.size(),1),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    ensure_buffer(object_data,std::max<std::size_t>(object_bytes,sizeof(ObjectData)),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    ensure_buffer(indirect,std::max<std::size_t>(indirect_bytes,sizeof(DrawIndexedIndirectCommand)),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    ensure_buffer(object_tints,std::max<std::size_t>(tint_bytes,sizeof(Vec4)),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    ensure_buffer(ui_uv,std::max<std::size_t>(uv.size()*sizeof(Vec2),1),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    create_target(width,height);
    const VkDeviceSize bytes=debug_attributes.size()*sizeof(DebugAttributes);
    ensure_buffer(vertices,std::max(bytes,VkDeviceSize{1}),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (!surface) ensure_buffer(readback,static_cast<VkDeviceSize>(width)*height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    begin(); // Wait once, BEFORE overwriting memory used by the previous submission.
    if (bytes) std::memcpy(vertices.mapped,debug_attributes.data(),static_cast<std::size_t>(bytes));
    if (!packed.bytes.empty()) std::memcpy(geometry.mapped,packed.bytes.data(),packed.bytes.size());
    if (object_bytes) std::memcpy(object_data.mapped,packed.objects.data(),object_bytes);
    if (indirect_bytes) std::memcpy(indirect.mapped,packed.commands.data(),indirect_bytes);
    if (tint_bytes) std::memcpy(object_tints.mapped,packed.tints.data(),tint_bytes);
    if (!uv.empty()) std::memcpy(ui_uv.mapped,uv.data(),uv.size()*sizeof(Vec2));
    const Basis basis=camera_basis(camera);
    const auto vec4=[](Vec3 p) { return std::array<float,4>{p.x,p.y,p.z,0}; };
    const float scale=1.0F/std::tan(camera.vertical_fov_degrees*kPi/360.0F);
    const CameraPush push{vec4(basis.right),vec4(basis.up),vec4(basis.forward),vec4(camera.position),
        {scale*static_cast<float>(scene_view.height)/static_cast<float>(scene_view.width),scale,kNearPlane,100000.0F}};
    if (object_count) {
        VkDescriptorBufferInfo descriptors[]{{object_data.handle,0,object_bytes},{indirect.handle,0,indirect_bytes},{object_tints.handle,0,tint_bytes}};
        VkWriteDescriptorSet writes[3]{};
        for (std::uint32_t i=0;i<3;++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet=geometry_set;
            writes[i].dstBinding=i; writes[i].descriptorCount=1; writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo=&descriptors[i];
        }
        if(descriptors_dirty || descriptor_object_bytes!=object_bytes || descriptor_indirect_bytes!=indirect_bytes || descriptor_tint_bytes!=tint_bytes) {
            vkUpdateDescriptorSets(device,3,writes,0,nullptr);
            descriptors_dirty=false;descriptor_object_bytes=object_bytes;descriptor_indirect_bytes=indirect_bytes;descriptor_tint_bytes=tint_bytes;++stats.descriptor_updates;
        }
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,cull_pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,cull_layout,0,1,&geometry_set,0,nullptr);
        vkCmdPushConstants(command,cull_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
        vkCmdPushConstants(command,cull_layout,VK_SHADER_STAGE_COMPUTE_BIT,sizeof(push),sizeof(object_count),&object_count);
        vkCmdDispatch(command,(object_count+63)/64,1,1);
        VkBufferMemoryBarrier visibility{}; visibility.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        visibility.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; visibility.dstAccessMask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        visibility.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; visibility.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        visibility.buffer=indirect.handle; visibility.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,0,0,nullptr,1,&visibility,0,nullptr);
    }
    std::array<VkClearValue,2> clears{};
    clears[0].color={{clear_color.x,clear_color.y,clear_color.z,1}};
    clears[1].depthStencil={1,0};
    VkRenderPassBeginInfo pass{}; pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass=render_pass; pass.framebuffer=framebuffer;
    pass.renderArea.extent={static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height)};
    pass.clearValueCount=2; pass.pClearValues=clears.data();
    vkCmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{static_cast<float>(scene_view.x),static_cast<float>(scene_view.y),
        static_cast<float>(scene_view.width),static_cast<float>(scene_view.height),0,1};
    VkRect2D scissor{{scene_view.x,scene_view.y},
        {static_cast<std::uint32_t>(scene_view.width),static_cast<std::uint32_t>(scene_view.height)}};
    vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
    if (object_count) {
        const VkDeviceSize packed_offset=packed.layout.sceneVertices;
        vkCmdBindVertexBuffers(command,0,1,&geometry.handle,&packed_offset);
        vkCmdBindIndexBuffer(command,geometry.handle,packed.layout.sceneIndices,VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,object_pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,object_layout,0,1,&geometry_set,0,nullptr);
        vkCmdPushConstants(command,object_layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push),&push);
        // Tint is indexed by firstInstance in an SSBO, not changed by a CPU
        // push-constant command for every object. Preserve the single-draw
        // fallback on devices without the optional multiDrawIndirect feature.
        const auto batch_limit=multi_draw_indirect && batching_requested?
            std::max(1U,device_properties.limits.maxDrawIndirectCount):1U;
        for(std::uint32_t first=0;first<object_count;) {
            const auto count=std::min(batch_limit,object_count-first);
            vkCmdDrawIndexedIndirect(command,indirect.handle,static_cast<VkDeviceSize>(first)*sizeof(DrawIndexedIndirectCommand),count,sizeof(DrawIndexedIndirectCommand));
            first+=count;++stats.indirect_draw_calls;
        }
        stats.indirect_objects+=object_count;
    }
    const VkBuffer debug_buffers[]{geometry.handle,vertices.handle};
    const VkDeviceSize debug_offsets[]{packed.layout.sceneVertices+debug_base*sizeof(engine::Vertex),0};
    vkCmdBindVertexBuffers(command,0,2,debug_buffers,debug_offsets);
    vkCmdBindIndexBuffer(command,geometry.handle,packed.layout.sceneIndices,VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push),&push);
    const Vec4 debug_transform{debug_scale,0,0,0};
    vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,sizeof(push),sizeof(debug_transform),&debug_transform);
    std::uint32_t first=debug_first;
    const auto batch=[&](VkPipeline pipeline, std::uint32_t count) {
        if (count) { vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline); vkCmdDrawIndexed(command,count,1,first,0,0); }
        first+=count;
    };
    batch(wire,frame.lines);
    batch(overlay_wire,frame.overlay_lines); batch(overlay_solid,frame.overlay_triangles);
    // UI/text shares the multisampled scene attachment, then one GPU resolve.
    if (custom_hud_indices) {
        VkViewport full{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
        VkRect2D clip{{0,0},{static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height)}};
        vkCmdSetViewport(command,0,1,&full); vkCmdSetScissor(command,0,1,&clip);
        const VkDeviceSize hud_offset=packed.layout.hudVertices;
        vkCmdBindVertexBuffers(command,0,1,&geometry.handle,&hud_offset);
        vkCmdBindIndexBuffer(command,geometry.handle,packed.layout.hudIndices,VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,hud_pipeline);
        const Vec4 screen{static_cast<float>(width),static_cast<float>(height),0,0};
        vkCmdPushConstants(command,hud_layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(screen),&screen);
        vkCmdDrawIndexed(command,custom_hud_indices,1,0,0,0);
    }
    if (draw_ui) {
        ImGui_ImplVulkan_SetSharedGeometry(geometry.handle,packed.layout.hudVertices+custom_hud_vertices*sizeof(Vertex_hud),
            packed.layout.hudIndices+custom_hud_indices*sizeof(std::uint32_t),ui_uv.handle);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),command);
    }
    vkCmdEndRenderPass(command);
    if (!surface) {
        if (object_count) {
            VkBufferMemoryBarrier host{}; host.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            host.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            host.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            host.buffer=indirect.handle; host.size=VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.imageExtent={static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),1};
        vkCmdCopyImageToBuffer(command,color.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);
        VkBufferMemoryBarrier barrier{};
        barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer=readback.handle; barrier.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    }
    rendered=true;
    // With a window, keep recording: present() appends the swapchain blit and
    // submits scene + presentation together. No intermediate CPU/GPU round trip.
    if (surface) { last_commands.clear(); return; }
    submit();
    check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"Wait for image readback");
    if (!surface) {
        last_commands.resize(object_count);
        if (object_count) std::memcpy(last_commands.data(),indirect.mapped,indirect_bytes);
        pixels.resize(static_cast<std::size_t>(width)*height);
        const auto* rgba=static_cast<const unsigned char*>(readback.mapped);
        for (std::size_t i=0;i<pixels.size();++i)
            pixels[i]=(static_cast<std::uint32_t>(rgba[i*4])<<16U)
                |(static_cast<std::uint32_t>(rgba[i*4+1])<<8U)|rgba[i*4+2];
    }
}

void VulkanRenderer::Impl::destroy_swapchain() noexcept {
    for (auto semaphore : finished) if (semaphore) vkDestroySemaphore(device,semaphore,nullptr);
    finished.clear(); swap_images.clear();
    if (swapchain) vkDestroySwapchainKHR(device,swapchain,nullptr);
    swapchain=VK_NULL_HANDLE; swap_extent={};
}
void VulkanRenderer::Impl::create_swapchain() {
    if (!surface) return;
    check(vkDeviceWaitIdle(device),"Wait before recreating swapchain");
    destroy_swapchain();
    VkSurfaceCapabilitiesKHR capabilities{};
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&capabilities),"Query surface capabilities");
    if (!(capabilities.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        throw std::runtime_error("Vulkan surface does not support transfer-destination swapchain images");
    VkExtent2D extent=capabilities.currentExtent;
    if (extent.width==UINT32_MAX) {
#ifdef _WIN32
        RECT rect{}; GetClientRect(hwnd,&rect);
        if (rect.right<=0 || rect.bottom<=0) return;
        extent.width=std::clamp(static_cast<std::uint32_t>(rect.right),capabilities.minImageExtent.width,capabilities.maxImageExtent.width);
        extent.height=std::clamp(static_cast<std::uint32_t>(rect.bottom),capabilities.minImageExtent.height,capabilities.maxImageExtent.height);
#else
        extent={static_cast<std::uint32_t>(target_width),static_cast<std::uint32_t>(target_height)};
#endif
    }
    if (!extent.width || !extent.height) return;
    std::uint32_t count=0;
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,nullptr),"Query surface formats");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats.data()),"Query surface formats");
    if (formats.size()==1 && formats[0].format==VK_FORMAT_UNDEFINED)
        formats[0].format=VK_FORMAT_B8G8R8A8_UNORM;
    std::optional<VkSurfaceFormatKHR> chosen;
    for (const auto& format : formats) {
        VkFormatProperties properties{}; vkGetPhysicalDeviceFormatProperties(physical,format.format,&properties);
        if (!(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT)) continue;
        if (!chosen) chosen=format;
        if ((format.format==VK_FORMAT_B8G8R8A8_UNORM || format.format==VK_FORMAT_R8G8B8A8_UNORM)
            && format.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen=format; break; }
    }
    if (!chosen) throw std::runtime_error("No Vulkan surface format supports image blits");
    std::uint32_t image_count=capabilities.minImageCount+1;
    if (capabilities.maxImageCount) image_count=std::min(image_count,capabilities.maxImageCount);
    VkCompositeAlphaFlagBitsKHR alpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (auto candidate : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
        if (capabilities.supportedCompositeAlpha&candidate) { alpha=candidate; break; }
    const std::uint32_t families[]{graphics_family,present_family};
    VkSwapchainCreateInfoKHR info{};
    info.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface=surface; info.minImageCount=image_count;
    info.imageFormat=chosen->format; info.imageColorSpace=chosen->colorSpace;
    info.imageExtent=extent; info.imageArrayLayers=1; info.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode=graphics_family==present_family?VK_SHARING_MODE_EXCLUSIVE:VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount=graphics_family==present_family?0:2;
    info.pQueueFamilyIndices=graphics_family==present_family?nullptr:families;
    info.preTransform=capabilities.currentTransform; info.compositeAlpha=alpha;
    info.presentMode=VK_PRESENT_MODE_FIFO_KHR; // Guaranteed, tear-free fallback.
    std::uint32_t mode_count=0;
    check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&mode_count,nullptr),"Query presentation modes");
    std::vector<VkPresentModeKHR> modes(mode_count);
    check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&mode_count,modes.data()),"Query presentation modes");
    if (std::find(modes.begin(),modes.end(),VK_PRESENT_MODE_MAILBOX_KHR)!=modes.end())
        info.presentMode=VK_PRESENT_MODE_MAILBOX_KHR;
    // No software limiter / vsync request: IMMEDIATE first (tearing possible).
    // Some surfaces/drivers expose only MAILBOX/FIFO; respect their capabilities.
    if (std::find(modes.begin(),modes.end(),VK_PRESENT_MODE_IMMEDIATE_KHR)!=modes.end())
        info.presentMode=VK_PRESENT_MODE_IMMEDIATE_KHR;
    info.clipped=VK_TRUE;
    check(vkCreateSwapchainKHR(device,&info,nullptr,&swapchain),"Create Vulkan swapchain");
    swap_extent=extent; swap_format=chosen->format;
    check(vkGetSwapchainImagesKHR(device,swapchain,&count,nullptr),"Get swapchain images");
    swap_images.resize(count);
    check(vkGetSwapchainImagesKHR(device,swapchain,&count,swap_images.data()),"Get swapchain images");
    finished.resize(count,VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semaphore{}; semaphore.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    // Presentation semaphores belong to acquired images, not frame indices.
    for (auto& handle : finished) check(vkCreateSemaphore(device,&semaphore,nullptr,&handle),"Create presentation semaphore");
}
void VulkanRenderer::Impl::present() {
    if (!surface || !rendered) return;
#ifdef _WIN32
    RECT rect{}; GetClientRect(hwnd,&rect);
    if (rect.right<=0 || rect.bottom<=0 || IsIconic(GetAncestor(hwnd,GA_ROOT))) return;
    if (swap_extent.width!=static_cast<std::uint32_t>(rect.right) || swap_extent.height!=static_cast<std::uint32_t>(rect.bottom))
        create_swapchain();
#endif
    if (!swapchain) create_swapchain();
    if (!swapchain) return;
    // A pending scene pass already waited for the previous frame in begin().
    // Cached-image presentation starts its own command buffer here. This fence
    // also guarantees the previous wait on the acquire semaphore has completed.
    if (!recording) begin();
    std::uint32_t index=0;
    VkResult acquire=VK_ERROR_OUT_OF_DATE_KHR;
    for (int attempt=0;attempt<3;++attempt) {
        acquire=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,acquired,VK_NULL_HANDLE,&index);
        if (acquire!=VK_ERROR_OUT_OF_DATE_KHR) break;
        create_swapchain();
        if (!swapchain) return; // Surface became minimized during acquire.
    }
    if (acquire==VK_ERROR_OUT_OF_DATE_KHR) {
#ifdef _WIN32
        // Keep a paint pending if the native window is still being resized.
        InvalidateRect(hwnd,nullptr,FALSE);
#endif
        return;
    }
    if (acquire!=VK_SUBOPTIMAL_KHR) check(acquire,"Acquire swapchain image");
    VkImageMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=swap_images[index]; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkImageBlit blit{};
    blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; blit.dstSubresource=blit.srcSubresource;
    blit.srcOffsets[1]={target_width,target_height,1};
    blit.dstOffsets[1]={static_cast<std::int32_t>(swap_extent.width),static_cast<std::int32_t>(swap_extent.height),1};
    vkCmdBlitImage(command,color.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swap_images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=0;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    submit(acquired,finished[index]);
    VkPresentInfoKHR present{}; present.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount=1; present.pWaitSemaphores=&finished[index];
    present.swapchainCount=1; present.pSwapchains=&swapchain; present.pImageIndices=&index;
    const VkResult result=vkQueuePresentKHR(present_queue,&present);
    if (result!=VK_ERROR_OUT_OF_DATE_KHR && result!=VK_SUBOPTIMAL_KHR) check(result,"Present Vulkan image");
    // Do not stall the UI thread with vkQueueWaitIdle here. The next begin()
    // fences command/vertex/target reuse; presentation semaphores are per image
    // and are reused only after that image is acquired again.
    if (result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR || acquire==VK_SUBOPTIMAL_KHR) {
        create_swapchain();
#ifdef _WIN32
        InvalidateRect(hwnd,nullptr,FALSE); // Present the replacement swapchain on the next paint.
#endif
    }
}
void VulkanRenderer::Impl::cleanup() noexcept {
    if (device) {
        vkDeviceWaitIdle(device);
        if (ui) {
            ImGui::SetCurrentContext(ui);
            // Init may have thrown after registering partially created objects.
            if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplVulkan_Shutdown();
#ifdef _WIN32
            if (ui_platform) ImGui_ImplWin32_Shutdown();
#endif
            ImGui::DestroyContext(ui);
            ui=nullptr;
        }
        destroy_swapchain(); destroy_target(); destroy_buffer(vertices); destroy_buffer(readback);
        for (auto* b:{&geometry,&object_data,&indirect,&object_tints,&ui_uv}) destroy_buffer(*b);
        for (auto p:{object_pipeline,hud_pipeline,cull_pipeline}) if (p) vkDestroyPipeline(device,p,nullptr);
        for (auto p:{object_layout,hud_layout,cull_layout}) if (p) vkDestroyPipelineLayout(device,p,nullptr);
        if (geometry_pool) vkDestroyDescriptorPool(device,geometry_pool,nullptr);
        if (geometry_set_layout) vkDestroyDescriptorSetLayout(device,geometry_set_layout,nullptr);
        for (auto pipeline : {wire,overlay_wire,overlay_solid}) if (pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if (layout) vkDestroyPipelineLayout(device,layout,nullptr);
        if (render_pass) vkDestroyRenderPass(device,render_pass,nullptr);
        if (acquired) vkDestroySemaphore(device,acquired,nullptr);
        if (fence) vkDestroyFence(device,fence,nullptr);
        if (pool) vkDestroyCommandPool(device,pool,nullptr);
        vkDestroyDevice(device,nullptr);
    }
    if (surface) vkDestroySurfaceKHR(instance,surface,nullptr);
    if (debug) {
        auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(instance,debug,nullptr);
    }
    if (instance) vkDestroyInstance(instance,nullptr);
}

VulkanRenderer::VulkanRenderer() : impl_(std::make_unique<Impl>()) {}
VulkanRenderer::~VulkanRenderer() = default;
void VulkanRenderer::set_indirect_batching(bool enabled) {
    if(!impl_)impl_=std::make_unique<Impl>();
    impl_->batching_requested=enabled;
    impl_->stats.indirect_batching=enabled && impl_->multi_draw_indirect;
}
void VulkanRenderer::initialize() {
    if (!impl_) impl_=std::make_unique<Impl>();
    if (impl_->device) return;
    try {
        LOG_DEBUG("renderer", "Initializing Vulkan instance");
        impl_->init();
        LOG_DEBUG("renderer", "Vulkan initialized on device: %s", impl_->name.c_str());
    }
    catch (...) { shutdown(); throw; }
}
void VulkanRenderer::shutdown() noexcept {
    LOG_DEBUG("renderer", "Shutting down Vulkan renderer");
    impl_.reset();
    pixels_.clear();
}
#ifdef _WIN32
void VulkanRenderer::set_window(void* hinstance, void* hwnd) {
    if (!impl_) impl_=std::make_unique<Impl>();
    if (impl_->instance) throw std::logic_error("set_window must precede Vulkan initialization");
    if (!hinstance || !hwnd) throw std::invalid_argument("Invalid Win32 Vulkan presentation target");
    impl_->hinstance=static_cast<HINSTANCE>(hinstance); impl_->hwnd=static_cast<HWND>(hwnd);
}
#endif
void VulkanRenderer::enable_ui(float font_size) {
    if (!std::isfinite(font_size) || font_size<8 || font_size>64)
        throw std::invalid_argument("UI font size must be in [8,64]");
    initialize();
    if (impl_->ui) { ImGui::SetCurrentContext(impl_->ui); return; }
    try {
        IMGUI_CHECKVERSION();
        impl_->ui=ImGui::CreateContext();
        auto& io=ImGui::GetIO();
        io.IniFilename=nullptr; // The editor owns layout persistence, no implicit CWD file.
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font{};
        font.OversampleH=3; font.OversampleV=2;
        font.SizePixels=font_size;
#ifdef _WIN32
        // Read installed fonts, never redistribute Microsoft's font files.
        wchar_t directory[MAX_PATH]{};
        if (GetWindowsDirectoryW(directory,MAX_PATH)>0) {
            const std::wstring path=std::wstring(directory)+L"\\Fonts\\segoeui.ttf";
            const int bytes=WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,nullptr,0,nullptr,nullptr);
            std::string utf8(static_cast<std::size_t>(bytes>0?bytes:1),'\0');
            if (bytes>0) WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,utf8.data(),bytes,nullptr,nullptr);
            static const ImWchar ranges[]{0x20,0x024F,0x2000,0x206F,0x2190,0x21FF,0};
            if (GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)
                io.Fonts->AddFontFromFileTTF(utf8.c_str(),font_size,&font,ranges);
        }
#endif
        if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault(&font);
        auto& style=ImGui::GetStyle();
        ImGui::StyleColorsDark();
        style.AntiAliasedLines=true; style.AntiAliasedLinesUseTex=true;
        style.AntiAliasedFill=true;
        style.WindowRounding=0; style.ChildRounding=6; style.FrameRounding=5;
        style.GrabRounding=4; style.ScrollbarRounding=5;
        style.WindowPadding={14,12}; style.FramePadding={9,7}; style.ItemSpacing={8,8};
        style.Colors[ImGuiCol_WindowBg]={0.065F,0.075F,0.095F,1};
        style.Colors[ImGuiCol_ChildBg]={0.08F,0.09F,0.115F,1};
        style.Colors[ImGuiCol_FrameBg]={0.115F,0.13F,0.16F,1};
        style.Colors[ImGuiCol_Button]={0.15F,0.18F,0.23F,1};
        style.Colors[ImGuiCol_ButtonHovered]={0.21F,0.31F,0.40F,1};
        style.Colors[ImGuiCol_ButtonActive]={0.12F,0.40F,0.49F,1};
        style.Colors[ImGuiCol_Header]={0.12F,0.30F,0.36F,1};
        style.Colors[ImGuiCol_CheckMark]={0.31F,0.85F,0.79F,1};
        style.Colors[ImGuiCol_SliderGrab]={0.31F,0.85F,0.79F,1};
        style.Colors[ImGuiCol_TextSelectedBg]={0.15F,0.50F,0.58F,0.6F};
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion=VK_API_VERSION_1_1; info.Instance=impl_->instance;
        info.PhysicalDevice=impl_->physical; info.Device=impl_->device;
        info.QueueFamily=impl_->graphics_family; info.Queue=impl_->graphics_queue;
        info.RenderPass=impl_->render_pass; info.MinImageCount=2; info.ImageCount=2;
        info.SharedGeometry=true; info.MSAASamples=impl_->samples; info.DescriptorPoolSize=32;
        info.CheckVkResultFn=[](VkResult result) { check(result,"Vulkan UI backend"); };
        if (!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("Initialize Vulkan UI backend");
#ifdef _WIN32
        if (impl_->hwnd) {
            if (!ImGui_ImplWin32_Init(impl_->hwnd)) throw std::runtime_error("Initialize Win32 UI input");
            impl_->ui_platform=true;
        }
#endif
    } catch (...) { shutdown(); throw; }
}
void VulkanRenderer::begin_ui_frame(float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds<=0)
        throw std::invalid_argument("UI delta time must be positive and finite");
    if (!impl_ || !impl_->ui) throw std::logic_error("Call enable_ui before begin_ui_frame");
    if (impl_->ui_frame_open || impl_->ui_frame_ready) throw std::logic_error("Render the previous UI frame before beginning another");
    ImGui::SetCurrentContext(impl_->ui);
    ImGui_ImplVulkan_NewFrame();
#ifdef _WIN32
    if (impl_->ui_platform) ImGui_ImplWin32_NewFrame();
#endif
    auto& io=ImGui::GetIO();
    io.DisplaySize={static_cast<float>(width_),static_cast<float>(height_)};
    io.DeltaTime=delta_seconds;
    ImGui::NewFrame();
    impl_->ui_frame_open=true;
}
void VulkanRenderer::finish_ui_frame() {
    if (!impl_ || !impl_->ui_frame_open) throw std::logic_error("No UI frame to finish");
    ImGui::SetCurrentContext(impl_->ui);
    ImGui::Render();
    impl_->ui_frame_open=false;
    impl_->ui_frame_ready=true;
}
#ifdef _WIN32
void VulkanRenderer::set_external_mouse_capture(bool enabled) {
    if (!impl_ || !impl_->ui_platform) throw std::logic_error("Enable Win32 UI first");
    ImGui::SetCurrentContext(impl_->ui);
    ImGui_ImplWin32_SetExternalMouseCapture(enabled);
}
bool VulkanRenderer::handle_ui_message(void* window, unsigned int message,
                                      std::uintptr_t wparam, std::intptr_t lparam) {
    if (!impl_ || !impl_->ui || !impl_->ui_platform) return false;
    ImGui::SetCurrentContext(impl_->ui);
    return ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(window),message,
        static_cast<WPARAM>(wparam),static_cast<LPARAM>(lparam))!=0;
}
#endif
unsigned int VulkanRenderer::antialiasing_samples() const noexcept {
    return impl_?static_cast<unsigned int>(impl_->samples):1;
}
RenderStatistics VulkanRenderer::statistics() const noexcept {return impl_?impl_->stats:RenderStatistics{};}
void VulkanRenderer::set_clear_color(Vec3 color) {
    if (!finite(color) || color.x < 0.0F || color.x > 1.0F
        || color.y < 0.0F || color.y > 1.0F || color.z < 0.0F || color.z > 1.0F)
        throw std::invalid_argument("Clear color components must be finite and between 0 and 1");
    clear_color_ = color;
}
void VulkanRenderer::resize(int width, int height) {
    width_=std::max(1,width); height_=std::max(1,height);
}
void VulkanRenderer::render(const Scene& scene, const EditorCamera& camera, EntityId selected) {
    render(scene,camera,selected,{});
}
void VulkanRenderer::render(const Scene& scene, const EditorCamera& camera, EntityId selected,
                            RenderViewport viewport) {
    if (!finite(camera.position) || !finite(camera.target) || !std::isfinite(camera.vertical_fov_degrees)
        || camera.vertical_fov_degrees<=0 || camera.vertical_fov_degrees>=179)
        throw std::invalid_argument("Invalid camera position/target/FOV");
    if (viewport.x==0 && viewport.y==0 && viewport.width==0 && viewport.height==0)
        viewport={0,0,width_,height_};
    if (viewport.x<0 || viewport.y<0 || viewport.width<=0 || viewport.height<=0
        || viewport.width>width_ || viewport.height>height_
        || viewport.x>width_-viewport.width || viewport.y>height_-viewport.height)
        throw std::invalid_argument("Scene viewport must fit inside the render target");
    initialize();
    try {
        const bool draw_ui=impl_->ui_frame_open || impl_->ui_frame_ready;
        if (impl_->ui_frame_open) finish_ui_frame();
        if (impl_->ui_frame_ready) ImGui::SetCurrentContext(impl_->ui);
        impl_->ui_frame_ready=false;
        const auto started=std::chrono::steady_clock::now();
        const auto& frame=impl_->frame_cache.update(scene,camera,selected,viewport.width,viewport.height,grid_visible_,gizmo_mode_);
        impl_->stats.geometry_cpu_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
        impl_->stats.geometry_builds=impl_->frame_cache.builds;impl_->stats.geometry_cache_hits=impl_->frame_cache.hits;
        impl_->draw(frame,camera,width_,height_,pixels_,viewport,draw_ui,clear_color_);
    } catch (...) { shutdown(); throw; }
}
void VulkanRenderer::render_geometry(const GeometryBuffer& geometry,const EditorCamera& camera,RenderViewport viewport) {
    geometry.validate();
    if (!finite(camera.position)||!finite(camera.target)||!std::isfinite(camera.vertical_fov_degrees)
        ||camera.vertical_fov_degrees<=0||camera.vertical_fov_degrees>=179) throw std::invalid_argument("Invalid camera");
    if (viewport.x==0&&viewport.y==0&&viewport.width==0&&viewport.height==0) viewport={0,0,width_,height_};
    if (viewport.x<0||viewport.y<0||viewport.width<=0||viewport.height<=0||viewport.width>width_||viewport.height>height_
        ||viewport.x>width_-viewport.width||viewport.y>height_-viewport.height) throw std::invalid_argument("Invalid scene viewport");
    initialize();
    try {
        const bool ui=impl_->ui_frame_open || impl_->ui_frame_ready;
        if (impl_->ui_frame_open) finish_ui_frame();
        if (impl_->ui_frame_ready) ImGui::SetCurrentContext(impl_->ui);
        impl_->ui_frame_ready=false;
        FrameGeometry frame; frame.geometry=geometry;
        impl_->draw(frame,camera,width_,height_,pixels_,viewport,ui,clear_color_);
    } catch (...) { shutdown(); throw; }
}
const std::vector<DrawIndexedIndirectCommand>& VulkanRenderer::last_draw_commands() const noexcept {
    static const std::vector<DrawIndexedIndirectCommand> empty;
    return impl_?impl_->last_commands:empty;
}
void VulkanRenderer::present() {
    if (!impl_) return;
    try { impl_->present(); }
    catch (...) { shutdown(); throw; }
}
const std::string& VulkanRenderer::device_name() const noexcept {
    static const std::string empty;
    return impl_?impl_->name:empty;
}
} // namespace engine
