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

#define LOG_INFO(...) printf("INFO : " __FUNCTION__ ":" __VA_ARGS__)
#define LOG_ERR(...) printf("ERR : " __FUNCTION__ ":" __VA_ARGS__)

using namespace oden;

static VkImage
create_image(VkDevice device, uint32_t width, uint32_t height, VkFormat format,
	VkImageUsageFlags usageFlags)
{
	VkImage ret = VK_NULL_HANDLE;

	VkImageCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {width, height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	vkCreateImage(device, &info, NULL, &ret);
	return (ret);
}

static VkImageView
create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
	VkImageView ret = VK_NULL_HANDLE;

	VkImageViewCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = {aspectMask, 0, 1, 0, 1},
	};
	vkCreateImageView(device, &info, NULL, &ret);
	return (ret);
}

static VkBuffer
create_buffer(VkDevice device, VkDeviceSize size)
{
	VkBuffer ret = VK_NULL_HANDLE;

	VkBufferCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size  = size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	};
	vkCreateBuffer(device, &info, nullptr, &ret);
	return (ret);
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
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		printf("ERROR : ");
	}
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		printf("WARNING : ");
	}
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
		printf("PERFORMANCE : ");
	}
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
		printf("INFO : ");
	}
	printf("%s", pMessage);
	printf("\n");
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		printf("\n");
	}
	return VK_FALSE;
}

inline void
bind_debug_fn(VkInstance instance, VkDebugReportCallbackCreateInfoEXT ext)
{
	VkDebugReportCallbackEXT callback;
	PFN_vkCreateDebugReportCallbackEXT func = PFN_vkCreateDebugReportCallbackEXT(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));

	if (func) {
		func(instance, &ext, nullptr, &callback);
	} else {
		printf("PFN_vkCreateDebugReportCallbackEXT IS NULL\n");
	}
}

