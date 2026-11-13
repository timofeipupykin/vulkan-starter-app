#include "application.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include <imgui.h>

#include "graphics.hpp"
#include "math.hpp"

namespace application {

namespace {

struct Vertex {
	math::vec3 position;
	math::vec3 normal;
};

// Must match "uniform Uniforms" block in shaders/pyramid.vert and shaders/pyramid.frag (std140 layout).
// vec3 is aligned to 16 bytes but takes 12, so a float fits right after it; otherwise padding is needed.
struct Uniforms {
	math::mat4 model;            //   0
	math::mat4 view_projection;  //  64
	math::vec3 camera_position;  // 128
	float _pad0;                 // 140: next vec3 must start at 144
	math::vec3 ambient_color;    // 144
	float ambient_intensity;     // 156
	math::vec3 light_direction;  // 160: where the light goes, unit length
	float light_intensity;       // 172
	math::vec3 light_color;      // 176
	float _pad1;                 // 188
	math::vec3 albedo;           // 192
	float _pad2;                 // 204
	math::vec3 specular_color;   // 208
	float shininess;             // 220
};

static_assert(sizeof(Uniforms) == 224);
static_assert(offsetof(Uniforms, camera_position) == 128);
static_assert(offsetof(Uniforms, ambient_color) == 144);
static_assert(offsetof(Uniforms, light_direction) == 160);
static_assert(offsetof(Uniforms, light_color) == 176);
static_assert(offsetof(Uniforms, albedo) == 192);
static_assert(offsetof(Uniforms, specular_color) == 208);

struct Camera {
	math::vec3 position = {0.0f, 2.0f, 6.0f};
	float yaw_deg = 0.0f;     // rotation around Y: 0 looks along -Z, > 0 turns left
	float pitch_deg = -18.0f; // > 0 looks up, < 0 looks down
	float fov_deg = 60.0f;
};

// Parameters controlled from the UI
float clear_color[3] = {0.1f, 0.1f, 0.12f};
float rotation_speed = 0.5f; // radians per second

Camera camera;

math::vec3 ambient_color = {1.0f, 1.0f, 1.0f};
float ambient_intensity = 0.1f;

math::vec3 light_direction = {-0.4f, -1.0f, -0.3f}; // normalized in update()
math::vec3 light_color = {1.0f, 0.95f, 0.9f};
float light_intensity = 1.0f;

math::vec3 albedo = {0.8f, 0.3f, 0.2f};
math::vec3 specular_color = {0.5f, 0.5f, 0.5f};
float shininess = 64.0f;

float rotation_angle = 0.0f;
double previous_time = -1.0;

uint32_t pyramid_index_count = 0;

Uniforms uniforms;

graphics::Buffer vertex_buffer;
graphics::Buffer index_buffer;
graphics::Buffer uniform_buffer;

VkDescriptorSetLayout descriptor_set_layout;
VkDescriptorPool descriptor_pool;
VkDescriptorSet descriptor_set;

VkPipelineLayout pipeline_layout;
VkPipeline pipeline;

// Regular square pyramid: base 2x2 at y = -0.75, apex at y = 0.75.
// Origin is in the middle of the height, so the pyramid rotates around itself.
// Each face is a convex polygon of corner indices, counter-clockwise when looking from outside.
// Every face gets its own vertices with the face normal, so the faces are lit flat.
void buildPyramid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
	const math::vec3 corners[] = {
		{-1.0f, -0.75f, -1.0f}, // 0: base
		{ 1.0f, -0.75f, -1.0f}, // 1: base
		{ 1.0f, -0.75f,  1.0f}, // 2: base
		{-1.0f, -0.75f,  1.0f}, // 3: base
		{ 0.0f,  0.75f,  0.0f}, // 4: apex
	};

	const std::vector<uint32_t> faces[] = {
		{0, 4, 1},    // side -Z
		{1, 4, 2},    // side +X
		{2, 4, 3},    // side +Z
		{3, 4, 0},    // side -X
		{0, 1, 2, 3}, // base, looks down
	};

