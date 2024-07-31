# Issues I've experienced

When attempting to call *vkUpdateDescriptorSets*, I got an *access violation reading location ...*

Turned out that when I created the *VkDescriptorSetLayoutCreateInfo* like so:

VkDescriptorSetLayoutCreateInfo info {};
info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
info.pNext = pNext;
info.pBindings = bindings.data();
info.bindingCount = (uint32_t)bindings.size();
info.flags = flags;


ate a binding that didn't exist.

Validation should prolly have warned about this...

Turns out validation in Vulkan is far from perfect, and there are many things they don't check for yet...