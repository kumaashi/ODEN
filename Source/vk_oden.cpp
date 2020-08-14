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

#define LOG_INFO(...) printf("INFO:" __FUNCTION__ ":" __VA_ARGS__)
#define LOG_ERR(...) printf("ERR:" __FUNCTION__ ":" __VA_ARGS__)

using namespace oden;

void
oden::oden_present_graphics(
	const char * appname, std::vector<cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t count, uint32_t heapcount, uint32_t slotmax)
{
	HWND hwnd = (HWND) handle;

	std::vector<VkExtensionProperties> vinstance_ext;
	std::vector<char *> vinstance_ext_names;
	std::vector<char *> ext_names;
	static VkInstance inst = VK_NULL_HANDLE;
	static VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	static VkDevice device = VK_NULL_HANDLE;
	static VkQueue graphics_queue = VK_NULL_HANDLE;
	static VkSurfaceKHR surface = VK_NULL_HANDLE;
	static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	static std::vector<VkImage> swapimages;
	static uint64_t deviceindex = 0;
	static uint64_t frame_count = 0;

	struct DeviceBuffer {
		uint64_t value = 0;
	};
	static std::vector<DeviceBuffer> devicebuffer;

	if (inst == nullptr) {
		uint32_t inst_ext_cnt = 0;
		uint32_t gpu_count = 0;
		uint32_t device_extension_count = 0;
		uint32_t queue_family_count = 0;

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
		const VkApplicationInfo app = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = NULL,
			.pApplicationName = appname,
			.applicationVersion = 0,
			.pEngineName = appname,
			.engineVersion = 0,
			.apiVersion = VK_API_VERSION_1_0,
		};
		VkInstanceCreateInfo inst_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = NULL,
			.pApplicationInfo = &app,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = (uint32_t)vinstance_ext_names.size(),
			.ppEnabledExtensionNames = (const char *const *)vinstance_ext_names.data(),
		};
		auto err = vkCreateInstance(&inst_info, NULL, &inst);
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
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = NULL,
			.enabledExtensionCount = (uint32_t)ext_names.size(),
			.ppEnabledExtensionNames = (const char *const *)ext_names.data(),
			.pEnabledFeatures = NULL,
		};
		err = vkCreateDevice(gpudev, &device_info, NULL, &device);
		vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphics_queue);
		
		//create surface 
		VkWin32SurfaceCreateInfoKHR surfaceinfo = {};
		surfaceinfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceinfo.hinstance = GetModuleHandle(NULL);
		surfaceinfo.hwnd = hwnd;
		vkCreateWin32SurfaceKHR(inst, &surfaceinfo, NULL, &surface);
		VkSwapchainCreateInfoKHR sc_info = {};
		sc_info.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		sc_info.surface            = surface;
		sc_info.minImageCount      = count;
		sc_info.imageFormat        = VK_FORMAT_B8G8R8A8_UNORM; //todo
		sc_info.imageColorSpace    = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		sc_info.imageExtent.width  = w;
		sc_info.imageExtent.height = h;
		sc_info.imageArrayLayers   = 1;
		sc_info.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		sc_info.imageSharingMode   = VK_SHARING_MODE_EXCLUSIVE;
		sc_info.queueFamilyIndexCount = 0; //VK_SHARING_MODE_CONCURRENT
		sc_info.pQueueFamilyIndices = nullptr;   //VK_SHARING_MODE_CONCURRENT
		//http://vulkan-spec-chunked.ahcox.com/ch29s05.html#VkSurfaceTransformFlagBitsKHR
		sc_info.preTransform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		sc_info.compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		sc_info.preTransform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		sc_info.presentMode        = VK_PRESENT_MODE_FIFO_KHR;
		sc_info.clipped            = VK_TRUE;
		sc_info.oldSwapchain       = VK_NULL_HANDLE;

		err = vkCreateSwapchainKHR(device, &sc_info, nullptr, &swapchain);
		
		{
			uint32_t count = 0;
			vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
			swapimages.resize(count);
			vkGetSwapchainImagesKHR(device, swapchain, &count, swapimages.data());
		}
		LOG_INFO("err err = %p\n", err);
		LOG_INFO("VkInstance inst = %p\n", inst);
		LOG_INFO("VkPhysicalDevice gpudev = %p\n", gpudev);
		LOG_INFO("VkDevice device = %p\n", device);
		LOG_INFO("VkQueue graphics_queue = %p\n", graphics_queue);
		LOG_INFO("VkSurfaceKHR surface = %p\n", surface);
		LOG_INFO("VkSwapchainKHR swapchain = %p\n", swapchain);
		for(auto & x : swapimages)
			LOG_INFO("swapimages = %p\n", x);
		
		//todo : descrpter buffer layout pipeline shader ImageView Depth
	}


	auto & ref = devicebuffer[deviceindex];

	if (hwnd == nullptr) {
	}

	for (auto & c : vcmd) {
		auto type = c.type;
		auto name = c.name;

		//CMD_SET_RENDER_TARGET
		if (type == CMD_SET_RENDER_TARGET) {
			auto x = c.set_render_target.rect.x;
			auto y = c.set_render_target.rect.y;
			auto w = c.set_render_target.rect.w;
			auto h = c.set_render_target.rect.h;
		}

		//CMD_SET_TEXTURE
		if (type == CMD_SET_TEXTURE || type == CMD_SET_TEXTURE_UAV) {
			auto w = c.set_texture.rect.w;
			auto h = c.set_texture.rect.h;
			auto slot = c.set_texture.slot;
		}

		//CMD_SET_CONSTANT
		if (type == CMD_SET_CONSTANT) {
			auto slot = c.set_constant.slot;
		}

		//CMD_SET_VERTEX
		if (type == CMD_SET_VERTEX) {
		}

		//CMD_SET_INDEX
		if (type == CMD_SET_INDEX) {
		}

		//CMD_SET_SHADER
		if (type == CMD_SET_SHADER) {
		}

		//CMD_CLEAR
		if (type == CMD_CLEAR) {
		}

		//CMD_CLEAR_
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

	frame_count++;
}
