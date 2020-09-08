/*
 *
 * Copyright (c) 2020 gyabo <gyaboyan@gmail.com>
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */
#include "ODEN.h"

#include <stdio.h>
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#include <map>
#include <vector>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(lib, "vulkan-1.lib")

//#define ODEN_VK_DEBUG_MODE

#ifdef ODEN_VK_DEBUG_MODE
#define LOG_MAIN(...) printf("MAIN : " __VA_ARGS__)
#else
#define LOG_MAIN(...) {}
#endif //ODEN_VK_DEBUG_MODE

#define LOG_INFO(...) printf("INFO : " __FUNCTION__ ":" __VA_ARGS__)
#define LOG_ERR(...) printf("ERR : " __FUNCTION__ ":" __VA_ARGS__)

using namespace oden;

static void
fork_process_wait(const char *command)
{
	PROCESS_INFORMATION pi;

	STARTUPINFO si = {};
	si.cb = sizeof(si);
	if (CreateProcess(NULL, (LPTSTR)command, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
		while (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
			Sleep(0);
		}
	}
}

static void
compile_glsl2spirv(
	std::string shaderfile,
	std::string type,
	std::vector<unsigned char> &vdata)
{
	//glslangValidator bloom.glsl -o a.spv -S frag -V --D _PS_
	//glslangValidator bloom.glsl -o a.spv -S vert -V --D _VS_
	auto tempfilename = shaderfile + type + std::string("temp.spv");
	auto basecmd = std::string("glslangValidator -V -S ");
	auto soption = std::string("null");
	/*
		.vert   for a vertex shader
		.tesc   for a tessellation control shader
		.tese   for a tessellation evaluation shader
		.geom   for a geometry shader
		.frag   for a fragment shader
		.comp   for a compute shader
		.mesh   for a mesh shader
		.task   for a task shader
		.rgen    for a ray generation shader
		.rint    for a ray intersection shader
		.rahit   for a ray any hit shader
		.rchit   for a ray closest hit shader
		.rmiss   for a ray miss shader
		.rcall   for a ray callable shader
	*/

	if (type == "_VS_")
		soption = "vert";
	if (type == "_GS_")
		soption = "geom";
	if (type == "_PS_")
		soption = "frag";
	if (type == "_CS_")
		soption = "comp";

	basecmd += soption;
	basecmd += " --D " + type + " " + shaderfile + std::string(" -o ") + tempfilename;
	LOG_MAIN("basecmd : %s\n", basecmd.c_str());

	fork_process_wait((char *)basecmd.c_str());
	{
		FILE *fp = fopen(tempfilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			vdata.resize(ftell(fp));
			fseek(fp, 0, SEEK_SET);
			if (vdata.size() > 0)
				fread(vdata.data(), 1, vdata.size(), fp);
			fclose(fp);
		}
	}
	DeleteFile(tempfilename.c_str());
}

static VKAPI_ATTR VkBool32
VKAPI_CALL debug_callback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT,
	uint64_t object,
	size_t location,
	int32_t messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	printf("vkdbg: ");
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		printf("ERROR : ");
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		printf("WARNING : ");
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		printf("PERFORMANCE : ");
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		printf("INFO : ");
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		printf("DEBUG : ");
	printf("%s", pMessage);
	printf("\n");
	return VK_FALSE;
}

inline void
bind_debug_fn(
	VkInstance instance,
	VkDebugReportCallbackCreateInfoEXT ext)
{
	VkDebugReportCallbackEXT callback;
	PFN_vkCreateDebugReportCallbackEXT cb;
	cb = PFN_vkCreateDebugReportCallbackEXT(
			vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));

	if (cb)
		cb(instance, &ext, nullptr, &callback);
	else
		LOG_MAIN("PFN_vkCreateDebugReportCallbackEXT IS NULL\n");
}

