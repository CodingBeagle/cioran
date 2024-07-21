#include <iostream>

// Vulkan
#include <vulkan/vulkan.h>
#include "vkbootstrap/VkBootstrap.h"

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

VkInstance vk_instance;
VkDevice vk_device;
VkPhysicalDevice vk_physical_device;

VkDebugUtilsMessengerEXT vk_debug_messenger;
VkSurfaceKHR vk_surface;

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
        800, 600,
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
    }

    // Clean up SDL resources
    SDL_Quit();    

    return 0;
}