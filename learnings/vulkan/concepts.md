# Concepts

## General Flow

1. Create Vulkan instance
2. Create a Surface KHR object
3. Pick a physical device
4. Create a logical device from the physical device
5. Create the swapchain
6. Create command buffers to rendering
7. Create necessary synchronization primitives

## VkInstance

A vulkan instance is the first thing you create in order to initialize Vulkan, and interface with it.

Vulkan has no global state. All is stored in the VkInstance object specific to your application.

When creating an instance, you have the ability to specify global layers to enable for the instance, extensions to enable, and also debug messengers, and more.

## VkSurfaceKHR

The VkSurfaceKHR object is a handle which refers to a native platform surface.

A surface is a platform-specifc way to present rendered images to a window on a OS.

A surface doesn't actually contain or represent image data, but it's an interface/context for the Vulkan instance to present rendered images to a window.

A surface will be associated with a swapchain.

## VkPhysicalDevice

A VkPhysicalDevice represents a handle to a physical device, such as a GPU.

It's used to query properties and capabilities of the physical device, which you then use to make decisions about which physical device to use (if the platform has multiple ones), and then you create a logical device from your requirements.

## VkDevice

VkDevice represents a logical device in Vulkan.

They are created from physical devices.

A VkDevice allows access to the GPU, such as issuing commands to it.

The VkDevice can be seen as an instance of the GPU driver that you use to actually interface with the GPU.

## VkSwapchainKHR

A swapchain in Vulkan is an abstraction for an array of presentable images that are associated with a surface. Hence why a surface is associated with a swapchain.

Each presentable image is represented by the *VkImage* object.

One image is displayed at a time.

An application will render to the image, and then queues the image for presentation to the surface.

The presentable images of the swapchain are owned by the presentation engine.

Applications can acquire use of a presentable image by calling *vkAcquireNextImageKHR*.

## Queues and Queue Families

Vulkan exposes one or more devices, each of which exposes one or more queues.

A queue represents a channel through which commands are submitted to the GPU. In order to submit commands to a queue, you record commands into a command buffer, and then submit the command buffer to the queue.

That is, queues provide an interface to the execution engines of a device.

On a physical device, queues are categorized into *queue families*. Each family supports one or more types of functionality, and each family can have one or multiple queues in it.

For example, you have a queue family which supports graphics, compute or transfer. Some queue families can have overlapping capabilities.

All queues within the same family are considered to be *compatible*, and so work produced for a family of queues can be executed on any queue within that family.

## Command Buffers

Command Buffers are used to record commands which can then be submitted to a device queue for execution.

Unless otherwise specified, and without explicit synchronization, various commands submitted to a queue via command buffers may execute in arbitrary order relative to each other, and/or concurrently. Memory side effects of those commands may not be directly visible to other commands without explicit memory dependencies.

Without explicit synchronization, action commands may overlap execution or execute out of order, potentially leading to data races. Each action command uses the state in effect at the point where the command occurs in the command buffer, regardless of when it is executed.

*However* there are some implicit ordering guarentees provided by Vulkan, which ensures that the order in which commands are submitted is meaningful, and to avoid unnecessary complexity in common operations.

In order to create command buffers, you need to have created a command pool to allocate command buffers from.

### Command Pools

Command pools are opaque objects that command buffer memory is allocated from.

### Commands

Commands are categorized into four different types:

- Action
    - Action commands perform operations that can udpate values in memory (draw commands, dispatch commands, etc...)
- State
    - State setting commands update the current state of a command buffer, affecting the operation of future action commands.
- Synchronization
    - Synchronization commands impose ordering constraints on action commands, by introducing explicit execution and memory dependencies.
- Indirection
    - Indirection commands execute other commands which were not directly recorded in the same command buffer.

### Submission Order

Submission Order is an important concept in Vulkan.

Within a command buffer, the order in which commands are recorded is in the sequence of first to last.
The order in which multiple command buffers are submitted in "pSubmits" parameter determines the order of the command buffers to be executed, first to last (lowest index to highest).
The order in which vkQueueSubmit commands are executed on the CPU also determines the order, from first to last.

However, this only counts within a single queue.

Across commands buffers in multiple queues, no implicit synchronization takes place. If this is required, synchronization commands can be used.

## Synchronization Primitives

- Fences
    - Can be used to communicate to a host (CPU / Application) that execution of some task on the device (GPU) has completed.
- Semaphores
    - Used to synchronize operations within a or across command queues.
- Pipeline Barriers
    - Used within a command buffer to control the order of execution of commands and to manage memory dependencies.