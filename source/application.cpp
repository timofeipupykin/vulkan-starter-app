#include "application.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

#include <imgui.h>

#include "graphics.hpp"
#include "math.hpp"

namespace application {

namespace {

struct Vertex {
	float position[3];
	float color[3];
};

// Regular square pyramid: base 2x2 at y = -0.75, apex at y = 0.75.
// Origin is in the middle of the height, so the pyramid rotates around itself.
const Vertex pyramid_vertices[] = {
	{{-1.0f, -0.75f, -1.0f}, {1.0f, 0.0f, 0.0f}}, // 0: base, red
	{{ 1.0f, -0.75f, -1.0f}, {0.0f, 1.0f, 0.0f}}, // 1: base, green
	{{ 1.0f, -0.75f,  1.0f}, {0.0f, 0.0f, 1.0f}}, // 2: base, blue
	{{-1.0f, -0.75f,  1.0f}, {1.0f, 1.0f, 0.0f}}, // 3: base, yellow
	{{ 0.0f,  0.75f,  0.0f}, {1.0f, 1.0f, 1.0f}}, // 4: apex, white
};

// Triangles are counter-clockwise when looking at the face from outside
const uint32_t pyramid_indices[] = {
	0, 4, 1, // side -Z
	1, 4, 2, // side +X
	2, 4, 3, // side +Z
	3, 4, 0, // side -X
	0, 1, 2, // base
	0, 2, 3, // base
};

constexpr uint32_t pyramid_index_count = sizeof(pyramid_indices) / sizeof(pyramid_indices[0]);

// Must match "uniform Uniforms" block in shaders/pyramid.vert (std140 layout)
struct Uniforms {
	math::mat4 mvp;
};

static_assert(sizeof(Uniforms) == 64);

// Parameters controlled from the UI
float clear_color[3] = {0.1f, 0.1f, 0.12f};
float rotation_speed = 1.0f; // radians per second
float camera_distance = 4.0f;
float camera_tilt_deg = 23.0f;
float fov_deg = 60.0f;

float rotation_angle = 0.0f;
double previous_time = -1.0;

Uniforms uniforms;

graphics::Buffer vertex_buffer;
graphics::Buffer index_buffer;
graphics::Buffer uniform_buffer;

VkDescriptorSetLayout descriptor_set_layout;
VkDescriptorPool descriptor_pool;
VkDescriptorSet descriptor_set;

VkPipelineLayout pipeline_layout;
VkPipeline pipeline;

bool createDescriptorSet() {
	auto& context = graphics::internal::context;

	const VkDescriptorSetLayoutBinding binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	const VkDescriptorSetLayoutCreateInfo set_layout = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &binding,
	};

	if (vkCreateDescriptorSetLayout(context.device, &set_layout, nullptr,
	                                &descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan descriptor set layout\n";
		return false;
	}

	const VkDescriptorPoolSize pool_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
	};

	const VkDescriptorPoolCreateInfo pool = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	if (vkCreateDescriptorPool(context.device, &pool, nullptr, &descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan descriptor pool\n";
		return false;
	}

	const VkDescriptorSetAllocateInfo allocate = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_set_layout,
	};

	if (vkAllocateDescriptorSets(context.device, &allocate, &descriptor_set) != VK_SUCCESS) {
		std::cerr << "Failed to allocate Vulkan descriptor set\n";
		return false;
	}

	// Point binding 0 of the set to our uniform buffer
	const VkDescriptorBufferInfo buffer_info = {
		.buffer = uniform_buffer.buffer,
		.offset = 0,
		.range = sizeof(Uniforms),
	};

	const VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &buffer_info,
	};

	vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);

	return true;
}

