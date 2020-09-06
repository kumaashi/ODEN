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
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>

#include <map>
#include <vector>
#include <string>
#include <vector>
#include <algorithm>
#include <DirectXMath.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "D3DCompiler.lib")

#define info_printf(...) printf("INFO:" __FUNCTION__ ":" __VA_ARGS__)
#define err_printf(...) printf("ERR:" __FUNCTION__ ":" __VA_ARGS__)

#ifdef ODEN_SUPPORT_DXR
#define ID3D12DeviceIF ID3D12Device5
#define ID3D12GraphicsCommandListIF ID3D12GraphicsCommandList4
#else //ODEN_SUPPORT_DXR
#define ID3D12DeviceIF ID3D12Device
#define ID3D12GraphicsCommandListIF ID3D12GraphicsCommandList
#endif //ODEN_SUPPORT_DXR

using namespace oden;

static ID3D12Resource *
create_resource(std::string name, ID3D12Device *dev,
	int w, int h, DXGI_FORMAT fmt, D3D12_RESOURCE_FLAGS flags,
	BOOL is_upload = FALSE, void *data = 0, size_t size = 0)
{
	ID3D12Resource *res = nullptr;
	D3D12_RESOURCE_DESC desc = {
		D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, UINT64(w), UINT(h), 1, 1, fmt,
		{1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, flags
	};
	D3D12_HEAP_PROPERTIES hprop = {
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN, 1, 1,
	};
	int maxmips = oden_get_mipmap_max(w, h);
	desc.MipLevels = maxmips;

	if (is_upload) {
		hprop.Type = D3D12_HEAP_TYPE_UPLOAD;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.MipLevels = 1;
	}
	auto state = D3D12_RESOURCE_STATE_GENERIC_READ;
	auto hr = dev->CreateCommittedResource(&hprop, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&res));
	if (hr)
		err_printf("name=%s: w=%d, h=%d, flags=%08X, hr=%08X\n", name.c_str(), w, h, flags, hr);
	info_printf("name=%s: w=%d, h=%d, flags=%08X\n", name.c_str(), w, h, flags);
	if (res && is_upload && data) {
		UINT8 *dest = nullptr;
		res->Map(0, NULL, reinterpret_cast<void **>(&dest));
		if (dest) {
			info_printf("UPLOAD name=%s: w=%d, h=%d, data=%p, size=%zu\n", name.c_str(), w, h, data, size);
			memcpy(dest, data, size);
			res->Unmap(0, NULL);
		} else {
			err_printf("CAN'T MAP name=%s: w=%d, h=%d, data=%p, size=%zu\n", name.c_str(), w, h, data, size);
		}
	}
	return res;
}

static D3D12_RESOURCE_BARRIER
get_barrier(ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = res;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	return barrier;
}

static D3D12_SHADER_BYTECODE
create_shader_from_file(std::string fstr, std::string entry, std::string profile,
	std::vector<uint8_t> &shader_code)
{
	ID3DBlob *blob = nullptr;
	ID3DBlob *blob_err = nullptr;
	ID3DBlob *blob_sig = nullptr;

	std::vector<WCHAR> wfname;
	UINT flags = 0;
	for (int i = 0; i < fstr.length(); i++)
		wfname.push_back(fstr[i]);
	wfname.push_back(0);
	D3DCompileFromFile(&wfname[0], NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry.c_str(), profile.c_str(), flags, 0, &blob, &blob_err);
	if (blob_err) {
		printf("%s:\n%s\n", __FUNCTION__, (char *) blob_err->GetBufferPointer());
		blob_err->Release();
	}
	if (!blob && !blob_err)
		printf("File Not found : %s\n", fstr.c_str());
	if (!blob)
		return {nullptr, 0};
	shader_code.resize(blob->GetBufferSize());
	memcpy(shader_code.data(), blob->GetBufferPointer(), blob->GetBufferSize());
	if (blob)
		blob->Release();
	return {shader_code.data(), shader_code.size() };
}

