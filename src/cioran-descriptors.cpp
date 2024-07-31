#include "cioran-descriptors.h"

#include <iostream>

namespace cioran {
    void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
        // VkDescriptorSetLayoutBinding describes the binding index for a shader stage,
        // And the descriptor type that is bound to that index.
        // DescriptorCount is the number of descriptors contained in the binding.
        VkDescriptorSetLayoutBinding newbind {};
        newbind.binding = binding;
        newbind.descriptorCount = 1;
        newbind.descriptorType = type;

        bindings.push_back(newbind);
    }

    void DescriptorLayoutBuilder::clear() {
        bindings.clear();
    }

    VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shader_flags, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
    {
        for (auto& b : bindings) {
            b.stageFlags |= shader_flags;
        }

        VkDescriptorSetLayoutCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = pNext;
        info.pBindings = bindings.data();
        info.bindingCount = (uint32_t)bindings.size();
        info.flags = flags;

        VkDescriptorSetLayout set;
        if (vkCreateDescriptorSetLayout(device, &info, nullptr, &set) != VK_SUCCESS) {
            std::cerr << "Failed to create descriptor set layout" << std::endl;
            std::terminate();
        }

        return set;
    }

    // VkDescriptorPools are used to allocate descriptor sets, and the pool owns the storage
    // For the descriptor sets.
    void DescriptorAllocator::init_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (PoolSizeRatio ratio : pool_ratios) {
            poolSizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = uint32_t(ratio.ratio * max_sets)
            });
        }

        VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.flags = 0;
        pool_info.maxSets = max_sets;
        pool_info.poolSizeCount = (uint32_t)poolSizes.size();
        pool_info.pPoolSizes = poolSizes.data();

        vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
    }

    VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;

        // A VkDescriptorSet is an object that holds a collection of descriptors,
        // Which are used to link shader resources to the shaders in a Vulkan pipeline
        VkDescriptorSet descriptor_set; 
        if (vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            std::cerr << "Failed to allocate descriptor set" << std::endl;
            std::terminate();
        }

        return descriptor_set;
    }

    void DescriptorAllocator::clear_descriptors(VkDevice device)
    {
        vkResetDescriptorPool(device, pool, 0);
    }

    void DescriptorAllocator::destroy_pool(VkDevice device)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
}