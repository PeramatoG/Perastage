#pragma once

#include <wx/window.h>
#include <vulkan/vulkan.h>

// Window-based Vulkan render surface
class VulkanViewport : public wxWindow
{
public:
    explicit VulkanViewport(wxWindow* parent);
    ~VulkanViewport();

private:
    void InitVulkan();
    void CreateSurface();

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

#ifdef _WIN32
    void* GetNativeWindowHandle();
#endif
};
