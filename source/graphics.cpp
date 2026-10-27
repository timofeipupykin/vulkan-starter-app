#include "graphics.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "graphics_internal.hpp"

namespace graphics {

VkShaderModule createShaderModule(const char* path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Failed to open shader file " << path << '\n';
		return VK_NULL_HANDLE;
	}

	// SPIR-V is a sequence of 32-bit words
	const size_t size = size_t(file.tellg());
	std::vector<uint32_t> code(size / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(code.data()), std::streamsize(size));

	const VkShaderModuleCreateInfo shader_module = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = code.data(),
	};

	VkShaderModule result = VK_NULL_HANDLE;

	if (vkCreateShaderModule(internal::context.device, &shader_module, nullptr,
	                         &result) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan shader module from " << path << '\n';
		return VK_NULL_HANDLE;
	}

	return result;
}

Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data) {
	const VkBufferCreateInfo buffer = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	Buffer result;

	if (vmaCreateBuffer(internal::context.allocator, &buffer, &allocation,
	                    &result.buffer, &result.allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to allocate and create Vulkan buffer\n";
		return {};
	}

	if (data != nullptr) {
		uploadToBuffer(result, data, size);
	}

	return result;
}

void uploadToBuffer(const Buffer& buffer, const void* data, VkDeviceSize size) {
	// Copies data and flushes CPU caches if memory is not host-coherent
	vmaCopyMemoryToAllocation(internal::context.allocator, data, buffer.allocation, 0, size);
}

void destroyBuffer(Buffer& buffer) {
	vmaDestroyBuffer(internal::context.allocator, buffer.buffer, buffer.allocation);
	buffer = {};
}

} // namespace graphics
