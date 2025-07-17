#pragma once

#include <wx/window.h>
#include <wx/event.h>
#include "irenderviewport.h"
#include <vulkan/vulkan.h>

// Window-based Vulkan render surface using Vulkan API
class VulkanViewport : public IRenderViewport
{
public:
    explicit VulkanViewport(wxWindow* parent);
    ~VulkanViewport();

    // Initializes the Vulkan renderer
    void InitRenderer() override;

    // Renders the current frame to the swapchain
    void DrawFrame();
    void DrawOverlay(wxDC& dc);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);

    // Paint handler used to trigger drawing
    void OnPaint(wxPaintEvent& event);

    // Resize handler recreates the swapchain when the window size changes
    void OnResize(wxSizeEvent& event);

    // Recreates swapchain and related resources
    void RecreateSwapchain();

    // Cleans up swapchain-dependent resources
    void CleanupSwapchain();

private:
    // Main Vulkan initialization pipeline
    void InitVulkan();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void RecordCommandBuffers();

    // Vulkan instance and surface
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Physical and logical GPU
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;

    // Swapchain and related data
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    // Framebuffers and render pass
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // Command buffers and pool
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // Vulkan device-level function pointers for swapchain operations
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

#ifdef _WIN32
    // Retrieves the native window handle (HWND)
    void* GetNativeWindowHandle();
#endif

    struct SimpleCamera {
        float x = 0.0f;
        float y = 2.0f;
        float z = 5.0f;
        float yaw = 0.0f;   // radians
        float pitch = 0.0f; // radians
        float fov = 1.0f;   // vertical FOV in radians
    } camera;

    bool mouseDragging = false;
    wxPoint lastMousePos;

    wxDECLARE_EVENT_TABLE();
};
