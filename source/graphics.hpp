#pragma once

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

namespace graphics {

struct Buffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

// Reads SPIR-V file (path is relative to the working directory)
VkShaderModule createShaderModule(const char* path);

// Creates host-visible buffer, optionally filling it with data
Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data);
void uploadToBuffer(const Buffer& buffer, const void* data, VkDeviceSize size);
void destroyBuffer(Buffer& buffer);

} // namespace graphics