void
oden::oden_present_graphics(const char * appname, std::vector<cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t num, uint32_t heapcount, uint32_t slotmax)
{
	HWND hwnd = (HWND) handle;
	enum {
		RDT_SLOT_SRV = 0,
		RDT_SLOT_CBV,
		RDT_SLOT_UAV,
		RDT_SLOT_MAX,
	};
	struct DeviceBuffer {
		ID3D12CommandAllocator *cmdalloc = nullptr;
		ID3D12GraphicsCommandListIF *cmdlist = nullptr;
		ID3D12Fence *fence = nullptr;
		std::vector<ID3D12Resource *> vscratch;
		uint64_t value = 0;
	};
	static std::vector<DeviceBuffer> devicebuffer;
	static ID3D12Device *dev = nullptr;
	static ID3D12CommandQueue *queue = nullptr;
	static IDXGISwapChain3 *swapchain = nullptr;
	static ID3D12DescriptorHeap *heap_rtv = nullptr;
	static ID3D12DescriptorHeap *heap_dsv = nullptr;
	static ID3D12DescriptorHeap *heap_shader = nullptr;
	static ID3D12RootSignature *rootsig = nullptr;
	static std::map<std::string, ID3D12Resource *> mres;
	static std::map<std::string, ID3D12PipelineState *> mpstate;
	static std::map<std::string, uint64_t> mcpu_handle;
	static std::map<std::string, uint64_t> mgpu_handle;
	static uint64_t handle_index_rtv = 0;
	static uint64_t handle_index_dsv = 0;
	static uint64_t handle_index_shader = 0;
	static uint64_t deviceindex = 0;
	static uint64_t frame_count = 0;

	if (dev == nullptr) {
		D3D12_COMMAND_QUEUE_DESC cqdesc = {};
		D3D12_DESCRIPTOR_HEAP_DESC dhdesc_rtv = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, heapcount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
		D3D12_DESCRIPTOR_HEAP_DESC dhdesc_dsv = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, heapcount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
		D3D12_DESCRIPTOR_HEAP_DESC dhdesc_shader = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, heapcount, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
		ID3D12Debug* debugController;
#ifdef DEBUG
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->Release();
		}
#endif //DEBUG
		D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev));
#ifdef ODEN_SUPPORT_DXR
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
			dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
			if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0) {
				exit(0);
			}
		}
