#include "vulkanviewport.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include "gridoverlay.h"

#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

struct Vec3 {
    float x;
    float y;
    float z;
};

wxBEGIN_EVENT_TABLE(VulkanViewport, wxWindow)
    EVT_PAINT(VulkanViewport::OnPaint)
    EVT_SIZE(VulkanViewport::OnResize)
    EVT_KEY_DOWN(VulkanViewport::OnKeyDown)
    EVT_LEFT_DOWN(VulkanViewport::OnMouseDown)
    EVT_LEFT_UP(VulkanViewport::OnMouseUp)
    EVT_MOTION(VulkanViewport::OnMouseMove)
    EVT_MOUSEWHEEL(VulkanViewport::OnMouseWheel)
    EVT_ERASE_BACKGROUND(VulkanViewport::OnEraseBackground)
    EVT_TIMER(wxID_ANY, VulkanViewport::OnRenderTimer)
wxEND_EVENT_TABLE()

VulkanViewport::VulkanViewport(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    // Vulkan initialization is deferred until InitRenderer() is called
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetFocus();

    renderTimer.SetOwner(this);
    renderTimer.Start(16);
}

void VulkanViewport::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this); // required for wxWidgets
    wxGCDC gdc(dc);

    if (instance == VK_NULL_HANDLE)
    {
        // Avoid initializing while the window is still 0x0 which
        // would trigger an exception in CreateSwapchain.
        wxSize size = GetClientSize();
        if (size.GetWidth() == 0 || size.GetHeight() == 0)
        {
            event.Skip();
            return;
        }

        InitRenderer();
    }
    else
    {
        DrawFrame();
    }

    DrawGridAndAxes(gdc, camera, GetClientSize());

    event.Skip(false);
}

void VulkanViewport::OnResize(wxSizeEvent& event)
{
    if (device != VK_NULL_HANDLE)
    {
        RecreateSwapchain();
    }

    event.Skip();
}

void VulkanViewport::OnKeyDown(wxKeyEvent& event)
{
    const float step = 0.2f;
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    Vec3 forward{ sinYaw, cosYaw, 0.0f };
    Vec3 right{ cosYaw, -sinYaw, 0.0f };

    switch (event.GetKeyCode())
    {
    case 'W': case WXK_UP:
        camera.x += forward.x * step;
        camera.y += forward.y * step;
        camera.z += forward.z * step;
        break;
    case 'S': case WXK_DOWN:
        camera.x -= forward.x * step;
        camera.y -= forward.y * step;
        camera.z -= forward.z * step;
        break;
    case 'A': case WXK_LEFT:
        camera.x -= right.x * step;
        camera.y -= right.y * step;
        camera.z -= right.z * step;
        break;
    case 'D': case WXK_RIGHT:
        camera.x += right.x * step;
        camera.y += right.y * step;
        camera.z += right.z * step;
        break;
    default:
        event.Skip();
        return;
    }
    Refresh();
}

void VulkanViewport::OnMouseDown(wxMouseEvent& event)
{
    mouseDragging = true;
    lastMousePos = event.GetPosition();
    CaptureMouse();
}

void VulkanViewport::OnMouseUp(wxMouseEvent& event)
{
    if (mouseDragging && HasCapture())
        ReleaseMouse();
    mouseDragging = false;
}

void VulkanViewport::OnMouseMove(wxMouseEvent& event)
{
    if (!mouseDragging)
    {
        event.Skip();
        return;
    }

    wxPoint pos = event.GetPosition();
    wxPoint delta = pos - lastMousePos;
    lastMousePos = pos;

    const float sensitivity = 0.005f;
    if (event.ShiftDown())
    {
        const float panScale = 0.01f;
        float cosYaw = std::cos(camera.yaw);
        float sinYaw = std::sin(camera.yaw);
        Vec3 right{ cosYaw, -sinYaw, 0.0f };
        camera.x -= delta.x * panScale * right.x;
        camera.y -= delta.x * panScale * right.y;
        camera.z += delta.y * panScale;
    }
    else
    {
        camera.yaw += delta.x * sensitivity;
        camera.pitch += -delta.y * sensitivity;
        camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);
    }
    Refresh();
}

void VulkanViewport::OnMouseWheel(wxMouseEvent& event)
{
    int rotation = event.GetWheelRotation();
    int delta = event.GetWheelDelta();
    if (delta == 0 || rotation == 0)
    {
        event.Skip();
        return;
    }

    float steps = static_cast<float>(rotation) / static_cast<float>(delta);
    const float stepSize = 0.5f;
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    Vec3 forward{ sinYaw, cosYaw, 0.0f };
    camera.x += forward.x * stepSize * steps;
    camera.y += forward.y * stepSize * steps;

    Refresh();
}

void VulkanViewport::OnRenderTimer(wxTimerEvent&)
{
    Refresh(false);
}

void VulkanViewport::OnEraseBackground(wxEraseEvent&)
{
    // Prevent background clearing to avoid flicker
}



VulkanViewport::~VulkanViewport()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
        CleanupSwapchain();

        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, surface, nullptr);

    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, nullptr);
}


