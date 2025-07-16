#pragma once

#include <wx/window.h>
#include "irenderviewport.h"
#include <vulkan/vulkan.h>


// Window-based Vulkan render surface
class VulkanViewport : public IRenderViewport
{
public:
    explicit VulkanViewport(wxWindow* parent);
    ~VulkanViewport();

    void InitRenderer() override;

private:
    void InitVulkan();
    void CreateSurface();

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    void PickPhysicalDevice();
    void CreateLogicalDevice();

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    void CreateSwapchain();

    // Vulkan device-level function pointers for swapchain operations
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;


#ifdef _WIN32
    void* GetNativeWindowHandle();
#endif
};
