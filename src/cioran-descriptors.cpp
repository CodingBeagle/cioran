#include "cioran-descriptors.h"

#include <iostream>

namespace cioran {
    void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
        // VkDescriptorSetLayoutBinding describes the binding index for a shader stage,
        // And the descriptor type that is bound to that index.
        // DescriptorCount is the number of descriptors contained in the binding.
        VkDescriptorSetLayoutBinding new_bind {};
        new_bind.binding = binding;
        new_bind.descriptorType = type;
        new_bind.descriptorCount = 1;

        bindings.push_back(new_bind);
    }

    void DescriptorLayoutBuilder::clear() {
        bindings.clear();
    }

    VkDescriptorSetLayout  DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shader_flags, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
    {
        for (auto& b : bindings) {
            b.stageFlags = shader_flags;
        }

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info {};
        descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.pNext = pNext;
        descriptor_set_layout_create_info.pBindings = bindings.data();
        descriptor_set_layout_create_info.flags = flags;

        VkDescriptorSetLayout descriptor_set_layout;
        if (vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            std::cerr << "Failed to create descriptor set layout" << std::endl;
            std::terminate();
        }

        return descriptor_set_layout;
    }

    // VkDescriptorPools are used to allocate descriptor sets, and the pool owns the storage
    // For the descriptor sets.
    void DescriptorAllocator::init_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
    {
        // A VkDescriptorPoolSize is used to specify a specific type of descriptor,
        // and the number of descriptors of that type that a pool should be able to allocate.
        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (PoolSizeRatio ratio : pool_ratios) {
            VkDescriptorPoolSize pool_size {};
            pool_size.type = ratio.type;
            pool_size.descriptorCount = static_cast<uint32_t>(max_sets * ratio.ratio);
            pool_sizes.push_back(pool_size);
        }

        // VkDescriptorPoolCreateInfo is used to create a descriptor pool.
        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = 0;
        // The maximum number of descriptor sets that can be allocated from the pool.
        pool_info.maxSets = max_sets;
        // Provide the descriptor pool sizes.
        pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
        pool_info.pPoolSizes = pool_sizes.data();

        if (vkCreateDescriptorPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
            std::cerr << "Failed to create descriptor pool" << std::endl;
            std::terminate();
        }
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