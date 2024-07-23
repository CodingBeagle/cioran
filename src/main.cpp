#include <iostream>
#include <vector>

// Vulkan
#include <vulkan/vulkan.h>
#include "vkbootstrap/VkBootstrap.h"

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

// Function prototypes
VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);
VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);
VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);
void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

struct FrameData {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;
};

VkInstance vk_instance;
VkDevice vk_device;
VkPhysicalDevice vk_physical_device;

VkDebugUtilsMessengerEXT vk_debug_messenger;
VkSurfaceKHR vk_surface;

VkSwapchainKHR vk_swapchain;
VkFormat vk_swapchain_format;
std::vector<VkImage> vk_swapchain_images;
std::vector<VkImageView> vk_swapchain_image_views;
VkExtent2D vk_swapchain_extent;

VkQueue graphics_queue;
uint32_t graphics_queue_family;

int frame_number { 0 };
constexpr unsigned int FRAME_OVERLAP { 2 };

FrameData frames[FRAME_OVERLAP];
FrameData& get_current_frame() { 
    return frames[frame_number % FRAME_OVERLAP];
};

int window_height = 600;
int window_width = 800;

int main(int argc, char **argv) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0) {
        const char* sdl_error = SDL_GetError();

        std::cout << "Failed to initialize SDL: " << sdl_error << std::endl;
        terminate();
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow(
        "Cioran",
        window_width, window_height,
        SDL_WINDOW_VULKAN);

    if (window == nullptr) {
        std::cout << "Failed to create window: " << SDL_GetError() << std::endl;
        terminate();
    }

    // Initialize Vulkan
    vkb::InstanceBuilder vk_instance_builder;

    auto inst_ret = vk_instance_builder.set_app_name("Cioran")
        .request_validation_layers(true)
        .require_api_version(1, 1, 0)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    vk_instance = vkb_inst.instance;
    vk_debug_messenger = vkb_inst.debug_messenger;

    if (SDL_Vulkan_CreateSurface(window, vk_instance, nullptr, &vk_surface) != 0) {
        std::cout << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        terminate();
    }

    VkPhysicalDeviceVulkan13Features vk13_features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    vk13_features.dynamicRendering = true;
    vk13_features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features vk12_features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    vk12_features.bufferDeviceAddress = true;
    vk12_features.descriptorIndexing = true;

    // Use vkbootstrap to select a GPU
    // We want a GPU that can write to the SDL surface and supports Vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector { vkb_inst };
    vkb::PhysicalDevice physical_device = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(vk13_features)
        .set_required_features_12(vk12_features)
        .set_surface(vk_surface)
        .select()
        .value();

    // Create the final Vulkan device
    vkb::DeviceBuilder device_builder { physical_device };
    vkb::Device vkb_device = device_builder.build().value();

    vk_device = vkb_device.device;
    vk_physical_device = physical_device.physical_device;

    // Create the swapchain
    vkb::SwapchainBuilder swapchain_builder { physical_device, vk_device, vk_surface };

    vk_swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkSwapChain = swapchain_builder
        .set_desired_format(VkSurfaceFormatKHR { .format = vk_swapchain_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(window_width, window_height)
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    vk_swapchain_extent = vkSwapChain.extent;
    vk_swapchain = vkSwapChain.swapchain;
    vk_swapchain_images = vkSwapChain.get_images().value();
    vk_swapchain_image_views = vkSwapChain.get_image_views().value();

    // Get graphics queue
    graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // Create command structures

    // Create the command pool
    // A command pool can be seen as an allocator for command buffers.
    VkCommandPoolCreateInfo commandPoolInfo {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // We indicate that we expect to be able to reset individual command buffers
    // made from this pool.
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = graphics_queue_family;

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // Create a command pool for each frame
        if (vkCreateCommandPool(
            vk_device,
            &commandPoolInfo,
            nullptr,
            &frames[i].command_pool) != VK_SUCCESS) 
        {
            std::cout << "Failed to create command pool" << std::endl;
            terminate();
        }

        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo commandBufferAllocInfo {};
        commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocInfo.commandPool = frames[i].command_pool;
        commandBufferAllocInfo.commandBufferCount = 1;
        commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        if (vkAllocateCommandBuffers(
            vk_device,
            &commandBufferAllocInfo,
            &frames[i].command_buffer) != VK_SUCCESS) 
        {
            std::cout << "Failed to allocate command buffer" << std::endl;
            terminate();
        }
    }

    // Initialize sync structures
    // One fence to control when the GPU has finished rendering the frame,
    // And 2 semaphores to synchronize rendering with swapchain.
    // We want the fence to start signalled so we can wait on it on the first frame.
    VkFenceCreateInfo fenceCreateInfo = fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = semaphore_create_info(0);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        if (vkCreateFence(vk_device, &fenceCreateInfo, nullptr, &frames[i].render_fence) != VK_SUCCESS) {
            std::cout << "Failed to create fence" << std::endl;
            terminate();
        }

        if (vkCreateSemaphore(vk_device, &semaphoreCreateInfo, nullptr, &frames[i].swapchain_semaphore) != VK_SUCCESS) {
            std::cout << "Failed to create swapchain semaphore" << std::endl;
            terminate();
        }

        if (vkCreateSemaphore(vk_device, &semaphoreCreateInfo, nullptr, &frames[i].render_semaphore) != VK_SUCCESS) {
            std::cout << "Failed to create render semaphore" << std::endl;
            terminate();
        }
    }

    bool running = true;
    while (running) {
        // SDL_PollEvent is the favored way of receving system events since it can be done from the main loop and does not suspend the main loop
        // while waiting for an event to be posted.
        // Common practice is to use a while loop to process all events in the event queue.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Draw

        // Wait for the GPU to finish its rendering work with our fence
        if (vkWaitForFences(vk_device, 1, &get_current_frame().render_fence, true, 1000000000) != VK_SUCCESS) {
            std::cout << "Failed to wait for fence" << std::endl;
            terminate();
        }

        // Reset the fence.
        // Fences have to be reset between uses.
        if (vkResetFences(vk_device, 1, &get_current_frame().render_fence) != VK_SUCCESS) {
            std::cout << "Failed to reset fence" << std::endl;
            terminate();
        }

        // Request image from the swapchain
        // vkAcquireNextImageKHR will block the thread with a maximum for the timeout set
        // in the case that no images are available for use.
        uint32_t swapchain_image_index;
        if (vkAcquireNextImageKHR(
            vk_device,
            vk_swapchain,
            1000000000,
            get_current_frame().swapchain_semaphore,
            VK_NULL_HANDLE,
            &swapchain_image_index) != VK_SUCCESS)
        {
            std::cout << "Failed to acquire next image" << std::endl;
            terminate();
        }

        VkCommandBuffer cmd = get_current_frame().command_buffer;

        // Now that we are sure that the commands finished executing, we can safely
        // reset the command buffer to begin recording again.
        // Resetting the buffer will completely remove all commands and free its memory.
        if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
            std::cout << "Failed to reset command buffer" << std::endl;
            terminate();
        }

        // Begin the command buffer recording.
        // We will use this command buffer exactly once, so we want to let Vulkan know that.
        VkCommandBufferBeginInfo cmdBeginInfo = command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // Start the recording
        if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS) {
            std::cout << "Failed to begin command buffer" << std::endl;
            terminate();
        }

        // Make the swapchain image into writeable mode before rending
        // Newly created images will be in the VK_IMAGE_LAYOUT_UNDEFINED (the don't care layout)
        // The new layout is VK_IMAGE_LAYOUT_GENERAL.
        // This is a general purpose layout which allows for reading and writing from the image.
        // It's not the most optimal layout for rendering, but it's a good starting point.
        transition_image(cmd, vk_swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        // Make a clear color from frame number.
        // This will flash!
        VkClearColorValue clearColor;
        float flash = std::abs(std::sin(frame_number / 120.0f));
        clearColor = { { 0.0f, 0.0f, flash, 1.0f } };

        VkImageSubresourceRange subresourceRange = image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

        // Clear image
        vkCmdClearColorImage(cmd, vk_swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);

        // Make the swapchain image into presentable mode
        // The swapchain only allows layouts in the form of VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting to the screen.
        transition_image(cmd, vk_swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // Finalize the command buffer (we can no longer add commands, but it can now be executed)
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            std::cout << "Failed to end command buffer" << std::endl;
            terminate();
        }

        // Prepare submission to the queue
        // We want to wait on the present semaphore, as that semaphore is signaled when the swapchain is ready
        // We will signal the render semaphore, to signal that rendering is finished
        VkCommandBufferSubmitInfo cmdSubmitInfo = command_buffer_submit_info(cmd);

        VkSemaphoreSubmitInfo waitInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);
        VkSemaphoreSubmitInfo signalInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

        VkSubmitInfo2 submit = submit_info(&cmdSubmitInfo, &signalInfo, &waitInfo);

        // Submit the command buffer to the queue and execute it
        // render fence will now block until the graphic commands finish execution
        if (vkQueueSubmit2(graphics_queue, 1, &submit, get_current_frame().render_fence) != VK_SUCCESS) {
            std::cout << "Failed to submit to queue" << std::endl;
            terminate();
        }

        // Prepare present
        // This will put the image we just rendered to into the visible window
        // We want to wait on the render semaphore for that,
        // as its necessary that drawing commands have finished before the image is displayed to the user
        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &vk_swapchain;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &get_current_frame().render_semaphore;

        presentInfo.pImageIndices = &swapchain_image_index;

        if (vkQueuePresentKHR(graphics_queue, &presentInfo) != VK_SUCCESS) {
            std::cout << "Failed to present image" << std::endl;
            terminate();
        }

        frame_number++;
    }

    // Make sure that the GPU has stopped doing its things
    vkDeviceWaitIdle(vk_device);

    // Destroy command pools
    // Destroying the pools will also destroy the command buffers allocated from them
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        vkDestroyCommandPool(vk_device, frames[i].command_pool, nullptr);
    }

    // Destroy swapchain resources
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    for (int i = 0; i < vk_swapchain_image_views.size(); i++) {
        vkDestroyImageView(vk_device, vk_swapchain_image_views[i], nullptr);
    }

    // Destroy surface
    vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);

    // Destroy Device
    vkDestroyDevice(vk_device, nullptr);

    // Destroy Debug Messenger
    vkb::destroy_debug_utils_messenger(vk_instance, vk_debug_messenger);

    // Destroy Instance
    vkDestroyInstance(vk_instance, nullptr);

    // Clean up SDL resources
    SDL_Quit();    

    return 0;
}

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
    VkSemaphoreSubmitInfo semaphoreSubmitInfo {};
    semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphoreSubmitInfo.stageMask = stageMask;
    semaphoreSubmitInfo.semaphore = semaphore;
    semaphoreSubmitInfo.deviceIndex = 0;
    semaphoreSubmitInfo.value = 1;
    
    return semaphoreSubmitInfo;
}

VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd) {
    VkCommandBufferSubmitInfo cmdSubmitInfo {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = cmd;
    cmdSubmitInfo.deviceMask = 0;

    return cmdSubmitInfo;
}

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo) {
    VkSubmitInfo2 submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    submitInfo.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreInfo;

    submitInfo.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfo;

    submitInfo.commandBufferInfoCount = cmd == nullptr ? 0 : 1;
    submitInfo.pCommandBufferInfos = cmd;

    return submitInfo;
}

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask) {
    VkImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
    // Pipeline barriers are synchronization primitives that are used to control the ordering
    // of memory operations performed by the GPU.
    // They are recorded into command buffers and executed when the command buffer is submitted
    // to a queue.
    // They ensure that certain operations complete before others begin, which is a way to enforce
    // dependencies between different stages of a graphics or compute pipeline.

    // Best way I've been able to think about them so far is that image barriers describe a two-way relationship.
    // You need to specify the source conditions which the barrier will wait for,
    // and then the destination conditions, which are the stages and operations that can then be performed.
    // By having both source conditions and destination conditions, you can be very precise in describing what to wait for, and what
    // Is then allowed to continue once those conditions are met.

    VkImageMemoryBarrier2 imageBarrier {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    // The source stage mask specifies the pipeline stages that must be complete before the barrier begins.
    // VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT means that all stages (compute, transfer, graphics) must be completed before the barrier.
    // It is a more conservative type of synchronization that can potentially impact performance. 
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    // Source stage access determines the types of memory access that need to be completed before the barrier.
    // While srcStageMask defines WHEN synchronization should occur, srcAccessMask defines WHAT needs to be synchronized.
    // It allows you to be more precise in synchronization control.
    // VK_ACCESS_2_MEMORY_WRITE_BIT means all write accesses to memory must be completed before the barrier, and is a broad scope.
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;

    // Setting destination stage mask to VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT means that all stages can be executed after the source conditions are met.
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    // Setting destination access mask to VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT means
    // that both memory write and memory read operations are allowed to proceed.
    // Practically, by a combination of VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT and these flags, we say that
    // any stage in the pipeline can perform any kind of read and write operation after the source conditions are met.
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    // Transition the memory layout from the current layout to a new layout
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    imageBarrier.subresourceRange = image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    return beginInfo;
}

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags) {
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = flags;
    return fenceCreateInfo;
}

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags) {
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = flags;
    return semaphoreCreateInfo;
}