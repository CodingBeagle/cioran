#include <iostream>
#include <vector>

// Vulkan
#include <vulkan/vulkan.h>
#include "vkbootstrap/VkBootstrap.h"

// VMA
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

// Cioran
#include "cioran-vulkan.h"

// Function prototypes
VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);
VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);
VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);
void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
void vma_log_error(VkResult result);

struct FrameData {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;
    cioran::VkDeletionQueue deletion_queue;
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

cioran::VkDeletionQueue main_deletion_queue;

VmaAllocator vma_allocator;

cioran::AllocatedImage draw_image;
VkExtent2D draw_extent;

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
    auto vulkan_init = cioran::initialize_vulkan();
    vk_instance = vulkan_init.instance;
    vk_debug_messenger = vulkan_init.debug_messenger;

    // Get a window surface that we can render to from the vulkan swapchain
    vk_surface = cioran::get_window_surface(window, vk_instance);

    // Get the physical device that we will use for rendering
    auto physical_device = cioran::get_physical_device(vulkan_init, vk_surface);

    // Create the final Vulkan device
    vkb::DeviceBuilder device_builder { physical_device };
    vkb::Device vkb_device = device_builder.build().value();
    vk_device = vkb_device.device;
    vk_physical_device = physical_device.physical_device;

    // Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo {};
    allocatorInfo.physicalDevice = vk_physical_device;
    allocatorInfo.device = vk_device;
    allocatorInfo.instance = vk_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    auto allocatorCreateResult = vmaCreateAllocator(&allocatorInfo, &vma_allocator);
    if (allocatorCreateResult != VK_SUCCESS) {
        vma_log_error(allocatorCreateResult);
        terminate();
    }

    main_deletion_queue.push_function([=]() {
        vmaDestroyAllocator(vma_allocator);
    });

    // Create the swapchain
    vk_swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
    auto swapchain = cioran::create_swapchain(physical_device, vk_device, vk_surface, window_width, window_height, vk_swapchain_format);

    vk_swapchain_extent = swapchain.extent;
    vk_swapchain = swapchain.swapchain;
    vk_swapchain_images = swapchain.get_images().value();
    vk_swapchain_image_views = swapchain.get_image_views().value();

    // Draw image size will match the window
    VkExtent3D drawImageExtent = {
        window_width,
        window_height,
        1
    };

    // Hardcoding the draw format to 32 bit float
    draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image.image_extent = drawImageExtent;

    // All images and buffers must fill in a UsageFlags with what they will be
    // used for. This allows the driver to perform optimizations in the background
    // depending on what that buffer or image is going to do later.
    VkImageUsageFlags drawImageUsages {};
    // The image can be used as the source of a transfer command (you can copy from)
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    // The image can be used as the destination of a transfer command (you can copy to)
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // The image can be used to create a VkImageView suitable for occupying a 
    // VkDescriptorSet slot of type VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    // The image can be used as a target for rendering operations.
    // Specifically, it can be used as a color attachment in a render pass.
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = cioran::create_image_create_info(draw_image.image_format, drawImageUsages, drawImageExtent);
    
    // For the draw image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo rimg_alloc_info = {};
    // Specify that this is a texture that will never be accessed by the CPU.
    rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    auto createImageResult = vmaCreateImage(vma_allocator, &rimg_info, &rimg_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);
    if (createImageResult != VK_SUCCESS) {
        
        vma_log_error(createImageResult);
        terminate();
    }

    // Build an image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = cioran::create_image_view_create_info(draw_image.image_format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);

    if (vkCreateImageView(vk_device, &rview_info, nullptr, &draw_image.image_view) != VK_SUCCESS) {
        std::cout << "Failed to create image view" << std::endl;
        terminate();
    }

    // Add image resources to deletion queue
    main_deletion_queue.push_function([=]() {
        vkDestroyImageView(vk_device, draw_image.image_view, nullptr);
        vmaDestroyImage(vma_allocator, draw_image.image, draw_image.allocation);
    });

    // Get graphics queue
    graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // Create command structures
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // Create a command pool for each frame
        auto command_pool = cioran::create_command_pool(vk_device, graphics_queue_family);
        frames[i].command_pool = command_pool;

        // Allocate the default command buffer that we will use for rendering
        auto command_buffer = cioran::create_command_buffer(vk_device, command_pool);
        frames[i].command_buffer = command_buffer;
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