	for (const std::vector<uint32_t>& face : faces) {
		const math::vec3 a = corners[face[0]];
		const math::vec3 b = corners[face[1]];
		const math::vec3 c = corners[face[2]];

		// Counter-clockwise from outside, so the cross product points outwards
		const math::vec3 normal = math::normalize(math::cross(b - a, c - a));

		const uint32_t first = uint32_t(vertices.size());

		for (uint32_t corner : face) {
			vertices.push_back({corners[corner], normal});
		}

		// Triangle fan: (0, 1, 2), (0, 2, 3), ...
		for (uint32_t i = 1; i + 1 < face.size(); ++i) {
			indices.insert(indices.end(), {first, first + i, first + i + 1});
		}
	}
}

// Direction from the UI may be zero: fall back to "straight down" instead of NaN
math::vec3 safeDirection(math::vec3 direction) {
	if (math::length(direction) < 1e-4f) {
		return {0.0f, -1.0f, 0.0f};
	}
	return math::normalize(direction);
}

bool createDescriptorSet() {
	auto& context = graphics::internal::context;

	const VkDescriptorSetLayoutBinding binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		// Fragment shader reads camera, light and material from the same buffer
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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
			.offset = offsetof(Vertex, normal),
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
	// Vectors are needed only here: createBuffer copies the data into the buffer right away
	std::vector<Vertex> pyramid_vertices;
	std::vector<uint32_t> pyramid_indices;
	buildPyramid(pyramid_vertices, pyramid_indices);

	pyramid_index_count = uint32_t(pyramid_indices.size());

	vertex_buffer = graphics::createBuffer(pyramid_vertices.size() * sizeof(Vertex),
	                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                       pyramid_vertices.data());
	index_buffer = graphics::createBuffer(pyramid_indices.size() * sizeof(uint32_t),
	                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                                      pyramid_indices.data());

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

	ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lab 2");
	ImGui::ColorEdit3("Background", clear_color);
	ImGui::SliderFloat("Rotation speed", &rotation_speed, 0.0f, 3.0f);

	// Ctrl+Click allows typing a value out of the slider range, AlwaysClamp forbids that
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Position", &camera.position.x, 0.05f);
		ImGui::SliderFloat("Yaw", &camera.yaw_deg, -180.0f, 180.0f);
		ImGui::SliderFloat("Pitch", &camera.pitch_deg, -89.0f, 89.0f, "%.1f",
		                   ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat("FOV", &camera.fov_deg, 20.0f, 120.0f, "%.1f",
		                   ImGuiSliderFlags_AlwaysClamp);
	}

	// Sections have widgets with the same labels, so each section gets its own ID
	if (ImGui::CollapsingHeader("Ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("ambient");
		ImGui::ColorEdit3("Color", &ambient_color.x);
		ImGui::SliderFloat("Intensity", &ambient_intensity, 0.0f, 1.0f);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("light");
		ImGui::DragFloat3("Direction", &light_direction.x, 0.02f);
		ImGui::ColorEdit3("Color", &light_color.x);
		ImGui::SliderFloat("Intensity", &light_intensity, 0.0f, 5.0f);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit3("Albedo", &albedo.x);
		ImGui::ColorEdit3("Specular", &specular_color.x);
		// pow(0, 0) is undefined, so shininess is at least 1
		ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f, "%.1f",
		                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
	}

	ImGui::End();

	const VkExtent2D extent = graphics::internal::context.swapchain_extent;
	const float aspect = float(extent.width) / float(extent.height);

	// Camera in the world is translate(p) * rotateY(yaw) * rotateX(pitch),
	// view matrix is its inverse: inverse transforms in reverse order
	const math::mat4 view = math::rotateX(math::radians(-camera.pitch_deg)) *
	                        math::rotateY(math::radians(-camera.yaw_deg)) *
	                        math::translate(-camera.position);
	const math::mat4 projection = math::perspective(math::radians(camera.fov_deg), aspect,
	                                                0.1f, 100.0f);

	uniforms.model = math::rotateY(rotation_angle);
	uniforms.view_projection = projection * view;
	uniforms.camera_position = camera.position;
	uniforms.ambient_color = ambient_color;
	uniforms.ambient_intensity = ambient_intensity;
	uniforms.light_direction = safeDirection(light_direction);
	uniforms.light_intensity = light_intensity;
	uniforms.light_color = light_color;
	uniforms.albedo = albedo;
	uniforms.specular_color = specular_color;
	uniforms.shininess = shininess;
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
