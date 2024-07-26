#ifndef CIORAN_VULKAN_H
#define CIORAN_VULKAN_H

#include <iostream>
#include <deque>
#include <functional>

// Currently my own vulkan implementation depends on VkBootstrap.
#include "vkbootstrap/VkBootstrap.h"

// currently my own vulkan implementation depends on SDL
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

// VMA
#include "vk_mem_alloc.h"

namespace cioran {
    struct VkDeletionQueue {
        std::deque<std::function<void()>> deletors;

        void push_function(std::function<void()>&& function) {
            deletors.push_back(function);
        }

        void flush() {
            // reverse iterate the deletion queue to execute all the functions
            for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
                (*it)(); // call functors
            }

            deletors.clear();
        }
    };

    struct AllocatedImage {
        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;
        VkExtent3D image_extent;
        VkFormat image_format;
    };

    vkb::Instance initialize_vulkan();
    VkSurfaceKHR get_window_surface(SDL_Window* window, VkInstance instance);
    vkb::PhysicalDevice get_physical_device(vkb::Instance instance, VkSurfaceKHR surface);
    vkb::Swapchain create_swapchain(vkb::PhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, uint32_t window_width, uint32_t window_height, VkFormat& vk_swapchain_format);

    VkImageCreateInfo create_image_create_info(VkFormat format, VkImageUsageFlags usage, VkExtent3D extent);
    VkImageViewCreateInfo create_image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

    VkCommandPool create_command_pool(VkDevice logicalDevice, uint32_t queue_family_index);
    VkCommandBuffer create_command_buffer(VkDevice logicalDevice, VkCommandPool command_pool);
}

#endif // CIORAN_VULKAN_H