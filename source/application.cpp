#include "application.hpp"

#include <imgui.h>

namespace application {

namespace {

float clear_color[3] = {0.1f, 0.1f, 0.12f};

} // namespace

bool initialize() {
	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
}

void update([[maybe_unused]] double time) {
	ImGui::Begin("Lab 1");
	ImGui::ColorEdit3("Background", clear_color);
	ImGui::End();
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;

	if (fd.command_buffer == VK_NULL_HANDLE) {
		return;
	}

	const VkCommandBuffer cb = fd.command_buffer;

	vkResetCommandBuffer(cb, 0);

	const VkCommandBufferBeginInfo begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(cb, &begin);

	VkClearValue clear_values[2];
	clear_values[0].color = {{clear_color[0], clear_color[1], clear_color[2], 1.0f}};
	clear_values[1].depthStencil = {1.0f, 0};

	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .offset = {0, 0}, .extent = context.swapchain_extent },
		.clearValueCount = sizeof(clear_values) / sizeof(clear_values[0]),
		.pClearValues = clear_values,
	};

	vkCmdBeginRenderPass(cb, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(cb);

	vkEndCommandBuffer(cb);
}

} // namespace application
