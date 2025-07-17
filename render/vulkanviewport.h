#pragma once

#include <wx/window.h>
#include <wx/event.h>
#include <wx/timer.h>
#include "irenderviewport.h"
#include <vulkan/vulkan.h>
#include "camera.h"

// Window-based Vulkan render surface using Vulkan API
class VulkanViewport : public wxWindow, public IRenderViewport
{
public:
    explicit VulkanViewport(wxWindow* parent);
    ~VulkanViewport();

    wxWindow* GetWindow() override { return this; }
    void InitRenderer() override;

    void DrawFrame();
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);

    void OnPaint(wxPaintEvent& event);
    void OnResize(wxSizeEvent& event);

    void RecreateSwapchain();
    void CleanupSwapchain();

private:
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

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

#ifdef _WIN32
    void* GetNativeWindowHandle();
#endif

    SimpleCamera camera;

    bool mouseDragging = false;
    wxPoint lastMousePos;

    wxTimer renderTimer;

    void OnRenderTimer(wxTimerEvent&);
    void OnEraseBackground(wxEraseEvent& event);

    wxDECLARE_EVENT_TABLE();
};