[[ nodiscard ]] static VkImage
create_image(
	VkDevice device,
	uint32_t width, uint32_t height, VkFormat format,
	VkImageUsageFlags usageFlags, int maxmips)
{
	VkImage ret = VK_NULL_HANDLE;
	VkImageCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = NULL;
	info.flags = 0;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = 1;
	info.mipLevels = maxmips;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = NULL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkCreateImage(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkImageView
create_image_view(
	VkDevice device,
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspectMask, int miplevel = 0)
{
	VkImageView ret = VK_NULL_HANDLE;
	VkImageViewCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = NULL;
	info.flags = 0;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.components.r = VK_COMPONENT_SWIZZLE_R;
	info.components.g = VK_COMPONENT_SWIZZLE_G;
	info.components.b = VK_COMPONENT_SWIZZLE_B;
	info.components.a = VK_COMPONENT_SWIZZLE_A;
	info.subresourceRange.aspectMask = aspectMask;
	info.subresourceRange.baseMipLevel = miplevel;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	vkCreateImageView(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkBuffer
create_buffer(VkDevice device, VkDeviceSize size)
{
	VkBuffer ret = VK_NULL_HANDLE;
	VkBufferCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size  = size;
	info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]] static VkImageMemoryBarrier
get_barrier(VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout old_image_layout,
	VkImageLayout new_image_layout,
	uint32_t baseMipLevel = 0,
	uint32_t levelCount = 1,
	uint32_t baseArrayLayer = 0,
	uint32_t layerCount = 1)
{
	VkImageMemoryBarrier ret = {};
	ret.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ret.pNext = NULL;
	ret.srcAccessMask = 0;
	ret.dstAccessMask = 0;
	ret.oldLayout = old_image_layout;
	ret.newLayout = new_image_layout;
	ret.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ret.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ret.image = image;
	ret.subresourceRange = { aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount };

	return (ret);
}

[[ nodiscard ]] static VkRenderPass
create_renderpass(
	VkDevice device,
	uint32_t color_num,
	bool is_presentable,
	VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM,
	VkFormat depth_format = VK_FORMAT_D32_SFLOAT)
{
	VkRenderPass ret = VK_NULL_HANDLE;

	auto initialLayoutColor = VK_IMAGE_LAYOUT_GENERAL;
	auto finalLayoutColor = VK_IMAGE_LAYOUT_GENERAL;
	auto initialLayoutDepth = VK_IMAGE_LAYOUT_GENERAL;
	auto finalLayoutDepth = VK_IMAGE_LAYOUT_GENERAL;
	auto loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	// todo VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV
	if (is_presentable) {
		initialLayoutColor = VK_IMAGE_LAYOUT_UNDEFINED;
		finalLayoutColor = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		initialLayoutDepth = VK_IMAGE_LAYOUT_UNDEFINED;
		finalLayoutDepth = VK_IMAGE_LAYOUT_GENERAL;
		loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	int attachment_index = 0;
	std::vector<VkAttachmentDescription> vattachments;
	std::vector<VkAttachmentReference> vattachment_refs;
	std::vector<VkSubpassDependency> vsubpassdepends;
	VkAttachmentDescription color_attachment = {};

	color_attachment.flags = 0;
	color_attachment.format = color_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	color_attachment.loadOp = loadOp;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = initialLayoutColor;
	color_attachment.finalLayout = finalLayoutColor;

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.flags = 0;
	depth_attachment.format = depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = loadOp;

	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = initialLayoutDepth;
	depth_attachment.finalLayout = finalLayoutDepth;

	VkAttachmentReference color_reference = {};
	color_reference.attachment = 0;
	color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_reference = {};
	depth_reference.attachment = 1;
	depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	for (int i = 0 ; i < color_num; i++) {
		auto ref = color_reference;
		ref.attachment = attachment_index;
		vattachments.push_back(color_attachment);
		vattachment_refs.push_back(ref);
		attachment_index++;
	}
	vattachments.push_back(depth_attachment);
	depth_reference.attachment = attachment_index;

	VkSubpassDescription subpass = {};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = (uint32_t)vattachment_refs.size();
	subpass.pColorAttachments = vattachment_refs.data();
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depth_reference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rp_info.pNext = NULL;
	rp_info.flags = 0;
	rp_info.attachmentCount = (uint32_t)vattachments.size();
	rp_info.pAttachments = vattachments.data();
	rp_info.subpassCount = 1;
	rp_info.pSubpasses = &subpass;
	auto err = vkCreateRenderPass(device, &rp_info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkFramebuffer
create_framebuffer(
	VkDevice device,
	VkRenderPass renderpass,
	std::vector<VkImageView> vimageview,
	uint32_t width,
	uint32_t height,
	VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM,
	VkFormat depth_format = VK_FORMAT_D32_SFLOAT)
{
	VkFramebuffer fb = nullptr;
	VkFramebufferCreateInfo fb_info = {};

	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = NULL;
	fb_info.renderPass = renderpass;
	fb_info.attachmentCount = (uint32_t)vimageview.size();
	fb_info.pAttachments = vimageview.data();
	fb_info.width = width;
	fb_info.height = height;
	fb_info.layers = 1;
	auto err = vkCreateFramebuffer(device, &fb_info, NULL, &fb);

	return (fb);
}

[[ nodiscard ]] static VkDescriptorSet
create_descriptor_set(
	VkDevice device,
	VkDescriptorPool descriptor_pool,
	VkDescriptorSetLayout descriptor_layout)
{
	VkDescriptorSet ret = VK_NULL_HANDLE;
	VkDescriptorSetAllocateInfo alloc_info = {};

	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	alloc_info.pNext = NULL,
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1,
	alloc_info.pSetLayouts = &descriptor_layout;
	auto err = vkAllocateDescriptorSets(device, &alloc_info, &ret);

	return (ret);
}

[[ nodiscard ]] static VkFence
create_fence(VkDevice device)
{
	VkFenceCreateInfo fence_ci = {};
	VkFence ret = VK_NULL_HANDLE;

	fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_ci.pNext = NULL;
	fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	auto err = vkCreateFence(device, &fence_ci, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkSampler
create_sampler(VkDevice device, bool isfilterd)
{
	VkSampler ret = VK_NULL_HANDLE;
	VkSamplerCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = NULL;
	info.magFilter = VK_FILTER_NEAREST;
	info.minFilter = VK_FILTER_NEAREST;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.mipLodBias = 0.0f;
	info.anisotropyEnable = VK_FALSE;
	info.maxAnisotropy = 0;
	info.compareOp = VK_COMPARE_OP_NEVER;
	info.minLod = 0.0f;
	info.maxLod = 3.402823466e+38f;
	info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	info.unnormalizedCoordinates = VK_FALSE;
	if (isfilterd) {
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}

	auto err = vkCreateSampler(device, &info, NULL, &ret);
	return (ret);
}

[[ nodiscard ]] static VkCommandPool
create_command_pool(
	VkDevice device, uint32_t index_qfi)
{
	VkCommandPoolCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = NULL;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = index_qfi;
	VkCommandPool ret = nullptr;
	auto err = vkCreateCommandPool(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkCommandBuffer
create_command_buffer(
	VkDevice device, VkCommandPool cmd_pool)
{
	VkCommandBufferAllocateInfo cballoc_info = {};

	cballoc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cballoc_info.pNext = nullptr;
	cballoc_info.commandPool = cmd_pool;
	cballoc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cballoc_info.commandBufferCount = 1;
	VkCommandBuffer ret = nullptr;
	auto err = vkAllocateCommandBuffers(device, &cballoc_info, &ret);

	return (ret);
}

[[ nodiscard ]] static VkDescriptorPool
create_descriptor_pool(
	VkDevice device, uint32_t heapcount)
{
	VkDescriptorPool ret = nullptr;
	VkDescriptorPoolCreateInfo info = {};
	std::vector<VkDescriptorPoolSize> vpoolsizes;

	heapcount = 0xFFFF;

	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, heapcount});

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.maxSets = 0xFFFF; //todo
	info.poolSizeCount = (uint32_t)vpoolsizes.size();
	info.pPoolSizes = vpoolsizes.data();
	auto err = vkCreateDescriptorPool(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]] static VkDescriptorSetLayout
create_descriptor_set_layout(
	VkDevice device,
	std::vector<VkDescriptorSetLayoutBinding> & vdesc_setlayout_binding)
{
	VkDescriptorSetLayout ret = nullptr;
	VkDescriptorSetLayoutCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pBindings = vdesc_setlayout_binding.data();
	info.bindingCount = (uint32_t)vdesc_setlayout_binding.size();
	auto err = vkCreateDescriptorSetLayout(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]] static VkPipelineLayout
create_pipeline_layout(
	VkDevice device,
	VkDescriptorSetLayout descriptor_layout)
{
	VkPipelineLayout ret = nullptr;
	VkPipelineLayoutCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = NULL;
	info.setLayoutCount = 1;
	info.pSetLayouts = &descriptor_layout;
	auto err = vkCreatePipelineLayout(device, &info, NULL, &ret);
	return (ret);
}

[[ nodiscard ]] static VkShaderModule
create_shader_module(
	VkDevice device, void *data, size_t size)
{
	VkShaderModule ret = nullptr;
	VkShaderModuleCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.pCode = (uint32_t *)data;
	info.codeSize = size;
	vkCreateShaderModule(device, &info, nullptr, &ret);

	return (ret);
}


[[ nodiscard ]] static VkPipeline
create_cpipeline_from_file(
	VkDevice device,
	const char *filename,
	VkPipelineLayout pipeline_layout)
{
	VkPipeline ret = nullptr;
	VkComputePipelineCreateInfo info = {};
	std::vector<uint8_t> cs;
	std::vector<VkShaderModule> vshadermodules;
	auto fname = std::string(filename);

	compile_glsl2spirv((fname + ".glsl").c_str(), "_CS_", cs);
	if (cs.empty())
		return VK_NULL_HANDLE;

	auto module = create_shader_module(device, cs.data(), cs.size());
	vshadermodules.push_back(module);
	info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage.pName = "main"; //glsl spec
	info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	info.stage.module = module;
	LOG_MAIN("INFO : Enable CS : module=%p\n", module);

	info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.layout = pipeline_layout;
	LOG_MAIN("%s : START vkCreateComputePipelines\n", __func__);
	vkCreateComputePipelines(device, nullptr, 1, &info, nullptr, &ret);
	LOG_MAIN("%s : END vkCreateComputePipelines\n", __func__);

	for (auto & modules : vshadermodules)
		if (modules)
			vkDestroyShaderModule(device, modules, nullptr);

	return (ret);
}

[[ nodiscard ]] static VkPipeline
create_gpipeline_from_file(
	VkDevice device,
	const char *filename,
	VkPipelineLayout pipeline_layout,
	VkRenderPass renderpass)
{
	VkPipeline ret = nullptr;
	VkPipelineCacheCreateInfo pipelineCache = {};
	VkPipelineVertexInputStateCreateInfo vi = {};
	VkPipelineInputAssemblyStateCreateInfo ia = {};
	VkPipelineRasterizationStateCreateInfo rs = {};
	VkPipelineColorBlendStateCreateInfo cb = {};
	VkPipelineDepthStencilStateCreateInfo ds = {};
	VkPipelineViewportStateCreateInfo vp = {};
	VkPipelineMultisampleStateCreateInfo ms = {};
	VkPipelineDynamicStateCreateInfo dynamic_state = {};

	//setup vp, sc
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	std::vector<VkDynamicState> vdynamic_state_enables;
	vdynamic_state_enables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	vdynamic_state_enables.push_back(VK_DYNAMIC_STATE_SCISSOR);

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pDynamicStates = vdynamic_state_enables.data();
	dynamic_state.dynamicStateCount = vdynamic_state_enables.size();

	//SETUP RS
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;//VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.depthClampEnable = VK_FALSE;
	rs.rasterizerDiscardEnable = VK_FALSE;
	rs.depthBiasEnable = VK_FALSE;
	rs.lineWidth = 1.0f;

	//SETUP CBA todo
	VkPipelineColorBlendAttachmentState att_state[1];
	memset(att_state, 0, sizeof(att_state));
	att_state[0].colorWriteMask = 0xf;
	att_state[0].blendEnable = VK_FALSE;
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = att_state;

	//SETUP DS
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.back.failOp = VK_STENCIL_OP_KEEP;
	ds.back.passOp = VK_STENCIL_OP_KEEP;
	ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
	ds.stencilTestEnable = VK_FALSE;
	ds.front = ds.back;

	//SETUP MS
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.pSampleMask = NULL;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//SETUP SM
	std::vector<VkPipelineShaderStageCreateInfo> vsstageinfo;
	std::vector<VkShaderModule> vshadermodules;

	LOG_INFO("vkCreateGraphicsPipelines filename=%s\n", filename);
	std::vector<uint8_t> vs;
	std::vector<uint8_t> gs;
	std::vector<uint8_t> ps; //todo fs
	auto fname = std::string(filename);
	compile_glsl2spirv((fname + ".glsl").c_str(), "_VS_", vs);
	compile_glsl2spirv((fname + ".glsl").c_str(), "_GS_", gs);
	compile_glsl2spirv((fname + ".glsl").c_str(), "_PS_", ps);

	VkPipelineShaderStageCreateInfo sstage = {};
	sstage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sstage.pName = "main"; //glsl spec

	if (!vs.empty()) {
		auto module = create_shader_module(device, vs.data(), vs.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		sstage.module = module;
		LOG_MAIN("INFO : Enable VS : module=%p\n", module);
		vsstageinfo.push_back(sstage);
	}
	if (!gs.empty()) {
		auto module = create_shader_module(device, gs.data(), gs.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		sstage.module = module;
		LOG_MAIN("INFO : Enable GS : module=%p\n", module);
		vsstageinfo.push_back(sstage);
	}
	if (!ps.empty()) {
		auto module = create_shader_module(device, ps.data(), ps.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		sstage.module = module;
		LOG_MAIN("INFO : Enable PS : module=%p\n", module);
		vsstageinfo.push_back(sstage);
	}

	//SETUP IA
	uint32_t stride_size = 0;
	std::vector<VkVertexInputAttributeDescription> vvertex_input_attr_descs;
	vvertex_input_attr_descs.push_back({0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;
	vvertex_input_attr_descs.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 3;
	vvertex_input_attr_descs.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 2;

	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkVertexInputBindingDescription vi_ibdesc = {};
	vi_ibdesc.binding = 0;
	vi_ibdesc.stride = stride_size;
	vi_ibdesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vi_ibdesc;
	vi.vertexAttributeDescriptionCount = vvertex_input_attr_descs.size();
	vi.pVertexAttributeDescriptions = vvertex_input_attr_descs.data();

	//Create Pipeline
	if (!vsstageinfo.empty()) {
		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.layout = pipeline_layout;
		pipeline_info.pVertexInputState = &vi;
		pipeline_info.pInputAssemblyState = &ia;
		pipeline_info.pRasterizationState = &rs;
		pipeline_info.pColorBlendState = &cb;
		pipeline_info.pMultisampleState = &ms;
		pipeline_info.pViewportState = &vp;
		pipeline_info.pDepthStencilState = &ds;
		pipeline_info.stageCount = vsstageinfo.size();
		pipeline_info.pStages = vsstageinfo.data();
		pipeline_info.pDynamicState = &dynamic_state;
		pipeline_info.renderPass = renderpass;
		auto pipeline_result = vkCreateGraphicsPipelines(
				device, nullptr, 1, &pipeline_info, NULL, &ret);
		LOG_INFO("vkCreateGraphicsPipelines Done filename=%s, pipeline=%p\n", filename, ret);
	}

	for (auto & module : vshadermodules)
		if (module)
			vkDestroyShaderModule(device, module, nullptr);
	return (ret);
}

static void
update_descriptor_sets(
	VkDevice device,
	VkDescriptorSet descriptor_sets,
	const void *pinfo,
	uint32_t binding,
	VkDescriptorType type)
{
	VkWriteDescriptorSet dw_sets = {};

	dw_sets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw_sets.pNext = NULL;
	dw_sets.descriptorType = type;
	dw_sets.descriptorCount = 1;
	dw_sets.dstSet = descriptor_sets;
	dw_sets.dstBinding = binding;
	dw_sets.dstArrayElement = 0;
	if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		dw_sets.pImageInfo = (const VkDescriptorImageInfo *)pinfo;
	if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		dw_sets.pImageInfo = (const VkDescriptorImageInfo *)pinfo;
	if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		dw_sets.pBufferInfo = (const VkDescriptorBufferInfo *)pinfo;
	vkUpdateDescriptorSets(device, 1, &dw_sets, 0, NULL);
}

void
oden::oden_present_graphics(
	const char * appname, std::vector<cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t count, uint32_t heapcount, uint32_t slotmax)
{
	HWND hwnd = (HWND) handle;

	enum {
		RDT_SLOT_SRV = 0,
		RDT_SLOT_CBV,
		RDT_SLOT_UAV,
		RDT_SLOT_MAX,
	};

	struct DeviceBuffer {
		uint64_t value = 0;
		VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		VkFramebuffer framebuffer = VK_NULL_HANDLE;

		std::vector<VkBuffer> vscratch_buffers;
		std::vector<VkDeviceMemory> vscratch_devmems;
	};

	static VkInstance inst = VK_NULL_HANDLE;
	static VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	static VkDevice device = VK_NULL_HANDLE;
	static VkQueue graphics_queue = VK_NULL_HANDLE;
	static VkSurfaceKHR surface = VK_NULL_HANDLE;
	static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	static VkCommandPool cmd_pool = VK_NULL_HANDLE;
	static VkSampler sampler_nearest = VK_NULL_HANDLE;
	static VkSampler sampler_linear = VK_NULL_HANDLE;
	static VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	static VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
	static VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	static VkPhysicalDeviceMemoryProperties devicememoryprop = {};

	static std::map<std::string, VkRenderPass> mrenderpasses;
	static std::map<std::string, VkFramebuffer> mframebuffers;
	static std::map<std::string, VkImage> mimages;
	static std::map<std::string, VkImageView> mimageviews;
	static std::map<std::string, VkClearValue> mclearvalues;
	static std::map<std::string, VkBuffer> mbuffers;
	static std::map<std::string, VkMemoryRequirements> mmemreqs;
	static std::map<std::string, VkDeviceMemory> mdevmem;

	static std::map<std::string, uint64_t> mdescriptor_set_offset;
	static std::map<std::string, VkPipeline> mpipelines;
	static std::map<std::string, VkPipelineBindPoint> mpipeline_bindpoints;

	static uint32_t backbuffer_index = 0;
	static uint64_t frame_count = 0;

	static std::vector<DeviceBuffer> devicebuffer;
	static std::vector<VkDescriptorSet> vdescriptor_sets;

	struct selected_handle {
		std::string renderpass_name;
		VkRenderPassBeginInfo info;
		VkRenderPass renderpass;
		VkRenderPass renderpass_commited;

		VkDescriptorSet descriptor_sets;
	};
	selected_handle rec = {};

	auto alloc_descriptor_sets = [&]() {
		static uint64_t index = 0;
		auto ret = vdescriptor_sets[index++ % vdescriptor_sets.size()];
		LOG_MAIN("%s : alloc_descriptor_sets handle=%p\n", __func__, ret);
		return ret;
	};

	auto alloc_devmem = [&](auto name, VkDeviceSize size, VkMemoryPropertyFlags flags, bool is_entry = true) {
		for (auto & x : mdevmem)
			LOG_MAIN("DEBUG alloc_devmem : addr=%p, name=%s\n", x.second, x.first.c_str());

		VkDeviceMemory devmem = mdevmem[name];
		if (devmem != nullptr) {
			LOG_MAIN("%s : already allocated name=%s\n", __func__, name.c_str());
			return devmem;
		}

		VkMemoryAllocateInfo ma_info = {};
		ma_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ma_info.pNext = nullptr;
		ma_info.allocationSize = size;
		ma_info.memoryTypeIndex = 0;

		for (uint32_t i = 0; i < devicememoryprop.memoryTypeCount; i++) {
			if ((devicememoryprop.memoryTypes[i].propertyFlags & flags) == flags) {
				ma_info.memoryTypeIndex = i;
				break;
			}
		}
		vkAllocateMemory(device, &ma_info, nullptr, &devmem);
		if (is_entry) {
			LOG_MAIN("%s : allocated name=%s\n", __func__, name.c_str());
			if (devmem)
				mdevmem[name] = devmem;
			else
				LOG_ERR("Can't alloc name=%s\n", name.c_str());
		}
		return devmem;
	};

	if (inst == nullptr) {
		uint32_t inst_ext_cnt = 0;
		uint32_t gpu_count = 0;
		uint32_t device_extension_count = 0;
		uint32_t queue_family_count = 0;
		uint32_t graphics_queue_family_index = UINT32_MAX;
		uint32_t presentQueueFamilyIndex = UINT32_MAX;

		std::vector<const char *> vinstance_ext_names;
		std::vector<const char *> ext_names;
		VkDebugReportCallbackCreateInfoEXT drcc_info = {};
		VkInstanceCreateInfo inst_info = {};
		VkPhysicalDeviceProperties gpu_props = {};
		VkPhysicalDeviceFeatures physDevFeatures = {};

		//Is supported vk?
		vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_cnt, NULL);
		std::vector<VkExtensionProperties> vinstance_ext(inst_ext_cnt);
		vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_cnt, vinstance_ext.data());
		for (auto x : vinstance_ext) {
			auto name = std::string(x.extensionName);
			if (name == VK_KHR_SURFACE_EXTENSION_NAME)
				vinstance_ext_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
			if (name == VK_KHR_WIN32_SURFACE_EXTENSION_NAME)
				vinstance_ext_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
			LOG_MAIN("vkEnumerateInstanceExtensionProperties : name=%s\n", name.c_str());
		}

		drcc_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		drcc_info.flags = 0;
		drcc_info.flags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
		drcc_info.flags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
		drcc_info.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		drcc_info.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
		drcc_info.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		drcc_info.pfnCallback = &debug_callback;

		//Create vk instances
		VkApplicationInfo vkapp = {};
		vkapp.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		vkapp.pNext = &drcc_info;
		vkapp.pApplicationName = appname;
		vkapp.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		vkapp.pEngineName = appname;
		vkapp.engineVersion = 0;
		vkapp.apiVersion = VK_API_VERSION_1_0;

		//DEBUG
		static const char *debuglayers[] = {
			//"VK_LAYER_LUNARG_standard_validation",

			//Todo avoid validation.
			"VK_LAYER_KHRONOS_validation",
		};
		vinstance_ext_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		//create instance
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = NULL;
		inst_info.pApplicationInfo = &vkapp;
		inst_info.enabledLayerCount = _countof(debuglayers);
		inst_info.ppEnabledLayerNames = debuglayers;
		inst_info.enabledExtensionCount = (uint32_t)vinstance_ext_names.size();
		inst_info.ppEnabledExtensionNames = (const char *const *)vinstance_ext_names.data();
		auto err = vkCreateInstance(&inst_info, NULL, &inst);

		bind_debug_fn(inst, drcc_info);

		//Enumaration GPU's
		err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
		LOG_MAIN("gpu_count=%d\n", gpu_count);
		if (gpu_count < 1) {
			LOG_ERR("-------------------------------------------------------\n");
			LOG_ERR(" Your Vulkan rendering system are belong to died.\n");
			LOG_ERR("-------------------------------------------------------\n");
			exit(1);
		}
		err = vkEnumeratePhysicalDevices(inst, &gpu_count, &gpudev);
		err = vkEnumerateDeviceExtensionProperties(gpudev, NULL, &device_extension_count, NULL);
		std::vector<VkExtensionProperties> vdevice_extensions(device_extension_count);
		err = vkEnumerateDeviceExtensionProperties(gpudev, NULL, &device_extension_count, vdevice_extensions.data());
		LOG_MAIN("vkEnumerateDeviceExtensionProperties : device_extension_count = %d, VK_KHR_SWAPCHAIN_EXTENSION_NAME=%s\n",
			vdevice_extensions.size(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		for (auto x : vdevice_extensions) {
			auto name = std::string(x.extensionName);
			if (name == VK_KHR_SWAPCHAIN_EXTENSION_NAME)
				ext_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			LOG_MAIN("vkEnumerateDeviceExtensionProperties : extensionName=%s\n", x.extensionName);
		}

		//Enumeration Queue attributes.
		vkGetPhysicalDeviceProperties(gpudev, &gpu_props);
		LOG_INFO("minTexelBufferOffsetAlignment  =%p\n", (void *)gpu_props.limits.minTexelBufferOffsetAlignment);
		LOG_INFO("minUniformBufferOffsetAlignment=%p\n", (void *)gpu_props.limits.minUniformBufferOffsetAlignment);
		LOG_INFO("minStorageBufferOffsetAlignment=%p\n", (void *)gpu_props.limits.minStorageBufferOffsetAlignment);
		vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, NULL);
		LOG_MAIN("vkGetPhysicalDeviceQueueFamilyProperties : queue_family_count=%d\n", queue_family_count);
		std::vector<VkQueueFamilyProperties> vqueue_props(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, vqueue_props.data());
		vkGetPhysicalDeviceFeatures(gpudev, &physDevFeatures);
		for (uint32_t i = 0; i < queue_family_count; i++) {
			auto flags = vqueue_props[i].queueFlags;
			if (flags & VK_QUEUE_GRAPHICS_BIT) {
				LOG_MAIN("index=%d : VK_QUEUE_GRAPHICS_BIT\n", i);
				if (graphics_queue_family_index == UINT32_MAX)
					graphics_queue_family_index = i;
			}

			if (flags & VK_QUEUE_COMPUTE_BIT)
				LOG_MAIN("index=%d : VK_QUEUE_COMPUTE_BIT\n", i);

			if (flags & VK_QUEUE_TRANSFER_BIT)
				LOG_MAIN("index=%d : VK_QUEUE_TRANSFER_BIT\n", i);

			if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
				LOG_MAIN("index=%d : VK_QUEUE_SPARSE_BINDING_BIT\n", i);

			if (flags & VK_QUEUE_PROTECTED_BIT)
				LOG_MAIN("index=%d : VK_QUEUE_PROTECTED_BIT\n", i);
		}

		//Create Device and Queue
		float queue_priorities[1] = {0.0};
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = NULL;
		queue_info.queueFamilyIndex = graphics_queue_family_index;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.flags = 0;

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = NULL;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledLayerCount = 1;
		device_info.ppEnabledLayerNames = debuglayers;
		device_info.enabledExtensionCount = (uint32_t)ext_names.size();
		device_info.ppEnabledExtensionNames = (const char *const *)ext_names.data();
		device_info.pEnabledFeatures = NULL;
		err = vkCreateDevice(gpudev, &device_info, NULL, &device);

		//get queue
		vkGetPhysicalDeviceMemoryProperties(gpudev, &devicememoryprop);
		vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);

		//Create Swapchain's
		VkWin32SurfaceCreateInfoKHR surfaceinfo = {};
		surfaceinfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceinfo.hinstance = GetModuleHandle(NULL);
		surfaceinfo.hwnd = hwnd;
		vkCreateWin32SurfaceKHR(inst, &surfaceinfo, NULL, &surface);

		//todo determine supported format from swapchain devices.
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpudev, 0, surface, &presentSupport);
		VkSurfaceCapabilitiesKHR capabilities = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpudev, surface, &capabilities);
		LOG_MAIN("vkGetPhysicalDeviceSurfaceSupportKHR Done\n", __LINE__);

		VkSwapchainCreateInfoKHR sc_info = {};
		sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		sc_info.surface = surface;
		sc_info.minImageCount = count;
		sc_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; //todo
		sc_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		sc_info.imageExtent.width = w;
		sc_info.imageExtent.height = h;
		sc_info.imageArrayLayers = 1;
		sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sc_info.queueFamilyIndexCount = 0;
		sc_info.pQueueFamilyIndices = nullptr;

		//http://vulkan-spec-chunked.ahcox.com/ch29s05.html#VkSurfaceTransformFlagBitsKHR
		sc_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		sc_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		//sc_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		sc_info.clipped = VK_TRUE;
		sc_info.oldSwapchain = VK_NULL_HANDLE;

		err = vkCreateSwapchainKHR(device, &sc_info, nullptr, &swapchain);

		//Get BackBuffer Images
		{
			uint32_t count = 0;
			std::vector<VkImage> temp;

			vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
			temp.resize(count);
			vkGetSwapchainImagesKHR(device, swapchain, &count, temp.data());
			for (auto & x : temp)
				LOG_MAIN("vkGetSwapchainImagesKHR temp = %p\n", x);

			for (int i = 0 ; i < temp.size(); i++) {
				auto name_color = oden_get_backbuffer_name(i);
				mimages[name_color] = temp[i];
				VkMemoryRequirements dummy = {};
				mmemreqs[name_color] = dummy;

				//dummy
				alloc_devmem(name_color, 256, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			}
		}

		//Create CommandBuffers
		cmd_pool = create_command_pool(device, graphics_queue_family_index);

		//Create Frame Resources
		devicebuffer.resize(count);

		for (int i = 0 ; i < count; i++) {
			auto & ref = devicebuffer[i];
			ref.cmdbuf = create_command_buffer(device, cmd_pool);
			ref.fence = create_fence(device);
			vkResetFences(device, 1, &ref.fence);

			LOG_MAIN("backbuffer cmdbuf[%d] = %p\n", i, ref.cmdbuf);
			LOG_MAIN("backbuffer fence[%d] = %p\n", i, ref.fence);
		}
		sampler_nearest = create_sampler(device, false);
		sampler_linear = create_sampler(device, true);
		descriptor_pool = create_descriptor_pool(device, heapcount);

		{
			std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding;
			for (int i = 0  ; i < slotmax; i++) {
				vdesc_setlayout_binding.push_back({(uint32_t)RDT_SLOT_SRV + i * RDT_SLOT_MAX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
				vdesc_setlayout_binding.push_back({(uint32_t)RDT_SLOT_CBV + i * RDT_SLOT_MAX, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
				vdesc_setlayout_binding.push_back({(uint32_t)RDT_SLOT_UAV + i * RDT_SLOT_MAX, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
			}
			descriptor_layout = create_descriptor_set_layout(device, vdesc_setlayout_binding);
			pipeline_layout = create_pipeline_layout(device, descriptor_layout);
		}

		for (int i = 0; i < 4096; i++)
			vdescriptor_sets.push_back(create_descriptor_set(device, descriptor_pool, descriptor_layout));

		LOG_MAIN("VkInstance inst = %p\n", inst);
		LOG_MAIN("VkPhysicalDevice gpudev = %p\n", gpudev);
		LOG_MAIN("VkDevice device = %p\n", device);
		LOG_MAIN("VkQueue graphics_queue = %p\n", graphics_queue);
		LOG_MAIN("VkSurfaceKHR surface = %p\n", surface);
		LOG_MAIN("VkSwapchainKHR swapchain = %p\n", swapchain);
		LOG_MAIN("vkCreateCommandPool cmd_pool = %p\n", cmd_pool);
		LOG_MAIN("vkCreateDescriptorPool descriptor_pool = %p\n", descriptor_pool);
		LOG_MAIN("vkCreateDescriptorSetLayout layout = %p\n", descriptor_layout);
		LOG_MAIN("vkCreatePipelineLayout = %p\n", pipeline_layout);
	}

	LOG_MAIN("frame_count=%llu\n", frame_count);

	//Determine resource index.
	auto & ref = devicebuffer[backbuffer_index];
	uint32_t present_index = 0;

	LOG_MAIN("vkWaitForFences[%d]\n", backbuffer_index);
	auto fence_status = vkGetFenceStatus(device, ref.fence);
	if (fence_status == VK_SUCCESS) {
		LOG_MAIN("The fence specified by fence is signaled.\n");
		auto wait_result = vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		LOG_MAIN("vkWaitForFences[%d] Done wait_result=%d(%s)\n", backbuffer_index, wait_result, wait_result ? "NG" : "OK");
		vkResetFences(device, 1, &ref.fence);
	}

	if (fence_status == VK_NOT_READY)
		LOG_MAIN("The fence specified by fence is unsignaled.\n");

	if (fence_status == VK_ERROR_DEVICE_LOST)
		LOG_MAIN("The device has been lost.\n");

	auto acquire_result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, ref.fence, &present_index);
	LOG_MAIN("vkAcquireNextImageKHR acquire_result=%d, present_index=%d, ref.fence=%p\n",
		acquire_result, present_index, ref.fence);

	//Destroy scratch resources
	for (auto & x : ref.vscratch_buffers)
		vkDestroyBuffer(device, x, NULL);
	ref.vscratch_buffers.clear();

	for (auto & x : ref.vscratch_devmems)
		vkFreeMemory(device, x, NULL);
	ref.vscratch_devmems.clear();

	//Destroy resources
	if (hwnd == nullptr) {
		vkDeviceWaitIdle(device);
		for (auto & ref : devicebuffer)
			vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		for (auto & x : mbuffers)
			vkDestroyBuffer(device, x.second, NULL);
		for (auto & x : mimageviews)
			vkDestroyImageView(device, x.second, NULL);
		for (auto & x : mimages)
			vkDestroyImage(device, x.second, NULL);
		for (auto & x : mdevmem)
			vkFreeMemory(device, x.second, NULL);
		mbuffers.clear();
		mimageviews.clear();
		mimages.clear();
		mdevmem.clear();
		return;
	}

	//Begin
	VkCommandBufferBeginInfo cmdbegininfo = {};
	cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbegininfo.pNext = nullptr;
	cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	cmdbegininfo.pInheritanceInfo = nullptr;

	vkResetCommandBuffer(ref.cmdbuf, 0);
	vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);

	LOG_MAIN("vcmd.size=%lu\n", vcmd.size());

	auto scratch_descriptor_sets = [&]() {
		rec.descriptor_sets = alloc_descriptor_sets();
		return rec.descriptor_sets;
	};

	auto setup_renderpass = [&](auto name, auto info, auto renderpass) {
		LOG_MAIN("setup_renderpass name=%s\n", name.c_str());
		rec.info = info;
		rec.renderpass = renderpass;
		rec.renderpass_name = name;
	};

	auto begin_renderpass = [&]() {
		if (rec.renderpass) {
			LOG_MAIN("!!!!!!!!!!!!!!!!!!!! vkCmdBeginRenderPass name=%s\n", rec.renderpass_name.c_str());
			vkCmdBeginRenderPass(ref.cmdbuf, &rec.info, VK_SUBPASS_CONTENTS_INLINE);
			rec.renderpass_commited = rec.renderpass;
		} else {
			LOG_MAIN("Failed vkCmdBeginRenderPass.\n");
		}
	};

	auto end_renderpass = [&]() {
		if (rec.renderpass_commited) {
			LOG_MAIN("!!!!!!!!!!!!!!!!!!!! vkCmdEndRenderPass name=%s\n", rec.renderpass_name.c_str());
			vkCmdEndRenderPass(ref.cmdbuf);
			rec.renderpass_commited = nullptr;
		}
	};

	//update shaders
	static bool is_lazy_shader_update = false;
	if (is_lazy_shader_update) {
		//lazy
		vkDeviceWaitIdle(device);
		mpipeline_bindpoints.clear();
		for (auto & x : mpipelines)
			vkDestroyPipeline(device, x.second, nullptr);
		mpipelines.clear();
		is_lazy_shader_update = false;
	}

	//Proc command.
	int cmd_index = 0;
	for (auto & c : vcmd) {
		auto type = c.type;
		auto name = c.name;
		LOG_MAIN("cmd_index = %04d name=%s: %s\n", cmd_index++, name.c_str(), oden_get_cmd_name(type));

		//allocate descriptor_sets.
		if (rec.descriptor_sets == nullptr)
			scratch_descriptor_sets();
		auto descriptor_sets = rec.descriptor_sets;

		if (type == CMD_SET_BARRIER) {
			LOG_MAIN("SET BARRIER name=%s\n", name.c_str());
			auto name_color = name;
			auto name_depth = oden_get_depth_render_target_name(name);
			auto image_color = mimages[name_color];
			auto image_depth = mimages[name_depth];

			//prepare for context roll.
			if (rec.renderpass_commited)
				end_renderpass();

			VkImageMemoryBarrier barrier = {};
			if (image_color) {
				if (c.set_barrier.to_present) {
					barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
				}
				if (c.set_barrier.to_texture) {
					if (name.find("depth") != std::string::npos)
						barrier = get_barrier(image_color, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
					else
						barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
				}
				if (c.set_barrier.to_rendertarget) {
					barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
				}
				LOG_MAIN("DONE BARRIER name=%s\n", name.c_str());
			}

			if (image_depth) {
				if (c.set_barrier.to_depthrendertarget) {
					barrier = get_barrier(image_depth, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
				}
				LOG_MAIN("DONE BARRIER name=%s\n", name.c_str());
			}
		}

		//CMD_SET_RENDER_TARGET
		if (type == CMD_SET_RENDER_TARGET) {
			auto x = c.set_render_target.rect.x;
			auto y = c.set_render_target.rect.y;
			auto w = c.set_render_target.rect.w;
			auto h = c.set_render_target.rect.h;
			bool is_backbuffer = c.set_render_target.is_backbuffer;

			//COLOR
			auto name_color = name;
			auto image_color = mimages[name_color];
			auto fmt_color = VK_FORMAT_B8G8R8A8_UNORM;
			int maxmips = oden_get_mipmap_max(w, h);

			//prepare for context roll.
			if (rec.renderpass_commited) {
				end_renderpass();
				descriptor_sets = scratch_descriptor_sets();
			}

			if (maxmips == 0)
				LOG_ERR("Invalid RT size w=%d, h=%d name=%s\n", w, h, name.c_str());
			if (image_color == nullptr) {
				image_color = create_image(device, w, h, fmt_color,
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						VK_IMAGE_USAGE_STORAGE_BIT |
						VK_IMAGE_USAGE_SAMPLED_BIT, maxmips);
				mimages[name_color] = image_color;
				LOG_MAIN("create_image name_color=%s, image_color=0x%p\n", name_color.c_str(), image_color);
			}

			//allocate color memreq and Bind
			if (mmemreqs.count(name_color) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetImageMemoryRequirements(device, image_color, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name_color] = memreqs;

				VkDeviceMemory devmem = alloc_devmem(
						name_color, memreqs.size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(device, image_color, devmem, 0);

				auto barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, maxmips);
				vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
			}

			//COLOR VIEW
			auto imageview_color = mimageviews[name_color];
			if (imageview_color == nullptr) {
				LOG_MAIN("create_image_view name=%s\n", name_color.c_str());
				imageview_color = create_image_view(device, image_color, fmt_color, VK_IMAGE_ASPECT_COLOR_BIT);
				mimageviews[name_color] = imageview_color;
				LOG_MAIN("create_image_view imageview_color=0x%p\n", imageview_color);
				if (is_backbuffer == false) {
					for (int i = 0 ; i < maxmips; i++) {
						auto imageview_color_mip = create_image_view(device, image_color, fmt_color, VK_IMAGE_ASPECT_COLOR_BIT, i);
						auto name_color_mip = oden_get_mipmap_name(name_color, i);
						mimageviews[name_color_mip] = imageview_color_mip;
						LOG_MAIN("create_image_view name=%s, imageview_color_mip=0x%p\n",
							name_color_mip.c_str(), imageview_color_mip);
					}
					VkImageSubresourceRange image_range_color = {};
					image_range_color.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					image_range_color.baseMipLevel = 0;
					image_range_color.levelCount = maxmips;
					image_range_color.baseArrayLayer = 0;
					image_range_color.layerCount = 1;

					VkClearColorValue clearColor = {};
					clearColor.float32[0] = c.clear.color[0];
					clearColor.float32[1] = c.clear.color[1];
					clearColor.float32[2] = c.clear.color[2];
					clearColor.float32[3] = c.clear.color[3];
					vkCmdClearColorImage(ref.cmdbuf, image_color, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_range_color);
				}
			}

			//DEPTH
			auto name_depth = oden_get_depth_render_target_name(name);
			auto image_depth = mimages[name_depth];
			auto fmt_depth = VK_FORMAT_D32_SFLOAT;
			if (image_depth == nullptr) {
				image_depth = create_image(device, w, h, fmt_depth,
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);
				mimages[name_depth] = image_depth;

				LOG_MAIN("create_image name_depth=%s, image_depth=0x%p\n", name_depth.c_str(), image_depth);
			}

			//allocate depth memreq and Bind
			if (mmemreqs.count(name_depth) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetImageMemoryRequirements(device, image_depth, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name_depth] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(
						name_depth, memreqs.size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(device, image_depth, devmem, 0);

				auto barrier = get_barrier(image_depth, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
				vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
			}

			//DEPTH VIEW
			auto imageview_depth = mimageviews[name_depth];
			if (imageview_depth == nullptr) {
				LOG_MAIN("create_image_view name=%s\n", name_color.c_str());
				imageview_depth = create_image_view(device, image_depth, fmt_depth, VK_IMAGE_ASPECT_DEPTH_BIT);
				mimageviews[name_depth] = imageview_depth;
				LOG_MAIN("create_image_view imageview_depth=0x%p\n", imageview_depth);
			}

			//RENDER PASS
			auto renderpass = mrenderpasses[name];
			LOG_MAIN("query renderpass name=%s\n", name.c_str());
			if (renderpass == nullptr) {
				renderpass = create_renderpass(device, 1, is_backbuffer, fmt_color, fmt_depth);
				LOG_MAIN("create_renderpass name=%s, ptr=%p\n", name_color.c_str(), renderpass);
				mrenderpasses[name] = renderpass;
			}

			//FRAMEBUFFER
			auto framebuffer = mframebuffers[name];
			if (framebuffer == nullptr && renderpass) {
				std::vector<VkImageView> imageviews;
				imageviews.push_back(imageview_color);
				imageviews.push_back(imageview_depth);

				framebuffer = create_framebuffer(device, renderpass, imageviews, w, h);
				LOG_MAIN("create_framebuffer name=%s, ptr=%p\n", name_color.c_str(), framebuffer);
				mframebuffers[name] = framebuffer;
			}
			LOG_MAIN("found renderpass name=%s, ptr=%p\n", name_color.c_str(), renderpass);
			LOG_MAIN("found framebuffer name=%s, ptr=%p\n", name_color.c_str(), framebuffer);

			//setup viewport and scissor
			VkViewport viewport = {};
			viewport.width = (float)w;
			viewport.height = (float)h;
			viewport.minDepth = (float)0.0f;
			viewport.maxDepth = (float)1.0f;
			vkCmdSetViewport(ref.cmdbuf, 0, 1, &viewport);

			VkRect2D scissor = {};
			scissor.extent.width = w;
			scissor.extent.height = h;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(ref.cmdbuf, 0, 1, &scissor);

			VkRenderPassBeginInfo rp_begin = {};
			rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rp_begin.pNext = NULL;
			rp_begin.renderPass = renderpass;
			rp_begin.framebuffer = framebuffer;
			rp_begin.renderArea.offset.x = 0;
			rp_begin.renderArea.offset.y = 0;
			rp_begin.renderArea.extent.width = w;
			rp_begin.renderArea.extent.height = h;
			rp_begin.clearValueCount = 0;
			rp_begin.pClearValues = nullptr;
			setup_renderpass(name, rp_begin, renderpass);
		}

		//CMD_SET_TEXTURE
		if (type == CMD_SET_TEXTURE || type == CMD_SET_TEXTURE_UAV) {
			uint32_t w = c.set_texture.rect.w;
			uint32_t h = c.set_texture.rect.h;
			auto slot = c.set_texture.slot;
			LOG_MAIN("DEBUG : name=%s, c.set_texture.miplevel=%d\n", name.c_str(), c.set_texture.miplevel);

			//prepare for context roll.
			if (rec.renderpass_commited)
				end_renderpass();

			if (rec.descriptor_sets == nullptr)
				descriptor_sets = scratch_descriptor_sets();

			//COLOR
			auto name_color = name;
			auto fmt_color = VK_FORMAT_R8G8B8A8_UNORM;
			auto image_color = mimages[name_color];
			if (image_color == nullptr) {
				image_color = create_image(device, w, h, fmt_color,
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						VK_IMAGE_USAGE_STORAGE_BIT |
						VK_IMAGE_USAGE_SAMPLED_BIT, 1);
				mimages[name_color] = image_color;
				LOG_MAIN("create_image name_color=%s, image_color=0x%p\n", name_color.c_str(), image_color);
			}

			//allocate color memreq and Bind
			if (mmemreqs.count(name_color) == 0) {
				auto data = c.buf.data();
				auto size = c.buf.size();
				VkMemoryRequirements memreqs = {};

				vkGetImageMemoryRequirements(device, image_color, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name_color] = memreqs;

				VkDeviceMemory devmem = alloc_devmem(
						name_color, memreqs.size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(device, image_color, devmem, 0);

				{
					LOG_MAIN("create_buffer-staging name=%s\n", name.c_str());
					auto scratch_buffer = create_buffer(device, size);
					ref.vscratch_buffers.push_back(scratch_buffer);

					LOG_MAIN("create_buffer-staging name=%s Done\n", name.c_str());
					VkMemoryRequirements memreqs = {};
					vkGetBufferMemoryRequirements(device, scratch_buffer, &memreqs);
					memreqs.size = memreqs.size + (memreqs.alignment - 1);
					memreqs.size &= ~(memreqs.alignment - 1);
					VkDeviceMemory devmem = alloc_devmem(std::string(name) + "_staging", memreqs.size,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
							VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
					ref.vscratch_devmems.push_back(devmem);
					vkBindBufferMemory(device, scratch_buffer, devmem, 0);
					LOG_MAIN("vkBindBufferMemory name=%s Done\n", name.c_str());
					void *dest = nullptr;
					vkMapMemory(device, devmem, 0, memreqs.size, 0, (void **)&dest);
					if (dest) {
						LOG_MAIN("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
						memcpy(dest, data, size);
						vkUnmapMemory(device, devmem);
					} else {
						LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
						Sleep(1000);
					}

					auto before_barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
					auto after_barrier = get_barrier(image_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &before_barrier);
					VkBufferImageCopy copy_region = {};
					copy_region.bufferOffset = 0;
					copy_region.bufferRowLength = w;
					copy_region.bufferImageHeight = h;
					copy_region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
					copy_region.imageOffset = {0, 0, 0};
					copy_region.imageExtent = {w, h, 1};
					vkCmdCopyBufferToImage(ref.cmdbuf, scratch_buffer, image_color, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
					vkCmdPipelineBarrier(ref.cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &after_barrier);
				}
			}

			//COLOR VIEW
			auto imageview_color = mimageviews[name_color];
			if (imageview_color == nullptr) {
				LOG_MAIN("create_image_view name=%s\n", name_color.c_str());
				imageview_color = create_image_view(device, image_color, fmt_color, VK_IMAGE_ASPECT_COLOR_BIT);
				mimageviews[name_color] = imageview_color;
				LOG_MAIN("create_image_view imageview_color=0x%p\n", imageview_color);
			}

			if (descriptor_sets) {
				if (type == CMD_SET_TEXTURE) {
					auto binding = (RDT_SLOT_MAX * slot) + RDT_SLOT_SRV;
					VkDescriptorImageInfo image_info = {};
					image_info.sampler = sampler_linear;
					image_info.imageView = imageview_color;
					image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					LOG_MAIN("vkUpdateDescriptorSets(image) start name=%s binding=%d\n", name.c_str(), binding);
					update_descriptor_sets(device, descriptor_sets, &image_info, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				}

				if (type == CMD_SET_TEXTURE_UAV) {
					auto miplevel = c.set_texture.miplevel;
					auto name_color_mip = oden_get_mipmap_name(name_color, miplevel);
					imageview_color = mimageviews[name_color_mip];
					auto binding = (RDT_SLOT_MAX * slot) + RDT_SLOT_UAV;
					VkDescriptorImageInfo image_info = {};
					image_info.sampler = sampler_linear;
					image_info.imageView = imageview_color;
					image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					update_descriptor_sets(device, descriptor_sets, &image_info, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					LOG_MAIN("vkUpdateDescriptorSets(compute image) imageview_color=%p, start name=%s binding=%d\n", imageview_color, name_color_mip.c_str(), binding);
				}
			} else {
				LOG_ERR("WARNING name=%s descriptor_sets is null\n", name.c_str());
				LOG_ERR("You Need prepare binding DS name=%s\n", name.c_str());
			}
		}

		//CMD_SET_CONSTANT
		if (type == CMD_SET_CONSTANT) {
			auto slot = c.set_constant.slot;
			auto data = c.buf.data();
			auto size = c.buf.size();
			//Create Constant Buffer
			auto buffer = mbuffers[name];
			if (buffer == nullptr) {
				LOG_MAIN("create_buffer-constant name=%s\n", name.c_str());
				buffer = create_buffer(device, size);
				mbuffers[name] = buffer;
				LOG_MAIN("create_buffer-constant name=%s Done\n", name.c_str());
			}

			//allocate buffer memreq and Bind
			auto devmem = mdevmem[name];
			if (devmem == nullptr) {
				VkMemoryRequirements memreqs = {};

				vkGetBufferMemoryRequirements(device, buffer, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name] = memreqs;
				devmem = alloc_devmem(name, memreqs.size,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
						VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				vkBindBufferMemory(device, buffer, devmem, 0);
				LOG_MAIN("vkBindBufferMemory name=%s Done\n", name.c_str());
			}

			//update
			void *dest = nullptr;
			vkMapMemory(device, devmem, 0, size, 0, (void **)&dest);
			if (dest) {
				LOG_MAIN("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
				memcpy(dest, data, size);
				vkUnmapMemory(device, devmem);
			} else {
				LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
				Sleep(1000);
			}

			if (descriptor_sets) {
				//update buffer reference
				auto binding = (RDT_SLOT_MAX * slot) + RDT_SLOT_CBV;
				VkDescriptorBufferInfo buffer_info = {};
				buffer_info.buffer = buffer;
				buffer_info.offset = 0;
				buffer_info.range = size;

				update_descriptor_sets(device, descriptor_sets, &buffer_info, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				LOG_MAIN("vkUpdateDescriptorSets start name=%s binding=%d\n", name.c_str(), binding);
			} else {
				LOG_ERR("You Need prepare binding DS name=%s\n", name.c_str());
			}
		}

		//CMD_SET_VERTEX
		if (type == CMD_SET_VERTEX) {
			auto buffer = mbuffers[name];
			auto data = c.buf.data();
			auto size = c.buf.size();
			if (buffer == nullptr) {
				LOG_MAIN("create_buffer-vertex name=%s\n", name.c_str());
				buffer = create_buffer(device, size);
				mbuffers[name] = buffer;
				LOG_MAIN("create_buffer-vertex name=%s Done\n", name.c_str());
			}

			//allocate buffer memreq and Bind
			if (mmemreqs.count(name) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetBufferMemoryRequirements(device, buffer, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(name, memreqs.size,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
						VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				vkBindBufferMemory(device, buffer, devmem, 0);
				LOG_MAIN("vkBindBufferMemory name=%s Done\n", name.c_str());

				void *dest = nullptr;
				vkMapMemory(device, devmem, 0, memreqs.size, 0, (void **)&dest);
				if (dest) {
					LOG_MAIN("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					memcpy(dest, data, size);
					vkUnmapMemory(device, devmem);
				} else {
					LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					Sleep(1000);
				}
			}

			VkDeviceSize offsets[1] = {0};
			vkCmdBindVertexBuffers(ref.cmdbuf, 0, 1, &buffer, offsets);
			LOG_MAIN("vkCmdBindVertexBuffers name=%s\n", name.c_str());
		}

		//CMD_SET_INDEX
		if (type == CMD_SET_INDEX) {
			auto buffer = mbuffers[name];
			auto data = c.buf.data();
			auto size = c.buf.size();
			if (buffer == nullptr) {
				LOG_MAIN("create_buffer-index name=%s\n", name.c_str());
				buffer = create_buffer(device, size);
				mbuffers[name] = buffer;
				LOG_MAIN("create_buffer-index name=%s Done\n", name.c_str());
			}

			//allocate buffer memreq and Bind
			if (mmemreqs.count(name) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetBufferMemoryRequirements(device, buffer, &memreqs);
				memreqs.size = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(name, memreqs.size,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
						VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				vkBindBufferMemory(device, buffer, devmem, 0);
				LOG_MAIN("vkBindBufferMemory index name=%s Done\n", name.c_str());

				void *dest = nullptr;
				vkMapMemory(device, devmem, 0, memreqs.size, 0, (void **)&dest);
				if (dest) {
					LOG_MAIN("vkMapMemory index name=%s addr=0x%p\n", name.c_str(), dest);
					memcpy(dest, data, size);
					vkUnmapMemory(device, devmem);
				} else {
					LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					Sleep(1000);
				}
			}

			VkDeviceSize offset = {};
			vkCmdBindIndexBuffer(ref.cmdbuf, buffer, offset, VK_INDEX_TYPE_UINT32);
			LOG_MAIN("vkCmdBindIndexBuffers name=%s\n", name.c_str());
		}

		//CMD_SET_SHADER
		if (type == CMD_SET_SHADER) {
			if (c.set_shader.is_update)
				is_lazy_shader_update = true;

			auto binding_point = mpipeline_bindpoints[name];
			auto pipeline = mpipelines[name];

			//prepare for context roll.
			if (rec.renderpass_commited) {
				end_renderpass();
				descriptor_sets = scratch_descriptor_sets();
			}

			if (pipeline == nullptr) {
				pipeline = create_gpipeline_from_file(device, name.c_str(), pipeline_layout, rec.renderpass);
				if (pipeline) {
					mpipelines[name] = pipeline;
					mpipeline_bindpoints[name] = VK_PIPELINE_BIND_POINT_GRAPHICS;
					binding_point = mpipeline_bindpoints[name];
				}
			}

			if (pipeline == nullptr) {
				pipeline = create_cpipeline_from_file(device, name.c_str(), pipeline_layout);
				if (pipeline) {
					mpipelines[name] = pipeline;
					mpipeline_bindpoints[name] = VK_PIPELINE_BIND_POINT_COMPUTE;
					binding_point = mpipeline_bindpoints[name];
				}
			}

			if (pipeline) {
				LOG_MAIN("vkCmdBindPipeline\n");
				vkCmdBindPipeline(ref.cmdbuf, binding_point, pipeline);

				LOG_MAIN("vkCmdBindDescriptorSets descriptor_sets=%p\n", descriptor_sets);
				vkCmdBindDescriptorSets(
					ref.cmdbuf, binding_point, pipeline_layout, 0,
					1, (const VkDescriptorSet *)&descriptor_sets, 0, NULL);
			} else {
				LOG_ERR("Failed make pipeline name=%s\n", name.c_str());
			}
		}

		//CMD_CLEAR
		if (type == CMD_CLEAR) {
			auto name_color = name;
			auto image_color = mimages[name_color];
			if (image_color == nullptr)
				LOG_ERR("NULL image_color name=%s\n", name_color.c_str());
			LOG_MAIN("vkCmdClearColorImage name=%s\n", name_color.c_str());

			//prepare for context roll.
			if (rec.renderpass_commited)
				end_renderpass();

			VkImageSubresourceRange image_range_color = {};
			image_range_color.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_range_color.baseMipLevel = 0;
			image_range_color.levelCount = 1;
			image_range_color.baseArrayLayer = 0;
			image_range_color.layerCount = 1;

			VkClearColorValue clearColor = {};
			clearColor.float32[0] = c.clear.color[0];
			clearColor.float32[1] = c.clear.color[1];
			clearColor.float32[2] = c.clear.color[2];
			clearColor.float32[3] = c.clear.color[3];
			vkCmdClearColorImage(ref.cmdbuf, image_color, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_range_color);
		}

		//CMD_CLEAR_DEPTH
		if (type == CMD_CLEAR_DEPTH) {
			auto name_depth = oden_get_depth_render_target_name(name);
			auto image_depth = mimages[name_depth];
			if (image_depth == nullptr)
				LOG_ERR("NULL image_depth name=%s\n", name_depth.c_str());

			//prepare for context roll.
			if (rec.renderpass_commited)
				end_renderpass();

			//Depth
			LOG_MAIN("vkCmdClearDepthStencilImage name=%s\n", name_depth.c_str());
			VkClearDepthStencilValue cdsv = {1.0f, 0};
			VkImageSubresourceRange image_range_depth = {};
			image_range_depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			image_range_depth.baseMipLevel = 0;
			image_range_depth.levelCount = 1;
			image_range_depth.baseArrayLayer = 0;
			image_range_depth.layerCount = 1;
			vkCmdClearDepthStencilImage(ref.cmdbuf, image_depth, VK_IMAGE_LAYOUT_GENERAL, &cdsv, 1, &image_range_depth);
		}

		//CMD_DRAW_INDEX
		if (type == CMD_DRAW_INDEX) {
			if (!rec.renderpass_commited)
				begin_renderpass();
			vkCmdBindDescriptorSets(
				ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,
				1, (const VkDescriptorSet*)&rec.descriptor_sets, 0, NULL);
			vkCmdDrawIndexed(ref.cmdbuf, c.draw_index.count, 1, 0, 0, 0);
		}

		//CMD_DRAW
		if (type == CMD_DRAW) {
			if (!rec.renderpass_commited)
				begin_renderpass();
			vkCmdBindDescriptorSets(
				ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,
				1, (const VkDescriptorSet *)&rec.descriptor_sets, 0, NULL);
			vkCmdDraw(ref.cmdbuf, c.draw.vertex_count, 1, 0, 0);
		}

		//CMD_DISPATCH
		if (type == CMD_DISPATCH) {
			vkCmdBindDescriptorSets(
				ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0,
				1, (const VkDescriptorSet *)&rec.descriptor_sets, 0, NULL);
			vkCmdDispatch(ref.cmdbuf, c.dispatch.x, c.dispatch.y, c.dispatch.z);

			//discard dispatch desc set and increase.
			scratch_descriptor_sets();
		}
	}
	if (rec.renderpass_commited)
		vkCmdEndRenderPass(ref.cmdbuf);

	//End Command Buffer
	vkEndCommandBuffer(ref.cmdbuf);

	//for debug.
	{
		for (auto & pair : mimages) {
			LOG_MAIN("handle=0x%p : name=%s\n", pair.second, pair.first.c_str());
		}
	}

	//Submit and Present
	VkPipelineStageFlags wait_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = &wait_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &ref.cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = nullptr;
	{
		auto fence_status = vkGetFenceStatus(device, ref.fence);
		LOG_MAIN("BEFORE vkGetFenceStatus fence_status=%d\n", fence_status);
		vkResetFences(device, 1, &ref.fence);
	}

	LOG_MAIN("vkQueueSubmit backbuffer_index=%d, fence=%p\n", backbuffer_index, ref.fence);
	auto submit_result = vkQueueSubmit(graphics_queue, 1, &submit_info, ref.fence);
	LOG_MAIN("vkQueueSubmit Done backbuffer_index=%d, fence=%p, submit_result=%d\n", backbuffer_index, ref.fence, submit_result);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.waitSemaphoreCount = 0;
	present_info.pWaitSemaphores = nullptr;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &present_index;
	present_info.pResults = nullptr;

	vkQueuePresentKHR(graphics_queue, &present_info);

	backbuffer_index = frame_count % count;
	LOG_MAIN("=======================================================================\n");
	LOG_MAIN("FRAME Done frame_count=%d\n", frame_count);
	LOG_MAIN("=======================================================================\n");

	frame_count++;
}