bool createPipeline() {
	auto& context = graphics::internal::context;

	VkShaderModule vertex_shader = graphics::createShaderModule("shaders/pyramid.vert.spv");
	VkShaderModule fragment_shader = graphics::createShaderModule("shaders/pyramid.frag.spv");

	if (vertex_shader == VK_NULL_HANDLE || fragment_shader == VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, vertex_shader, nullptr);
		vkDestroyShaderModule(context.device, fragment_shader, nullptr);
		return false;
	}

	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	const VkVertexInputAttributeDescription vertex_attributes[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position),
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color),
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = sizeof(vertex_attributes) / sizeof(vertex_attributes[0]),
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	// Viewport and scissor are set dynamically every frame, because window can be resized
	const VkPipelineViewportStateCreateInfo viewport = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisample = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};

	const VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo color_blend = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
	};

	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	const VkPipelineDynamicStateCreateInfo dynamic = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
		.pDynamicStates = dynamic_states,
	};

	const VkPipelineLayoutCreateInfo layout = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,
	};

	if (vkCreatePipelineLayout(context.device, &layout, nullptr, &pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan pipeline layout\n";
		vkDestroyShaderModule(context.device, vertex_shader, nullptr);
		vkDestroyShaderModule(context.device, fragment_shader, nullptr);
		return false;
	}

	const VkGraphicsPipelineCreateInfo graphics_pipeline = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = sizeof(stages) / sizeof(stages[0]),
		.pStages = stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic,
		.layout = pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

	const VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
	                                                  &graphics_pipeline, nullptr, &pipeline);

	// Shader modules are only needed during pipeline creation
	vkDestroyShaderModule(context.device, vertex_shader, nullptr);
	vkDestroyShaderModule(context.device, fragment_shader, nullptr);

	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan graphics pipeline\n";
		return false;
	}

	return true;
}

} // namespace

bool initialize() {
	vertex_buffer = graphics::createBuffer(sizeof(pyramid_vertices),
	                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, pyramid_vertices);
	index_buffer = graphics::createBuffer(sizeof(pyramid_indices),
	                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, pyramid_indices);

	uniform_buffer = graphics::createBuffer(sizeof(Uniforms),
	                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);

	if (vertex_buffer.buffer == VK_NULL_HANDLE || index_buffer.buffer == VK_NULL_HANDLE ||
	    uniform_buffer.buffer == VK_NULL_HANDLE) {
		return false;
	}

	if (!createDescriptorSet()) {
		return false;
	}

	if (!createPipeline()) {
		return false;
	}

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyPipeline(context.device, pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);

	// Destroying the pool also frees descriptor_set
	vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);

	graphics::destroyBuffer(uniform_buffer);
	graphics::destroyBuffer(index_buffer);
	graphics::destroyBuffer(vertex_buffer);
}

void update(double time) {
	if (previous_time < 0.0) {
		previous_time = time;
	}

	const float delta_time = float(time - previous_time);
	previous_time = time;

	// Accumulate the angle, so changing the speed doesn't make the pyramid jump
	rotation_angle += delta_time * rotation_speed;

	ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lab 1");
	ImGui::ColorEdit3("Background", clear_color);
	ImGui::SliderFloat("Rotation speed", &rotation_speed, 0.0f, 3.0f);
	ImGui::SliderFloat("Distance", &camera_distance, 2.0f, 10.0f);
	ImGui::SliderFloat("Tilt", &camera_tilt_deg, -80.0f, 80.0f);
	ImGui::SliderFloat("FOV", &fov_deg, 20.0f, 120.0f);
	ImGui::End();

	const VkExtent2D extent = graphics::internal::context.swapchain_extent;
	const float aspect = float(extent.width) / float(extent.height);

	const math::mat4 model = math::rotateY(rotation_angle);
	const math::mat4 view = math::translate(0.0f, 0.0f, -camera_distance) *
	                        math::rotateX(math::radians(camera_tilt_deg));
	const math::mat4 projection = math::perspective(math::radians(fov_deg), aspect, 0.1f, 100.0f);

	uniforms.mvp = projection * view * model;
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;

	if (fd.command_buffer == VK_NULL_HANDLE) {
		return;
	}

	const VkCommandBuffer cb = fd.command_buffer;

	// Safe here: prepare() has already waited for the previous frame to finish on GPU
	graphics::uploadToBuffer(uniform_buffer, &uniforms, sizeof(uniforms));

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

	const VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(context.swapchain_extent.width),
		.height = float(context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	const VkRect2D scissor = { .offset = {0, 0}, .extent = context.swapchain_extent };

	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdSetViewport(cb, 0, 1, &viewport);
	vkCmdSetScissor(cb, 0, 1, &scissor);

	const VkDeviceSize vertex_offset = 0;
	vkCmdBindVertexBuffers(cb, 0, 1, &vertex_buffer.buffer, &vertex_offset);
	vkCmdBindIndexBuffer(cb, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
	                        0, 1, &descriptor_set, 0, nullptr);

	vkCmdDrawIndexed(cb, pyramid_index_count, 1, 0, 0, 0);

	vkCmdEndRenderPass(cb);

	vkEndCommandBuffer(cb);
}

} // namespace application