        get_current_frame().deletion_queue.flush();

        // Reset the fence.
        // Fences have to be reset between uses.
        if (vkResetFences(vk_device, 1, &get_current_frame().render_fence) != VK_SUCCESS) {
            std::cout << "Failed to reset fence" << std::endl;
            terminate();
        }

        // Request presentable image from the swapchain
        // vkAcquireNextImageKHR will block the thread with a maximum for the timeout set in the case that no images are available for use.
        // The semaphore is used to singal when the presentation engine is finished reading from the image.
        // This is because when you aquire the image, the presentation engine might still be reading from it.
        // This semaphore has to be used in the command buffer to make sure nothing tampers with the memory before its signalled.
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

        // The wait_swapchain_semaphore will be provided as a wait semaphore to the submit command.
        // In this case, it means that all commands BEFORE VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR stage
        // is allowed to execute, but the pipeline will be stalled at this stage until the semaphore is signalled.
        // VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR == The stage where, besides other things, final color output is written.
        // In this case, we need to make sure that the swapchain is done reading from the image data before we write new data to it.
        VkSemaphoreSubmitInfo wait_swapchain_semaphore = semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);

        // Here, we specify that once all graphic pipeline stages are done, we singal this semaphore.
        VkSemaphoreSubmitInfo signal_render_semaphore = semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

        VkSubmitInfo2 submit = submit_info(&cmdSubmitInfo, &signal_render_semaphore, &wait_swapchain_semaphore);

        // Submit the command buffer to the queue and execute it
        // render fence will now block until the graphic commands finish execution
        // The fence will be signaled once all submitted command buffers have completed execution.
        // We use this fence in the beginning of the render loop to make sure the pipeline has completed rendering before we
        // render a new frame.
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

        // Destroy sync objects
        vkDestroyFence(vk_device, frames[i].render_fence, nullptr);
        vkDestroySemaphore(vk_device, frames[i].swapchain_semaphore, nullptr);
        vkDestroySemaphore(vk_device, frames[i].render_semaphore, nullptr);

        frames[i].deletion_queue.flush();
    }

    main_deletion_queue.flush();

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

void vma_log_error(VkResult result) {
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cerr << "Failed to create image: Out of host memory." << std::endl;
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cerr << "Failed to create image: Out of device memory." << std::endl;
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cerr << "Failed to create image: Initialization failed." << std::endl;
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            std::cerr << "Failed to create image: Memory map failed." << std::endl;
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            std::cerr << "Failed to create image: Layer not present." << std::endl;
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            std::cerr << "Failed to create image: Extension not present." << std::endl;
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            std::cerr << "Failed to create image: Feature not present." << std::endl;
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            std::cerr << "Failed to create image: Too many objects." << std::endl;
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            std::cerr << "Failed to create image: Format not supported." << std::endl;
            break;
        case VK_ERROR_FRAGMENTED_POOL:
            std::cerr << "Failed to create image: Fragmented pool." << std::endl;
            break;
        default:
            std::cerr << "Failed to create image: Unknown error." << std::endl;
            break;
    }
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
    // VkImageMemoryBarrier is a type of memory barrier.
    // It is specialized in handling a layout transition between stage and memory access dependencies.
    // A memory barrier can ensure correct ordering of memory operations between different stages in the graphics pipeline,
    // and even between different command buffers.

    // Memory barriers have the following important components:
    // - Source Stage Mask: The pipeline stages that must be complete before the barrier.
    // - Source Access Mask: The types of memory access that must be complete within the source stage mask before the barrier.
    // - Destination Stage Mask: Specifies the pipeline stages that must wait for the barrier.
    // - Destination Access Mask: Specifies the types of memory access within the destination stage that must wait for the barrier.

    // The VkImageMemoryBarrier additionally has an old and new layout transition, which will happen
    // after the source masks, but before the destination masks.
    VkImageMemoryBarrier2 imageBarrier {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    // VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT = We wait for all commands in all stages to complete.
    // VK_ACCESS_2_MEMORY_WRITE_BIT = All memory write commands within all stages must be complete
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;

    // VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT = All commands for all stages must wait for sources + transition to complete
    // And both read and write operations of all commands in all stages must wait for sources + to complete
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
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