void
oden::oden_present_graphics(
	const char * appname, std::vector<cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t count, uint32_t heapcount, uint32_t slotmax)
{
	HWND hwnd = (HWND) handle;

	struct DeviceBuffer {
		uint64_t value = 0;
		VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
	};

	static VkInstance inst = VK_NULL_HANDLE;
	static VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	static VkDevice device = VK_NULL_HANDLE;
	static VkQueue graphics_queue = VK_NULL_HANDLE;
	static VkSurfaceKHR surface = VK_NULL_HANDLE;
	static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	static VkPhysicalDeviceMemoryProperties devicememoryprop = {};
	static VkCommandPool cmd_pool;
	static std::map<std::string, VkImage> mimages;
	static std::map<std::string, VkImageView> mimageviews;
	static std::map<std::string, VkBuffer> mbuffers;
	static std::map<std::string, VkMemoryRequirements> mmemreqs;
	static std::map<std::string, VkDeviceMemory> mdevmem;

	static uint64_t deviceindex = 0;
	static uint64_t frame_count = 0;

	static std::vector<DeviceBuffer> devicebuffer;

	auto alloc_devmem = [ = ](auto name, VkDeviceSize size, VkMemoryPropertyFlags flags) {
		if (mdevmem.count(name) != 0)
			LOG_INFO("already allocated name=%s\n", name.c_str());

		VkMemoryAllocateInfo ma_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = size,
			.memoryTypeIndex = 0,
		};

		for (uint32_t i = 0; i < devicememoryprop.memoryTypeCount; i++) {
			if ((devicememoryprop.memoryTypes[i].propertyFlags & flags) == flags) {
				ma_info.memoryTypeIndex = i;
				break;
			}
		}
		VkDeviceMemory devmem = nullptr;
		vkAllocateMemory(device, &ma_info, nullptr, &devmem);
		if (devmem)
			mdevmem[name] = devmem;
		else
			LOG_ERR("Can't alloc name=%s\n", name.c_str());
		return devmem;
	};

	if (inst == nullptr) {
		uint32_t inst_ext_cnt = 0;
		uint32_t gpu_count = 0;
		uint32_t device_extension_count = 0;
		uint32_t queue_family_count = 0;
		std::vector<char *> vinstance_ext_names;
		std::vector<char *> ext_names;

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
			LOG_INFO("vkEnumerateInstanceExtensionProperties : name=%s\n", name.c_str());
		}
		VkDebugReportCallbackCreateInfoEXT callbackInfo = {};
		callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callbackInfo.flags = 0;
		callbackInfo.flags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
		callbackInfo.flags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
		//callbackInfo.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
		callbackInfo.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;

		callbackInfo.pfnCallback = &debug_callback;

		//Create vk instances
		const VkApplicationInfo app = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = &callbackInfo,
			.pApplicationName = appname,
			.applicationVersion = VK_MAKE_VERSION(0, 0, 1),
			.pEngineName = appname,
			.engineVersion = 0,
			.apiVersion = VK_API_VERSION_1_0,
		};

		//DEBUG
		vinstance_ext_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		static const char *debuglayers[] = { "VK_LAYER_LUNARG_standard_validation" };
		VkInstanceCreateInfo inst_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = NULL,
			.pApplicationInfo = &app,

			//DEBUG
			.enabledLayerCount = 1,
			.ppEnabledLayerNames = debuglayers,

			.enabledExtensionCount = (uint32_t)vinstance_ext_names.size(),
			.ppEnabledExtensionNames = (const char *const *)vinstance_ext_names.data(),
		};
		auto err = vkCreateInstance(&inst_info, NULL, &inst);
		bind_debug_fn(inst, callbackInfo);

		//Enumaration GPU's
		err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
		LOG_INFO("gpu_count=%d\n", gpu_count);
		if (gpu_count != 1) {
			LOG_ERR("---------------------------------------------\n");
			LOG_ERR("muri\n");
			LOG_ERR("---------------------------------------------\n");
			exit(1);
		}
		err = vkEnumeratePhysicalDevices(inst, &gpu_count, &gpudev);
		err = vkEnumerateDeviceExtensionProperties(gpudev, NULL, &device_extension_count, NULL);
		std::vector<VkExtensionProperties> vdevice_extensions(device_extension_count);
		err = vkEnumerateDeviceExtensionProperties(gpudev, NULL, &device_extension_count, vdevice_extensions.data());
		LOG_INFO("vkEnumerateDeviceExtensionProperties : device_extension_count = %p, VK_KHR_SWAPCHAIN_EXTENSION_NAME=%s\n", vdevice_extensions.size(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		for (auto x : vdevice_extensions) {
			auto name = std::string(x.extensionName);
			if (name == VK_KHR_SWAPCHAIN_EXTENSION_NAME)
				ext_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			LOG_INFO("vkEnumerateDeviceExtensionProperties : extensionName=%s\n", x.extensionName);
		}

		//Enumeration Queue attributes.
		VkPhysicalDeviceProperties gpu_props = {};
		vkGetPhysicalDeviceProperties(gpudev, &gpu_props);
		vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, NULL);
		LOG_INFO("vkGetPhysicalDeviceQueueFamilyProperties : queue_family_count=%d\n", queue_family_count);
		std::vector<VkQueueFamilyProperties> vqueue_props(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, vqueue_props.data());
		VkPhysicalDeviceFeatures physDevFeatures;
		vkGetPhysicalDeviceFeatures(gpudev, &physDevFeatures);
		uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
		uint32_t presentQueueFamilyIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queue_family_count; i++) {
			auto flags = vqueue_props[i].queueFlags;
			if (flags & VK_QUEUE_GRAPHICS_BIT) {
				LOG_INFO("index=%d : VK_QUEUE_GRAPHICS_BIT\n", i);
				if (graphicsQueueFamilyIndex == UINT32_MAX)
					graphicsQueueFamilyIndex = i;
			}

			if (flags & VK_QUEUE_COMPUTE_BIT)
				LOG_INFO("index=%d : VK_QUEUE_COMPUTE_BIT\n", i);

			if (flags & VK_QUEUE_TRANSFER_BIT)
				LOG_INFO("index=%d : VK_QUEUE_TRANSFER_BIT\n", i);

			if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
				LOG_INFO("index=%d : VK_QUEUE_SPARSE_BINDING_BIT\n", i);

			if (flags & VK_QUEUE_PROTECTED_BIT)
				LOG_INFO("index=%d : VK_QUEUE_PROTECTED_BIT\n", i);
		}

		//Create Device and Queue's
		float queue_priorities[1] = {0.0};
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = NULL;
		queue_info.queueFamilyIndex = graphicsQueueFamilyIndex;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.flags = 0;
		VkDeviceCreateInfo device_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = NULL,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue_info,

			//DEBUG
			.enabledLayerCount = 1,
			.ppEnabledLayerNames = debuglayers,

			.enabledExtensionCount = (uint32_t)ext_names.size(),
			.ppEnabledExtensionNames = (const char *const *)ext_names.data(),
			.pEnabledFeatures = NULL,
		};
		err = vkCreateDevice(gpudev, &device_info, NULL, &device);
		vkGetPhysicalDeviceMemoryProperties(gpudev, &devicememoryprop);

		vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphics_queue);

		//Create Swapchain's
		VkWin32SurfaceCreateInfoKHR surfaceinfo = {};
		surfaceinfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceinfo.hinstance = GetModuleHandle(NULL);
		surfaceinfo.hwnd = hwnd;
		vkCreateWin32SurfaceKHR(inst, &surfaceinfo, NULL, &surface);
		VkSwapchainCreateInfoKHR sc_info = {
			.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface            = surface,
			.minImageCount      = count,
			.imageFormat        = VK_FORMAT_B8G8R8A8_UNORM, //todo
			.imageColorSpace    = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
			.imageExtent{w, h},
			.imageArrayLayers   = 1,
			.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode   = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0, //VK_SHARING_MODE_CONCURRENT
			.pQueueFamilyIndices = nullptr,   //VK_SHARING_MODE_CONCURRENT
			//http://vulkan-spec-chunked.ahcox.com/ch29s05.html#VkSurfaceTransformFlagBitsKHR
			.preTransform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			//.presentMode        = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
			.presentMode        = VK_PRESENT_MODE_IMMEDIATE_KHR,
			.clipped            = VK_TRUE,
			.oldSwapchain       = VK_NULL_HANDLE,
		};
		err = vkCreateSwapchainKHR(device, &sc_info, nullptr, &swapchain);

		//Get BackBuffer Images
		{
			std::vector<VkImage> temp;
			uint32_t count = 0;

			vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
			temp.resize(count);
			vkGetSwapchainImagesKHR(device, swapchain, &count, temp.data());
			for (auto & x : temp)
				LOG_INFO("temp = %p\n", x);

			for (int i = 0 ; i < temp.size(); i++) {
				auto name_color = oden_get_backbuffer_name(i);
				mimages[name_color] = temp[i];
				VkMemoryRequirements dummy = {};
				mmemreqs[name_color] = dummy;
			}
		}

		//Create CommandBuffers
		const VkCommandPoolCreateInfo cmd_pool_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = graphicsQueueFamilyIndex,
		};
		vkCreateCommandPool(device, &cmd_pool_info, NULL, &cmd_pool);

		//Create Frame Resources
		devicebuffer.resize(count);
		for (int i = 0 ; i < count; i++) {
			auto & ref = devicebuffer[i];
			VkFenceCreateInfo fence_ci = {
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.pNext = NULL,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT
			};

			VkCommandBufferAllocateInfo cba_info = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = cmd_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			err = vkAllocateCommandBuffers(device, &cba_info, &ref.cmdbuf);
			err = vkCreateFence(device, &fence_ci, NULL, &ref.fence);
			vkResetFences(device, 1, &ref.fence);
			LOG_INFO("backbuffer cmdbuf[%d] = %p\n", i, ref.cmdbuf);
			LOG_INFO("backbuffer fence[%d] = %p\n", i, ref.fence);
		}


		LOG_INFO("VkInstance inst = %p\n", inst);
		LOG_INFO("VkPhysicalDevice gpudev = %p\n", gpudev);
		LOG_INFO("VkDevice device = %p\n", device);
		LOG_INFO("VkQueue graphics_queue = %p\n", graphics_queue);
		LOG_INFO("VkSurfaceKHR surface = %p\n", surface);
		LOG_INFO("VkSwapchainKHR swapchain = %p\n", swapchain);
		LOG_INFO("vkCreateCommandPool cmd_pool = %p\n", cmd_pool);
		//todo : VkDescriptorPool, descrpter buffer layout pipeline shader ImageView Depth
	}


	//Determine resource index.
	auto & ref = devicebuffer[deviceindex];
	LOG_INFO("vkWaitForFences[%d]\n", deviceindex);
	auto fence_status = vkGetFenceStatus(device, ref.fence);
	if (fence_status == VK_SUCCESS) {
		LOG_INFO("The fence specified by fence is signaled.\n");
		vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		LOG_INFO("vkWaitForFences[%d] Done\n", deviceindex);
		vkResetFences(device, 1, &ref.fence);
	}

	if (fence_status == VK_NOT_READY)
		LOG_INFO("The fence specified by fence is unsignaled.\n");

	if (fence_status == VK_ERROR_DEVICE_LOST)
		LOG_INFO("The device has been lost.\n");

	uint32_t present_index = 0;
	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, ref.fence, &present_index);
	LOG_INFO("vkAcquireNextImageKHR present_index=%d\n", present_index);

	//Destroy resources
	if (hwnd == nullptr) {
	}

	//Begin
	VkCommandBufferBeginInfo cmdbegininfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		.pInheritanceInfo = nullptr,
	};
	vkResetCommandBuffer(ref.cmdbuf, 0);
	vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);

	//Proc command.
	for (auto & c : vcmd) {
		auto type = c.type;
		auto name = c.name;

		//CMD_SET_RENDER_TARGET
		if (type == CMD_SET_RENDER_TARGET) {
			auto x = c.set_render_target.rect.x;
			auto y = c.set_render_target.rect.y;
			auto w = c.set_render_target.rect.w;
			auto h = c.set_render_target.rect.h;

			//COLOR
			auto name_color = name;
			auto image_color = mimages[name_color];
			auto fmt_color = VK_FORMAT_B8G8R8A8_UNORM;
			if (image_color == nullptr) {
				image_color = create_image(device, w, h, fmt_color, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
				mimages[name_color] = image_color;
				LOG_INFO("create_image name_color=%s, image_color=0x%p\n", name_color.c_str(), image_color);
			}

			//allocate color memreq and Bind
			if (mmemreqs.count(name_color) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetImageMemoryRequirements(device, image_color, &memreqs);
				memreqs.size  = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name_color] = memreqs;

				VkDeviceMemory devmem = alloc_devmem(name_color, memreqs.size,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(device, image_color, devmem, 0);
			}

			//COLOR VIEW
			auto imageview_color = mimageviews[name_color];
			if (imageview_color == nullptr) {
				LOG_INFO("create_image_view name=%s\n", name_color.c_str());
				imageview_color = create_image_view(device, image_color, fmt_color, VK_IMAGE_ASPECT_COLOR_BIT);
				mimageviews[name_color] = imageview_color;
				LOG_INFO("create_image_view imageview_color=0x%p\n", imageview_color);
			}

			//DEPTH
			auto name_depth = oden_get_depth_render_target_name(name);
			auto image_depth = mimages[name_depth];
			auto fmt_depth = VK_FORMAT_D32_SFLOAT;
			if (image_depth == nullptr) {
				image_depth = create_image(device, w, h, fmt_depth, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
				mimages[name_depth] = image_depth;
				LOG_INFO("create_image name_depth=%s, image_depth=0x%p\n", name_depth.c_str(), image_depth);
			}

			//allocate depth memreq and Bind
			if (mmemreqs.count(name_depth) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetImageMemoryRequirements(device, image_depth, &memreqs);
				memreqs.size  = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name_depth] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(name_depth, memreqs.size,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(device, image_depth, devmem, 0);
			}

			//DEPTH VIEW
			auto imageview_depth = mimageviews[name_depth];
			if (imageview_depth == nullptr) {
				LOG_INFO("create_image_view name=%s\n", name_color.c_str());
				imageview_depth = create_image_view(device, image_depth, fmt_depth, VK_IMAGE_ASPECT_DEPTH_BIT);
				mimageviews[name_depth] = imageview_depth;
				LOG_INFO("create_image_view imageview_depth=0x%p\n", imageview_depth);
			}

			//todo setup rendertarget and framebuffers
		}

		//CMD_SET_TEXTURE
		if (type == CMD_SET_TEXTURE || type == CMD_SET_TEXTURE_UAV) {
			auto w = c.set_texture.rect.w;
			auto h = c.set_texture.rect.h;
			auto slot = c.set_texture.slot;
			//“o˜^‚³‚ê‚Ä‚¢‚È‚¯‚ê‚ÎImage, ImageViewì¬‚·‚é
		}

		//CMD_SET_CONSTANT
		if (type == CMD_SET_CONSTANT) {
			auto slot = c.set_constant.slot;
		}

		//CMD_SET_VERTEX
		if (type == CMD_SET_VERTEX) {
			auto buffer = mbuffers[name];
			auto size = c.set_vertex.size;
			auto data = c.set_vertex.data;
			if (buffer == nullptr) {
				LOG_INFO("create_buffer-vertex name=%s\n", name.c_str());
				buffer = create_buffer(device, size);
				mbuffers[name] = buffer;
				LOG_INFO("create_buffer-vertex name=%s Done\n", name.c_str());
			}

			//allocate buffer memreq and Bind
			if (mmemreqs.count(name) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetBufferMemoryRequirements(device, buffer, &memreqs);
				memreqs.size  = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(name, memreqs.size,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
						VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				vkBindBufferMemory(device, buffer, devmem, 0);
				LOG_INFO("vkBindBufferMemory name=%s Done\n", name.c_str());

				void *dest = nullptr;
				vkMapMemory(device, devmem, 0, memreqs.size, 0, (void **)&dest);
				if (dest) {
					LOG_INFO("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					memcpy(dest, data, size);
					vkUnmapMemory(device, devmem);
				} else {
					LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					Sleep(1000);
				}
			}

			VkDeviceSize offsets[1] = {0};
			vkCmdBindVertexBuffers(ref.cmdbuf, 0, 1, &buffer, offsets);
			LOG_INFO("vkCmdBindVertexBuffers name=%s\n", name.c_str());
		}

		//CMD_SET_INDEX
		if (type == CMD_SET_INDEX) {
			auto buffer = mbuffers[name];
			auto size = c.set_index.size;
			auto data = c.set_index.data;
			if (buffer == nullptr) {
				LOG_INFO("create_buffer-index name=%s\n", name.c_str());
				buffer = create_buffer(device, size);
				mbuffers[name] = buffer;
				LOG_INFO("create_buffer-index name=%s Done\n", name.c_str());
			}

			//allocate buffer memreq and Bind
			if (mmemreqs.count(name) == 0) {
				VkMemoryRequirements memreqs = {};

				vkGetBufferMemoryRequirements(device, buffer, &memreqs);
				memreqs.size  = memreqs.size + (memreqs.alignment - 1);
				memreqs.size &= ~(memreqs.alignment - 1);
				mmemreqs[name] = memreqs;
				VkDeviceMemory devmem = alloc_devmem(name, memreqs.size,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
						VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				vkBindBufferMemory(device, buffer, devmem, 0);
				LOG_INFO("vkBindBufferMemory index name=%s Done\n", name.c_str());

				void *dest = nullptr;
				vkMapMemory(device, devmem, 0, memreqs.size, 0, (void **)&dest);
				if (dest) {
					LOG_INFO("vkMapMemory index name=%s addr=0x%p\n", name.c_str(), dest);
					memcpy(dest, data, size);
					vkUnmapMemory(device, devmem);
				} else {
					LOG_ERR("vkMapMemory name=%s addr=0x%p\n", name.c_str(), dest);
					Sleep(1000);
				}
			}

			VkDeviceSize offset = {};
			vkCmdBindIndexBuffer(ref.cmdbuf, buffer, offset, VK_INDEX_TYPE_UINT32);
			LOG_INFO("vkCmdBindIndexBuffers name=%s\n", name.c_str());
		}

		//CMD_SET_SHADER
		if (type == CMD_SET_SHADER) {
		}

		//CMD_CLEAR
		if (type == CMD_CLEAR) {
			auto name_color = name;
			auto image_color = mimages[name_color];
			if (image_color == nullptr)
				LOG_ERR("NULL image_color name=%s\n", name.c_str());

			VkClearColorValue clearColor = {
				c.clear.color[0],
				c.clear.color[1],
				c.clear.color[2],
				c.clear.color[3],
			};
			VkClearValue clearValue = {};
			clearValue.color = clearColor;

			VkImageSubresourceRange imageRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			};
			LOG_INFO("vkCmdClearColorImage name=%s\n", name_color.c_str());
			vkCmdClearColorImage(ref.cmdbuf, image_color, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &imageRange);
		}

		//CMD_CLEAR_DEPTH
		if (type == CMD_CLEAR_DEPTH) {

		}

		//CMD_DRAW_INDEX
		if (type == CMD_DRAW_INDEX) {
			auto count = c.draw_index.count;
		}

		//CMD_DRAW
		if (type == CMD_DRAW) {
			auto vertex_count = c.draw.vertex_count;
		}

		//CMD_DISPATCH
		if (type == CMD_DISPATCH) {
			auto x = c.dispatch.x;
			auto y = c.dispatch.y;
			auto z = c.dispatch.z;
		}
	}

	//Submit and Present
	vkEndCommandBuffer(ref.cmdbuf);
	VkPipelineStageFlags wait_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = &wait_mask,
		.commandBufferCount = 1,
		.pCommandBuffers = &ref.cmdbuf,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr,
	};
	vkQueueSubmit(graphics_queue, 1, &submit_info, ref.fence);

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &present_index,
		.pResults = nullptr,
	};
	vkQueuePresentKHR(graphics_queue, &present_info);
	frame_count++;
	deviceindex = frame_count % count;
}
