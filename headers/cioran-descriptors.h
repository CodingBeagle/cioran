#ifndef CIORAN_DESCRIPTORS_H
#define CIORAN_DESCRIPTORS_H

#include <vector>
#include <span>

#include <vulkan/vulkan.h>

namespace cioran {
    struct DescriptorLayoutBuilder {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        void add_binding(uint32_t binding, VkDescriptorType type);
        void clear();
        VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags stage_flags, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
    };

    // Descriptor pools are used to allocate memory for descriptor sets.
    // They also manage the lifecycle of descriptor sets.
    // When you clear / reset a pool, it destroys all of the descriptor sets allocated from it.
    struct DescriptorAllocator {
        struct PoolSizeRatio {
            VkDescriptorType type;
            float ratio;
        };

        VkDescriptorPool pool;

        void init_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios);
        void clear_descriptors(VkDevice device);
        void destroy_pool(VkDevice device);

        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
    };
}

#endif // CIORAN_DESCRIPTORS_H