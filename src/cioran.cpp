#include "cioran-vulkan.h"

namespace cioran {
    vkb::Instance initialize_vulkan()
    {
        vkb::InstanceBuilder vk_instance_builder;

        auto inst_ret = vk_instance_builder.set_app_name("Cioran")
            .request_validation_layers(true)
            .require_api_version(1, 1, 0)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

        vkb::Instance vkb_inst = inst_ret.value();

        return vkb_inst;
    }

    VkSurfaceKHR get_window_surface(SDL_Window* window, VkInstance instance)
    {
        VkSurfaceKHR surface;
        if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) != 0) {
            std::cout << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
            terminate();
        }

        return surface;
    }

    // When getting a physical device we need to make sure we get one that supports
    // Rendering to the surface we created for that purpose
    vkb::PhysicalDevice get_physical_device(vkb::Instance instance, VkSurfaceKHR surface)
    {
        VkPhysicalDeviceVulkan13Features vk13_features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vk13_features.dynamicRendering = true;
        vk13_features.synchronization2 = true;

        VkPhysicalDeviceVulkan12Features vk12_features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12_features.bufferDeviceAddress = true;
        vk12_features.descriptorIndexing = true;

        // Use vkbootstrap to select a GPU
        // We want a GPU that can write to the SDL surface and supports Vulkan 1.3 with the correct features
        vkb::PhysicalDeviceSelector selector { instance };
        vkb::PhysicalDevice physical_device = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(vk13_features)
            .set_required_features_12(vk12_features)
            .set_surface(surface)
            .select()
            .value();

        return physical_device;
    }

    vkb::Swapchain create_swapchain(vkb::PhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, uint32_t window_width, uint32_t window_height, VkFormat& vk_swapchain_format)
    {
        vkb::SwapchainBuilder swapchain_builder { physicalDevice, logicalDevice, surface };

        vkb::Swapchain vkSwapChain = swapchain_builder
            .set_desired_format(VkSurfaceFormatKHR { .format = vk_swapchain_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(window_width, window_height)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

        return vkSwapChain;
    }

    VkCommandPool create_command_pool(VkDevice logicalDevice, uint32_t queue_family_index)
    {
        // Create the command pool
        // A command pool can be seen as an allocator for command buffers.
        VkCommandPoolCreateInfo commandPoolInfo {};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // We indicate that we expect to be able to reset individual command buffers
        // made from this pool.
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queue_family_index;

        VkCommandPool command_pool;
        if (vkCreateCommandPool(logicalDevice, &commandPoolInfo, nullptr, &command_pool) != VK_SUCCESS) {
            std::cout << "Failed to create command pool" << std::endl;
            terminate();
        }

        return command_pool;
    }

    VkCommandBuffer create_command_buffer(VkDevice logicalDevice, VkCommandPool command_pool)
    {
        // Allocate a command buffer from the command pool
        VkCommandBufferAllocateInfo commandBufferInfo {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool = command_pool;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        if (vkAllocateCommandBuffers(logicalDevice, &commandBufferInfo, &command_buffer) != VK_SUCCESS) {
            std::cout << "Failed to allocate command buffer" << std::endl;
            terminate();
        }

        return command_buffer;
    }
}