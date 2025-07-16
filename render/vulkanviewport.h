#pragma once

#include <wx/window.h>
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
};