#endif //ODEN_SUPPORT_DXR
		dev->CreateCommandQueue(&cqdesc, IID_PPV_ARGS(&queue));
		dev->CreateDescriptorHeap(&dhdesc_rtv, IID_PPV_ARGS(&heap_rtv));
		dev->CreateDescriptorHeap(&dhdesc_dsv, IID_PPV_ARGS(&heap_dsv));
		dev->CreateDescriptorHeap(&dhdesc_shader, IID_PPV_ARGS(&heap_shader));

		IDXGIFactory4 *factory = nullptr;
		IDXGISwapChain *temp = nullptr;
		DXGI_SWAP_CHAIN_DESC desc = {
			{
				w, h, {0, 0},
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
				DXGI_MODE_SCALING_UNSPECIFIED
			},
			{1, 0},
			DXGI_USAGE_RENDER_TARGET_OUTPUT,
			num, hwnd, TRUE, DXGI_SWAP_EFFECT_FLIP_DISCARD,
			DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
		};
		CreateDXGIFactory1(IID_PPV_ARGS(&factory));
		factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
		factory->CreateSwapChain(queue, &desc, &temp);
		temp->QueryInterface(IID_PPV_ARGS(&swapchain));
		temp->Release();
		factory->Release();

		devicebuffer.resize(num);
		for (int i = 0; i < num; i++) {
			auto & x = devicebuffer[i];
			dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&x.cmdalloc));
			dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, x.cmdalloc, nullptr, IID_PPV_ARGS(&x.cmdlist));
			dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&x.fence));
			x.cmdlist->Close();
			x.value = i;
		}

		for (int i = 0 ; i < num; i++) {
			ID3D12Resource *res = nullptr;
			swapchain->GetBuffer(i, IID_PPV_ARGS(&res));
			mres[oden_get_backbuffer_name(i)] = res;
		}

		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
		std::vector<D3D12_ROOT_PARAMETER> vroot_param;
		std::vector<D3D12_DESCRIPTOR_RANGE> vdesc_range;
		D3D12_ROOT_PARAMETER root_param = {};
		root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_param.DescriptorTable.NumDescriptorRanges = 1;

		for (UINT i = 0 ; i < slotmax; i++) {
			vdesc_range.push_back({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
			vdesc_range.push_back({D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
			vdesc_range.push_back({D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
		}

		for (auto & x : vdesc_range) {
			root_param.DescriptorTable.pDescriptorRanges = &x;
			vroot_param.push_back(root_param);
		}

		//https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_static_sampler_desc
		D3D12_STATIC_SAMPLER_DESC sampler[2];
		const D3D12_STATIC_SAMPLER_DESC default_sampler = {
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			0.0f, 0,
			D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
			0, 0, D3D12_SHADER_VISIBILITY_ALL,
		};

		sampler[0] = default_sampler;
		sampler[0].ShaderRegister = 0;
		sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler[1] = default_sampler;
		sampler[1].ShaderRegister = 1;
		sampler[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		root_signature_desc.pStaticSamplers = sampler;
		root_signature_desc.NumStaticSamplers = _countof(sampler);

		root_signature_desc.NumParameters = vroot_param.size();
		root_signature_desc.pParameters = vroot_param.data();
		ID3DBlob *perrblob = nullptr;
		ID3DBlob *signature = nullptr;

		auto hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &perrblob);
		if (hr && perrblob) {
			err_printf("Failed D3D12SerializeRootSignature:\n%s\n", (char *) perrblob->GetBufferPointer());
			exit(1);
		}

		hr = dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootsig));
		if (perrblob) perrblob->Release();
		if (signature) signature->Release();
	};

	{
		auto ret = dev->GetDeviceRemovedReason();
		if (ret) {
			err_printf("!!!!!!Device Lost frame_count=%lld, reason=%08X\n", frame_count, ret);
			exit(1);
		}
	}

	std::map<std::string, D3D12_RESOURCE_TRANSITION_BARRIER> mbarrier;
	deviceindex = swapchain->GetCurrentBackBufferIndex();

	auto & ref = devicebuffer[deviceindex];

	queue->Signal(ref.fence, ref.value);
	if (ref.fence->GetCompletedValue() < ref.value) {
		auto hevent = CreateEvent(NULL, FALSE, FALSE, NULL);
		ref.fence->SetEventOnCompletion(ref.value, hevent);
		WaitForSingleObject(hevent, INFINITE);
		CloseHandle(hevent);
	}
	ref.value++;

	for (auto & scratch : ref.vscratch)
		scratch->Release();
	ref.vscratch.clear();

	if (hwnd == nullptr) {
		auto release = [](auto & x) {
			if (x) x->Release();
			x = nullptr;
		};
		auto mrelease = [](auto & m, auto release) {
			for (auto & p : m) {
				if (p.second)
					printf("%s : release=%s\n", __FUNCTION__, p.first.c_str());
				release(p.second);
			}
			m.clear();
		};
		for (auto & ref : devicebuffer) {
			release(ref.fence);
			release(ref.cmdlist);
			release(ref.cmdalloc);
		}
		mrelease(mres, release);
		mrelease(mpstate, release);
		release(rootsig);
		release(heap_shader);
		release(heap_dsv);
		release(heap_rtv);
		release(swapchain);
		release(queue);
		release(dev);
		return;
	}

	ref.cmdalloc->Reset();
	ref.cmdlist->Reset(ref.cmdalloc, 0);
	ref.cmdlist->SetGraphicsRootSignature(rootsig);
	ref.cmdlist->SetComputeRootSignature(rootsig);
	ref.cmdlist->SetDescriptorHeaps(1, &heap_shader);

	for (auto & c : vcmd) {
		auto type = c.type;
		auto name = c.name;
		auto res = mres[name];
		auto pstate = mpstate[name];

		auto fmt_color = DXGI_FORMAT_R16G16B16A16_FLOAT;
		auto fmt_depth = DXGI_FORMAT_D32_FLOAT;

		//CMD_SET_BARRIER
		if (type == CMD_SET_BARRIER) {
			D3D12_RESOURCE_TRANSITION_BARRIER tb {};
			tb.pResource = nullptr;
			if (c.set_barrier.to_present) {
				tb.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				tb.StateAfter = D3D12_RESOURCE_STATE_COMMON;
			}
			if (c.set_barrier.to_texture) {
				tb.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				tb.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}
			if (c.set_barrier.to_rendertarget) {
				tb.StateBefore = D3D12_RESOURCE_STATE_COMMON;
				tb.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
			mbarrier[name] = tb;
		}

		//CMD_SET_RENDER_TARGET
		if (type == CMD_SET_RENDER_TARGET) {
			auto x = c.set_render_target.rect.x;
			auto y = c.set_render_target.rect.y;
			auto w = c.set_render_target.rect.w;
			auto h = c.set_render_target.rect.h;
			auto name_color = name;
			auto name_depth = oden_get_depth_render_target_name(name);
			auto cpu_handle_color = heap_rtv->GetCPUDescriptorHandleForHeapStart();
			auto cpu_handle_depth = heap_dsv->GetCPUDescriptorHandleForHeapStart();

			{
				auto res = mres[name_color];
				if (res == nullptr) {
					res = create_resource(name_color, dev, w, h, fmt_color,
							D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
					if (!res) {
						err_printf("create_resource(rtv) name=%s\n", name.c_str());
						exit(1);
					}
					mres[name_color] = res;
				}

				if (mcpu_handle.count(name_color) == 0) {
					D3D12_RENDER_TARGET_VIEW_DESC desc = {};
					auto res_desc = res->GetDesc();
					auto temp = cpu_handle_color;
					desc.Format = res_desc.Format;
					desc.Texture2D.MipSlice = 0;
					desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					temp.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * handle_index_rtv;
					dev->CreateRenderTargetView(res, &desc, temp);
					mcpu_handle[name_color] = handle_index_rtv++;
				};

				auto cpu_index = mcpu_handle[name_color];
				cpu_handle_color.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * cpu_index;
			}

			{
				auto res = mres[name_depth];
				if (res == nullptr) {
					res = create_resource(name_depth, dev, w, h, fmt_depth, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
					if (!res) {
						err_printf("create_resource(dsv) name=%s\n", name.c_str());
						exit(1);
					}
					mres[name_depth] = res;
				}

				if (mcpu_handle.count(name_depth) == 0) {
					auto temp = cpu_handle_depth;
					D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
					desc.Format = fmt_depth;
					desc.Texture2D.MipSlice = 0;
					desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					temp.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * handle_index_dsv;
					dev->CreateDepthStencilView(res, &desc, temp);
					mcpu_handle[name_depth] = handle_index_dsv++;
				};
				auto cpu_index = mcpu_handle[name_depth];
				cpu_handle_depth.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * cpu_index;
			}
			if (mbarrier.count(name_color)) {
				D3D12_RESOURCE_BARRIER barrier = get_barrier(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
				barrier.Transition = mbarrier[name_color];
				barrier.Transition.pResource = mres[name_color];
				ref.cmdlist->ResourceBarrier(1, &barrier);
			}
			mbarrier.erase(name_color);
			if (mbarrier.count(name_depth)) {
				D3D12_RESOURCE_BARRIER barrier = get_barrier(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
				barrier.Transition = mbarrier[name_depth];
				barrier.Transition.pResource = mres[name_depth];
				ref.cmdlist->ResourceBarrier(1, &barrier);
			}
			mbarrier.erase(name_depth);
			

			D3D12_VIEWPORT viewport = { FLOAT(x), FLOAT(y), FLOAT(w), FLOAT(h), 0.0f, 1.0f };
			D3D12_RECT rect = { x, y, w, h };
			ref.cmdlist->RSSetViewports(1, &viewport);
			ref.cmdlist->RSSetScissorRects(1, &rect);
			ref.cmdlist->OMSetRenderTargets(1, &cpu_handle_color, FALSE, &cpu_handle_depth);
		}

		//CMD_SET_TEXTURE
		if (type == CMD_SET_TEXTURE || type == CMD_SET_TEXTURE_UAV) {
			auto w = c.set_texture.rect.w;
			auto h = c.set_texture.rect.h;
			auto cpu_handle = heap_shader->GetCPUDescriptorHandleForHeapStart();
			auto gpu_handle = heap_shader->GetGPUDescriptorHandleForHeapStart();
			auto slot = c.set_texture.slot;

			if (res == nullptr) {
				info_printf("res=null : name=%s\n", name.c_str());
				fmt_color = DXGI_FORMAT_R8G8B8A8_UNORM;
				res = create_resource(name, dev, w, h, fmt_color, D3D12_RESOURCE_FLAG_NONE);
				if (!res) {
					err_printf("create_resource(texture) name=%s\n", name.c_str());
					exit(1);
				}
				auto scratch = create_resource(name, dev, c.set_texture.size, 1,
						DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE, TRUE, c.set_texture.data, c.set_texture.size);
				if (!scratch) {
					err_printf("create_resource(texture scratch) name=%s\n", name.c_str());
					exit(1);
				}
				ref.vscratch.push_back(scratch);
				mres[name] = res;

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
				D3D12_TEXTURE_COPY_LOCATION dest = {};
				D3D12_TEXTURE_COPY_LOCATION src = {};
				D3D12_RESOURCE_DESC desc_res = res->GetDesc();
				UINT64 total_bytes = 0;
				UINT subres_index = 0;

				dev->GetCopyableFootprints(&desc_res, subres_index, 1, 0, &footprint, nullptr, nullptr, &total_bytes);
				dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				dest.pResource = res;
				src.pResource = scratch;

				dest.SubresourceIndex = subres_index;
				src.PlacedFootprint = footprint;
				ref.cmdlist->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
				D3D12_RESOURCE_BARRIER barrier = get_barrier(res, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
				ref.cmdlist->ResourceBarrier(1, &barrier);
			}
			D3D12_RESOURCE_DESC desc_res = res->GetDesc();
			if (type == CMD_SET_TEXTURE) {
				if (mgpu_handle.count(name) == 0) {
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					desc.Format = fmt_color;
					if(name.find("depth") != std::string::npos)
						desc.Format = DXGI_FORMAT_R32_FLOAT;
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Texture2D.MipLevels = desc_res.MipLevels;
					cpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * handle_index_shader;
					dev->CreateShaderResourceView(res, &desc, cpu_handle);
					info_printf("CMD_SET_TEXTURE CreateShaderResourceView name=%s, fmt=%d\n", name.c_str(), desc.Format);
					mgpu_handle[name] = handle_index_shader++;
				}

				if (mbarrier.count(name) && (desc_res.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
					D3D12_RESOURCE_BARRIER barrier = get_barrier(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
					barrier.Transition = mbarrier[name];
					barrier.Transition.pResource = mres[name];
					ref.cmdlist->ResourceBarrier(1, &barrier);
				}
				mbarrier.erase(name);
				auto gpu_index = mgpu_handle[name];
				gpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * gpu_index;
				ref.cmdlist->SetGraphicsRootDescriptorTable((slot * RDT_SLOT_MAX) + RDT_SLOT_SRV, gpu_handle);
			}
			if (type == CMD_SET_TEXTURE_UAV) {
				if (mgpu_handle.count(name) == 0) {
					for (int i = 0 ; i < desc_res.MipLevels; i++) {
						D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
						desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
						desc.Texture2D.MipSlice = i;
						desc.Texture2D.PlaneSlice = 0;
						desc.Format = desc_res.Format;
						auto cpu_handle = heap_shader->GetCPUDescriptorHandleForHeapStart();
						cpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * handle_index_shader;
						dev->CreateUnorderedAccessView(res, nullptr, &desc, cpu_handle);
						if (dev->GetDeviceRemovedReason()) {
							err_printf("GetDeviceRemovedReason UAV\n");
							exit(1);
						}
						mgpu_handle[oden_get_mipmap_name(name, i)] = handle_index_shader;
						if (i == 0)
							mgpu_handle[name] = handle_index_shader;
						handle_index_shader++;
					}
				}
				auto miplevel = c.set_texture.miplevel;
				auto mipname = oden_get_mipmap_name(name, miplevel);
				auto gpu_index = mgpu_handle[mipname];
				auto gpu_handle = heap_shader->GetGPUDescriptorHandleForHeapStart();

				gpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * gpu_index;
				ref.cmdlist->SetComputeRootDescriptorTable((slot * RDT_SLOT_MAX) + RDT_SLOT_UAV, gpu_handle);
			}
		}

		//CMD_SET_CONSTANT
		if (type == CMD_SET_CONSTANT) {
			auto cpu_handle = heap_shader->GetCPUDescriptorHandleForHeapStart();
			auto gpu_handle = heap_shader->GetGPUDescriptorHandleForHeapStart();
			auto slot = c.set_constant.slot;
			if (res == nullptr) {
				res = create_resource(name, dev, c.set_constant.size, 1,
						DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE, TRUE, c.set_constant.data, c.set_constant.size);
				if (!res) {
					err_printf("create_resource(cbv) name=%s\n", name.c_str());
					exit(1);
				}
				mres[name] = res;
			}
			if (mgpu_handle.count(name) == 0) {
				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				D3D12_RESOURCE_DESC desc_res = res->GetDesc();
				desc.SizeInBytes = (desc_res.Width + 255) & ~255;
				desc.BufferLocation = res->GetGPUVirtualAddress();
				cpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * handle_index_shader;
				dev->CreateConstantBufferView(&desc, cpu_handle);
				mgpu_handle[name] = handle_index_shader++;
			}
			auto gpu_index = mgpu_handle[name];
			gpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * gpu_index;
			ref.cmdlist->SetGraphicsRootDescriptorTable((slot * RDT_SLOT_MAX) + RDT_SLOT_CBV, gpu_handle);
			{
				UINT8 *dest = nullptr;
				res->Map(0, NULL, reinterpret_cast<void **>(&dest));
				if (dest) {
					memcpy(dest, c.set_constant.data, c.set_constant.size);
					res->Unmap(0, NULL);
				} else {
					printf("%s : can't map\n", __FUNCTION__);
				}
			}
		}

		//CMD_SET_VERTEX
		if (type == CMD_SET_VERTEX) {
			if (res == nullptr) {
				res = create_resource(name, dev, c.set_vertex.size, 1,
						DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE, TRUE, c.set_vertex.data, c.set_vertex.size);
				if (!res) {
					err_printf("create_resource(buffer vertex) name=%s\n", name.c_str());
					exit(1);
				}
				mres[name] = res;
			}
			D3D12_VERTEX_BUFFER_VIEW view = {
				res->GetGPUVirtualAddress(), UINT(c.set_vertex.size), UINT(c.set_vertex.stride_size)
			};
			ref.cmdlist->IASetVertexBuffers(0, 1, &view);
			ref.cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		//CMD_SET_INDEX
		if (type == CMD_SET_INDEX) {
			auto res = mres[name];
			if (res == nullptr && c.set_index.data) {
				res = create_resource(name, dev, c.set_index.size, 1,
						DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE, TRUE, c.set_index.data, c.set_index.size);
				if (!res) {
					err_printf("create_resource(buffer index) name=%s\n", name.c_str());
					exit(1);
				}
				mres[name] = res;
			}
			if (c.set_index.data) {
				D3D12_INDEX_BUFFER_VIEW view = {
					res->GetGPUVirtualAddress(), UINT(c.set_index.size), DXGI_FORMAT_R32_UINT
				};
				ref.cmdlist->IASetIndexBuffer(&view);
			} else {
				ref.cmdlist->IASetIndexBuffer(nullptr);
			}
		}

		//CMD_SET_SHADER
		if (type == CMD_SET_SHADER) {
			if (pstate == nullptr || c.set_shader.is_update) {
				if (pstate)
					pstate->Release();
				pstate = nullptr;
				mpstate[name] = nullptr;

				std::vector<uint8_t> vs;
				std::vector<uint8_t> gs;
				std::vector<uint8_t> ps;
				std::vector<uint8_t> cs;
				D3D12_GRAPHICS_PIPELINE_STATE_DESC gpstate_desc = {};
				D3D12_COMPUTE_PIPELINE_STATE_DESC cpstate_desc = {};
				D3D12_INPUT_ELEMENT_DESC layout[] = {
					{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				};

				//Depth
				gpstate_desc.DepthStencilState.DepthEnable = c.set_shader.is_enable_depth ? TRUE : FALSE;
				gpstate_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				gpstate_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

				//IA
				gpstate_desc.InputLayout.pInputElementDescs = layout;
				gpstate_desc.InputLayout.NumElements = _countof(layout);

				//Blend
				for (auto & bs : gpstate_desc.BlendState.RenderTarget) {
					bs.BlendEnable = FALSE;
					bs.LogicOpEnable = FALSE;
					bs.SrcBlend = D3D12_BLEND_SRC_ALPHA;
					bs.DestBlend = D3D12_BLEND_INV_DEST_ALPHA;
					bs.BlendOp = D3D12_BLEND_OP_ADD;
					bs.SrcBlendAlpha = D3D12_BLEND_ONE;
					bs.DestBlendAlpha = D3D12_BLEND_ZERO;
					bs.BlendOpAlpha = D3D12_BLEND_OP_ADD;
					bs.LogicOp = D3D12_LOGIC_OP_XOR;
					bs.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				}
				gpstate_desc.pRootSignature = rootsig;
				gpstate_desc.NumRenderTargets = _countof(gpstate_desc.BlendState.RenderTarget);
				gpstate_desc.VS = create_shader_from_file(std::string(name + ".hlsl"), "VSMain", "vs_5_0", vs);
				gpstate_desc.GS = create_shader_from_file(std::string(name + ".hlsl"), "GSMain", "gs_5_0", gs);
				gpstate_desc.PS = create_shader_from_file(std::string(name + ".hlsl"), "PSMain", "ps_5_0", ps);
				gpstate_desc.SampleDesc.Count = 1;
				gpstate_desc.SampleMask = UINT_MAX;
				gpstate_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				gpstate_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				gpstate_desc.RasterizerState.DepthClipEnable = TRUE;
				gpstate_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				for (auto & fmt : gpstate_desc.RTVFormats)
					fmt = fmt_color;

				cpstate_desc.pRootSignature = rootsig;
				cpstate_desc.CS = create_shader_from_file(std::string(name + ".hlsl"), "CSMain", "cs_5_0", cs);

				if (!vs.empty() && !ps.empty()) {
					auto status = dev->CreateGraphicsPipelineState(&gpstate_desc, IID_PPV_ARGS(&pstate));
					if (pstate)
						mpstate[name] = pstate;
					else
						err_printf("CreateGraphicsPipelineState : %s : status=0x%08X\n", name.c_str(), status);
				}

				if (!cs.empty()) {
					auto status = dev->CreateComputePipelineState(&cpstate_desc, IID_PPV_ARGS(&pstate));
					if (pstate)
						mpstate[name] = pstate;
					else
						err_printf("CreateComputePipelineState : %s : status=0x%08X\n", name.c_str(), status);
				}
			}
			if (pstate) {
				ref.cmdlist->SetPipelineState(pstate);
			} else {
				err_printf("Compile Error %s\n", name.c_str());
				Sleep(500);
			}
		}

		//CMD_CLEAR
		if (type == CMD_CLEAR) {
			auto cpu_handle = heap_rtv->GetCPUDescriptorHandleForHeapStart();
			auto index = mcpu_handle[name];
			cpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * index;
			ref.cmdlist->ClearRenderTargetView(cpu_handle, c.clear.color, 0, NULL);
		}

		//CMD_CLEAR_
		if (type == CMD_CLEAR_DEPTH) {
			auto cpu_handle = heap_dsv->GetCPUDescriptorHandleForHeapStart();
			auto index = mcpu_handle[oden_get_depth_render_target_name(name)];
			cpu_handle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * index;
			ref.cmdlist->ClearDepthStencilView(cpu_handle, D3D12_CLEAR_FLAG_DEPTH, c.clear_depth.value, 0, 0, NULL);
		}

		//CMD_DRAW_INDEX
		if (type == CMD_DRAW_INDEX) {
			auto count = c.draw_index.count;
			ref.cmdlist->DrawIndexedInstanced(count, 1, 0, 0, 0);
		}

		//CMD_DRAW
		if (type == CMD_DRAW) {
			auto vertex_count = c.draw.vertex_count;
			ref.cmdlist->DrawInstanced(vertex_count, 1, 0, 0);
		}

		//CMD_DISPATCH
		if (type == CMD_DISPATCH) {
			auto x = c.dispatch.x;
			auto y = c.dispatch.y;
			auto z = c.dispatch.z;
			ref.cmdlist->Dispatch(x, y, z);
		}
	}
	for (auto & tb : mbarrier) {
		D3D12_RESOURCE_BARRIER barrier = get_barrier(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
		barrier.Transition = tb.second;
		barrier.Transition.pResource = mres[tb.first];
		ref.cmdlist->ResourceBarrier(1, &barrier);
	}
	ref.cmdlist->Close();
	ID3D12CommandList *pplists[] = {
		ref.cmdlist,
	};
	queue->ExecuteCommandLists(1, pplists);
	swapchain->Present(1, 0);
	frame_count++;
}
