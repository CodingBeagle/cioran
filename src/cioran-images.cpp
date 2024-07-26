#include "cioran-images.h"

namespace cioran {
    void copy_image_to_image(VkCommandBuffer command_buffer, VkImage src_image, VkImage dst_image, VkExtent2D srcSize, VkExtent2D dstSize)
    {
        // Blit operation refers to the process of transferring a block of pixels from one region
        // of memory to another.
        // VkImageBlit2 is used to describe such a blit operation.
        VkImageBlit2 blit_region {};
        blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

        // srcOffsets consists of two VkOffset3D structs indicating coordinates in 3D space.
        // The one in srcOffsets[0] specifies the top-left corner of the source region.
        // The one in srcOffsets[1] specifies the bottom-right corner of the source region.
        blit_region.srcOffsets[1].x = srcSize.width;
        blit_region.srcOffsets[1].y = srcSize.height;
        blit_region.srcOffsets[1].z = 1;

        // Same as for srcOffsets, just for the destination rectangle instead.
        blit_region.dstOffsets[1].x = dstSize.width;
        blit_region.dstOffsets[1].y = dstSize.height;
        blit_region.dstOffsets[1].z = 1;

        // Specify the part of the source image (the subresource) that will be used in the blit operation.
        // In this case, it is the color part of the image, of the highest mip level.
        blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.srcSubresource.baseArrayLayer = 0;
        blit_region.srcSubresource.layerCount = 1;
        blit_region.srcSubresource.mipLevel = 0;

        // Same as for srcSubresource, just for the destination image instead.
        blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.dstSubresource.baseArrayLayer = 0;
        blit_region.dstSubresource.layerCount = 1;
        blit_region.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blit_info {};
        blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blit_info.srcImage = src_image;
        blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blit_info.dstImage = dst_image;
        blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blit_info.filter = VK_FILTER_LINEAR;
        blit_info.regionCount = 1;
        blit_info.pRegions = &blit_region;

        vkCmdBlitImage2(command_buffer, &blit_info);
    }
}

