#ifndef CIORAN_VULKAN_H
#define CIORAN_VULKAN_H

#include <iostream>

// Currently my own vulkan implementation depends on VkBootstrap.
#include "vkbootstrap/VkBootstrap.h"

// currently my own vulkan implementation depends on SDL
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

namespace cioran {
    vkb::Instance initialize_vulkan();
    VkSurfaceKHR get_window_surface(SDL_Window* window, VkInstance instance);
    vkb::PhysicalDevice get_physical_device(vkb::Instance instance, VkSurfaceKHR surface);
    vkb::Swapchain create_swapchain(vkb::PhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, uint32_t window_width, uint32_t window_height, VkFormat& vk_swapchain_format);

    VkCommandPool create_command_pool(VkDevice logicalDevice, uint32_t queue_family_index);
    VkCommandBuffer create_command_buffer(VkDevice logicalDevice, VkCommandPool command_pool);
}

#endif // CIORAN_VULKAN_H