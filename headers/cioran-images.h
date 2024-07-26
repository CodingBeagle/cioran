#ifndef CIORAN_IMAGES_H
#define CIORAN_IMAGES_H

#include <vulkan/vulkan.h>

namespace cioran {
    void copy_image_to_image(VkCommandBuffer command_buffer, VkImage src_image, VkImage dst_image, VkExtent2D srcSize, VkExtent2D dstSize);
}

#endif // CIORAN_IMAGES_H