// Initializes Vulkan instance, surface, device and swapchain with complete setup
void VulkanViewport::InitVulkan()
{
    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Perastage";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CustomEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = {
#ifdef _WIN32
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance.");

    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateImageViews();        // Required to access swapchain images
    CreateRenderPass();        // Needed before creating framebuffers
    CreateFramebuffers();      // One per swapchain image
    CreateCommandPool();       // Needed to allocate command buffers
    CreateCommandBuffers();    // Must match number of framebuffers
    RecordCommandBuffers();    // Prepares commands to draw frame
}


void VulkanViewport::CreateSurface()
{
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(GetHandle());
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = hwnd;
    surfaceInfo.hinstance = hinstance;

    if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Win32 Vulkan surface.");
#endif
}

void VulkanViewport::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("No Vulkan-compatible GPUs found.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, families.data());

        for (uint32_t i = 0; i < families.size(); ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsQueueFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (presentSupport)
                presentQueueFamily = i;

            if (graphicsQueueFamily != UINT32_MAX && presentQueueFamily != UINT32_MAX)
            {
                physicalDevice = dev;
                return;
            }
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU with graphics and present support.");
}

// Creates a logical Vulkan device and loads required swapchain extension functions.
void VulkanViewport::CreateLogicalDevice()
{
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::set<uint32_t> uniqueFamilies = { graphicsQueueFamily, presentQueueFamily };
    float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical Vulkan device.");

    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);

    // Load required device-level swapchain extension functions
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));

    if (!vkCreateSwapchainKHR || !vkDestroySwapchainKHR || !vkGetSwapchainImagesKHR ||
        !vkAcquireNextImageKHR || !vkQueuePresentKHR)
    {
        throw std::runtime_error("Failed to load swapchain extension functions.");
    }
}

void VulkanViewport::CreateSwapchain()
{
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    // Surface formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& available : formats)
    {
        if (available.format == VK_FORMAT_B8G8R8A8_UNORM &&
            available.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = available;
            break;
        }
    }

    // Choose present mode
    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data());

    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Always available
    for (const auto& mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenPresentMode = mode;
            break;
        }
    }

    // Set extent from surface or window size
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        swapchainExtent = capabilities.currentExtent;
    }
    else
    {
        wxSize size = GetClientSize();
        if (size.GetWidth() == 0 || size.GetHeight() == 0)
            throw std::runtime_error("Invalid window size for swapchain.");
        swapchainExtent = {
            std::clamp((uint32_t)size.GetWidth(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp((uint32_t)size.GetHeight(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    // Choose number of images
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = chosenFormat.format;
    swapchainInfo.imageColorSpace = chosenFormat.colorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueIndices[] = { graphicsQueueFamily, presentQueueFamily };
    if (graphicsQueueFamily != presentQueueFamily) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueIndices;
    }
    else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = chosenPresentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain.");

    // Store images
    uint32_t swapImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, nullptr);
    swapchainImages.resize(swapImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, swapchainImages.data());

    swapchainImageFormat = chosenFormat.format;
}

// Creates a VkImageView for each swapchain image.
void VulkanViewport::CreateImageViews()
{
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image views.");
        }
    }
}

// Creates a basic render pass with a single color attachment.
void VulkanViewport::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass.");
}

// Creates a framebuffer for each swapchain image view.
void VulkanViewport::CreateFramebuffers()
{
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { swapchainImageViews[i] };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = swapchainExtent.width;
        fbInfo.height = swapchainExtent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

// Creates a command pool for graphics commands.
void VulkanViewport::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool.");
}

// Allocates one command buffer per framebuffer.
void VulkanViewport::CreateCommandBuffers()
{
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers.");
}

// Records draw commands into each command buffer to clear the screen.
void VulkanViewport::RecordCommandBuffers()
{
    for (size_t i = 0; i < commandBuffers.size(); ++i)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin recording command buffer.");

        VkClearValue clearColor = { {{ 0.3f, 0.3f, 0.3f, 1.0f }} }; // base color

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapchainFramebuffers[i];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapchainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to record command buffer.");
    }
}

// Renders the current frame by submitting the recorded command buffer and presenting it.
void VulkanViewport::DrawFrame()
{
    uint32_t imageIndex = 0;

    VkResult result = vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to acquire swapchain image.");

    // Ensure imageIndex is safe to access
    if (imageIndex >= commandBuffers.size())
        throw std::runtime_error("imageIndex out of range for commandBuffers.");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer.");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to present swapchain image.");
}

void VulkanViewport::InitRenderer()
{
    InitVulkan();
    DrawFrame();
}

void VulkanViewport::CleanupSwapchain()
{
    for (VkFramebuffer fb : swapchainFramebuffers)
        vkDestroyFramebuffer(device, fb, nullptr);
    swapchainFramebuffers.clear();

    if (renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView view : swapchainImageViews)
        vkDestroyImageView(device, view, nullptr);
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    if (!commandBuffers.empty())
    {
        vkFreeCommandBuffers(device, commandPool,
            static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }
}

void VulkanViewport::RecreateSwapchain()
{
    wxSize size = GetClientSize();
    if (size.GetWidth() == 0 || size.GetHeight() == 0)
        return;

    vkDeviceWaitIdle(device);

    CleanupSwapchain();

    CreateSwapchain();
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateCommandBuffers();
    RecordCommandBuffers();

    DrawFrame();
}
