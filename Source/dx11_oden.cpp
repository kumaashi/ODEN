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
#include <d3d11.h>
#include <d3dcommon.h>
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
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

#pragma warning(disable:4838)

#define info_printf(...) printf("INFO:" __FUNCTION__ ":" __VA_ARGS__)
#define err_printf(...) printf("ERR:" __FUNCTION__ ":" __VA_ARGS__)

static HRESULT
CompileShaderFromFile(std::string name,
	LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	ID3DBlob* perrblob = NULL;
	std::vector<WCHAR> wfname;
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
	for (int i = 0; i < name.length(); i++)
		wfname.push_back(name[i]);
	wfname.push_back(0);
	hr = D3DCompileFromFile(&wfname[0], NULL,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			szEntryPoint, szShaderModel, flags, 0, ppBlobOut, &perrblob);
	if (FAILED(hr)) {
		err_printf("name=%s: %s\n", name.c_str(), perrblob ? perrblob->GetBufferPointer() : "UNKNOWN");
	}
	if (perrblob)
		perrblob->Release();
	return hr;
}

void
oden::oden_present_graphics(const char * appname, std::vector<oden::cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t num, uint32_t heapcount, uint32_t slotmax)
{
	using namespace oden;

	HWND hwnd = (HWND) handle;
	static ID3D11Device *dev  = NULL;
	static ID3D11DeviceContext *ctx = NULL;
	static IDXGISwapChain *swapchain = NULL;

	struct PipelineState {
		ID3D11VertexShader *vs = NULL;
		ID3D11GeometryShader *gs = NULL;
		ID3D11PixelShader *ps = NULL;
		ID3D11ComputeShader *cs = NULL;
		ID3D11InputLayout *layout = NULL;
		ID3D11DepthStencilState *dsstate = NULL;
	};
	static std::map<std::string, ID3D11RenderTargetView *> mrtv;
	static std::map<std::string, ID3D11ShaderResourceView *> msrv;
	static std::map<std::string, ID3D11DepthStencilView *> mdsv;
	static std::map<std::string, ID3D11UnorderedAccessView *> muav;

	static std::map<std::string, ID3D11Texture2D *> mtex;
	static std::map<std::string, ID3D11Buffer *> mbuf;
	static std::map<std::string, PipelineState> mpstate;
	static ID3D11SamplerState * sampler_state_point = NULL;
	static ID3D11SamplerState * sampler_state_linear = NULL;
	static ID3D11RasterizerState * rsstate = NULL;
	static uint64_t device_index = 0;
	static uint64_t frame_count = 0;

	if (dev == nullptr) {
		DXGI_SWAP_CHAIN_DESC d3dsddesc = {
			{
				w, h, { 60, 1 }, DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
				DXGI_MODE_SCALING_UNSPECIFIED,
			}, {1, 0},
			DXGI_USAGE_RENDER_TARGET_OUTPUT,
			num, hwnd, TRUE, DXGI_SWAP_EFFECT_DISCARD,
			DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
		};

		D3D11CreateDeviceAndSwapChain(
			NULL, D3D_DRIVER_TYPE_HARDWARE,
			NULL, 0, NULL, 0, D3D11_SDK_VERSION,
			&d3dsddesc, &swapchain, &dev, NULL, &ctx);
		ID3D11Texture2D *backtex = nullptr;
		ID3D11RenderTargetView *backrtv = nullptr;
		swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			(LPVOID *) &backtex);
		dev->CreateRenderTargetView(backtex, NULL, &backrtv);
		for (uint32_t i = 0; i < num; i++) {
			auto name = oden_get_backbuffer_name(i);
			mtex[name] = backtex;
			mrtv[name] = backrtv;
		}

		D3D11_SAMPLER_DESC sampler_desc = {
			D3D11_FILTER_MIN_MAG_MIP_POINT,
			D3D11_TEXTURE_ADDRESS_WRAP,
			D3D11_TEXTURE_ADDRESS_WRAP,
			D3D11_TEXTURE_ADDRESS_WRAP,
			0, 1, D3D11_COMPARISON_NEVER,
			{0.0f, 0.0f, 0.0f, 0.0f}, -D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX
		};
		dev->CreateSamplerState(&sampler_desc, &sampler_state_point);
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		dev->CreateSamplerState(&sampler_desc, &sampler_state_linear);

		D3D11_RASTERIZER_DESC rsstate_desc = {};
		rsstate_desc.FillMode = D3D11_FILL_SOLID;
		rsstate_desc.CullMode = D3D11_CULL_NONE;
		rsstate_desc.FrontCounterClockwise = TRUE;
		rsstate_desc.DepthClipEnable = TRUE;
		rsstate_desc.ScissorEnable = TRUE;
		dev->CreateRasterizerState(&rsstate_desc, &rsstate);
	};

	if (hwnd == nullptr) {
		auto release = [](auto & a, const char * name = nullptr) {
			if (a) {
				printf("release : %p : name=%s\n",
					a, name ? name : "noname");
				a->Release();
			}
			a = nullptr;
		};
		auto mrelease = [&](auto & m) {
			for (auto & p : m)
				release(p.second, p.first.c_str());
		};

		mrelease(muav);
		mrelease(mdsv);
		mrelease(mrtv);
		mrelease(mtex);
		mrelease(mbuf);
		for (auto & p : mpstate) {
			release(p.second.vs, (p.first + ": VS").c_str());
			release(p.second.gs, (p.first + ": GS").c_str());
			release(p.second.ps, (p.first + ": PS").c_str());
			release(p.second.layout, (p.first + ": IA").c_str());
		}
		release(sampler_state_point);
		release(sampler_state_linear);
		release(rsstate);
		release(swapchain);
		release(ctx);
		release(dev);
		return;
	}

	ctx->IASetInputLayout(NULL);
	ctx->VSSetShader(NULL, NULL, 0);
	ctx->GSSetShader(NULL, NULL, 0);
	ctx->PSSetShader(NULL, NULL, 0);
	ctx->CSSetShader(NULL, NULL, 0);
	ctx->VSSetSamplers(0, 1, &sampler_state_point);
	ctx->PSSetSamplers(0, 1, &sampler_state_point);
	ctx->VSSetSamplers(1, 1, &sampler_state_linear);
	ctx->PSSetSamplers(1, 1, &sampler_state_linear);
	ctx->RSSetState(rsstate);

	for (auto & c : vcmd) {
		auto type = c.type;
		auto name = c.name;

		auto fmt_color = DXGI_FORMAT_R16G16B16A16_FLOAT;
		auto fmt_depth = DXGI_FORMAT_D32_FLOAT;

		//CMD_SET_RENDER_TARGET
		if (type == CMD_SET_RENDER_TARGET) {
			auto name_depth = oden_get_depth_render_target_name(name);
			auto tex = mtex[name];
			if (tex == nullptr) {
				int maxmips = oden_get_mipmap_max(c.set_render_target.rect.w, c.set_render_target.rect.h);
				D3D11_TEXTURE2D_DESC desc = {
					c.set_render_target.rect.w, c.set_render_target.rect.h, maxmips, 1, fmt_color, {1, 0},
					D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0,  0,
				};
				dev->CreateTexture2D(&desc, NULL, &tex);
				if (tex) {
					mtex[name] = tex;
				} else {
					err_printf("CMD_SET_RENDER_TARGET name=%s, tex=%p, maxmips=%d\n", name.c_str(), tex, maxmips);
					exit(1);
				}
				info_printf("CreateTexture2D(rtv) name=%s, tex=%p, maxmips=%d\n", name.c_str(), tex, maxmips);
			}
			auto tex_depth = mtex[name_depth];
			if (tex_depth == nullptr) {
				D3D11_TEXTURE2D_DESC desc = {
					c.set_render_target.rect.w, c.set_render_target.rect.h, 1, 1, DXGI_FORMAT_R32_TYPELESS, {1, 0},
					D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, 0,  0,
				};
				dev->CreateTexture2D(&desc, NULL, &tex_depth);
				if (tex_depth) {
					mtex[name_depth] = tex_depth;
				} else {
					err_printf("ERROR CMD_SET_RENDER_TARGET name_depth=%s, tex=%p\n", name_depth.c_str(), tex_depth);
					exit(1);
				}
				info_printf("CreateTexture2D(dsv) name=%s, tex_depth=%p\n", name_depth.c_str(), tex_depth);
			}
			auto rtv = mrtv[name];
			if (rtv == nullptr) {
				dev->CreateRenderTargetView(tex, nullptr, &rtv);
				info_printf("CreateRenderTargetView name=%s, rtv=%p\n", name.c_str(), rtv);
				if (rtv) {
					mrtv[name] = rtv;
				} else {
					err_printf("ERROR CMD_SET_RENDER_TARGET : CreateRenderTargetView name=%s, tex=%p\n", name.c_str(), tex);
					exit(1);
				}
			}

			auto dsv = mdsv[name];
			if (dsv == nullptr) {
				D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
				dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				dev->CreateDepthStencilView(tex_depth, &dsv_desc, &dsv);
				info_printf("CreateDepthStencilView name(dsv)=%s, dsv=%p\n", name.c_str(), dsv);
				if (dsv) {
					mdsv[name] = dsv;
				} else {
					err_printf("ERROR CMD_SET_RENDER_TARGET : CreateDepthStencilView name=%s, tex=%p\n", name.c_str(), tex);
					exit(1);
				}
			}
			auto vw = c.set_render_target.rect.w;
			auto vh = c.set_render_target.rect.h;
			D3D11_RECT rc = { 0, 0, vw, vh };
			D3D11_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT) vw, (FLOAT) vh, 0.0f, 1.0f, };
			ctx->RSSetScissorRects(1, &rc);
			ctx->RSSetViewports(1, &vp);
			ctx->OMSetRenderTargets(1, &rtv, dsv);
		}

		//CMD_SET_TEXTURE
		if (type == CMD_SET_TEXTURE || type == CMD_SET_TEXTURE_UAV) {
			auto slot = c.set_texture.slot;
			auto tex = mtex[name];
			auto srv = msrv[name];
			auto rtv = mrtv[name];
			auto dsv = mdsv[name];
			auto uav = muav[name];
			auto data = c.buf.data();
			auto size = c.buf.size();
			if (tex == nullptr) {
				fmt_color = DXGI_FORMAT_R8G8B8A8_UNORM;
				D3D11_TEXTURE2D_DESC desc = {
					c.set_texture.rect.w, c.set_texture.rect.h, 1, 1, fmt_color, {1, 0},
					D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0,  0,
				};
				D3D11_SUBRESOURCE_DATA initdata = {};
				initdata.pSysMem = data;
				initdata.SysMemPitch = c.set_texture.stride_size;
				initdata.SysMemSlicePitch = size;
				dev->CreateTexture2D(&desc, &initdata, &tex);
				info_printf("CreateTexture2D : name=%s, tex=%p\n", name.c_str(), tex);
				if (tex)
					mtex[name] = tex;
				else {
					err_printf("ERROR CMD_SET_TEXTURE name=%s, tex=%p\n", name.c_str(), tex);
					exit(1);
				}
			}

			D3D11_TEXTURE2D_DESC texdesc = {};
			tex->GetDesc(&texdesc);

			if (type == CMD_SET_TEXTURE) {
				if (srv == nullptr) {
					D3D11_SHADER_RESOURCE_VIEW_DESC desc = {
						fmt_color,
						D3D11_SRV_DIMENSION_TEXTURE2D,
						{0, 0},
					};
					desc.Texture2D.MipLevels = 1;
					if (rtv)
						desc.Texture2D.MipLevels = texdesc.MipLevels - 1;
					if (dsv)
						desc.Texture2D.MipLevels = texdesc.MipLevels - 1;

					if(name.find("depth") != std::string::npos) {
						desc.Format = fmt_depth;
						if(desc.Format == DXGI_FORMAT_D32_FLOAT)
							printf("!!!!! DXGI_FORMAT_D32_FLOAT\n");
						if(desc.Format == DXGI_FORMAT_R32_FLOAT)
							printf("!!!!! DXGI_FORMAT_R32_FLOAT\n");
						desc.Format = DXGI_FORMAT_R32_FLOAT;
					}
					info_printf("CMD_SET_TEXTURE name=%s, tex=%p, srv=%p, rtv=%p, miplevels=%d\n",
						name.c_str(), tex, srv, rtv, desc.Texture2D.MipLevels);
					dev->CreateShaderResourceView(tex, &desc, &srv);
					if (srv == nullptr) {
						err_printf("ERROR CMD_SET_TEXTURE CreateShaderResourceView name=%s, tex=%p\n",
							name.c_str(), tex);
						exit(1);
					}
					msrv[name] = srv;
					info_printf("CreateShaderResourceView : name=%s, srv=%p\n", name.c_str(), srv);
				}

				ID3D11UnorderedAccessView * uavnull[1] = { nullptr };
				ctx->CSSetUnorderedAccessViews(0, 1, uavnull, nullptr);
				if (srv) {
					ctx->VSSetShaderResources(slot, 1, &srv);
					ctx->PSSetShaderResources(slot, 1, &srv);
				} else {
					err_printf("CMD_SET_TEXTURE : name=%s, srv=%p\n", name.c_str(), srv);
					exit(1);
				}
			}

			if (type == CMD_SET_TEXTURE_UAV) {
				if (uav == nullptr && tex) {
					auto temp = uav;
					for (int i = 0 ; i < texdesc.MipLevels; i++) {
						D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
						desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
						desc.Texture2D.MipSlice = i;
						desc.Format = texdesc.Format;
						dev->CreateUnorderedAccessView(tex, &desc, &temp);
						info_printf("CreateUnorderedAccessView name=%s, uav=%p\n", name.c_str(), temp);

						if (temp) {
							muav[oden_get_mipmap_name(name, i)] = temp;
							if (i == 0)
								muav[name] = temp;
						} else {
							err_printf("CMD_SET_TEXTURE_UAV : CreateUnorderedAccessView : name=%s, uav=%p\n", name.c_str(), uav);
						}
					}
				}
				auto miplevel = c.set_texture.miplevel;
				auto mipname = oden_get_mipmap_name(name, miplevel);
				uav = muav[mipname];
				//printf("DEBUG CMD_SET_TEXTURE_UAV name=%s, uav=%p\n", mipname.c_str(), uav);
				ctx->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
			}
		}

		//CMD_SET_CONSTANT
		if (type == CMD_SET_CONSTANT) {
			auto slot = c.set_constant.slot;
			auto cb = mbuf[name];
			auto data = c.buf.data();
			auto size = c.buf.size();

			if (cb == nullptr) {
				D3D11_BUFFER_DESC bd = {
					size, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0
				};
				auto hr = dev->CreateBuffer(&bd, nullptr, &cb);
				info_printf("CreateBuffer : name=%s, cb=%p\n", name.c_str(), cb);
				if (cb) {
					mbuf[name] = cb;
				} else {
					err_printf("CreateBuffer : name=%s, cb=%p\n", name.c_str(), cb);
					exit(1);
				}
			}
			ctx->UpdateSubresource(cb, 0, NULL, data, 0, 0);
			ctx->VSSetConstantBuffers(slot, 1, &cb);
			ctx->PSSetConstantBuffers(slot, 1, &cb);
			ctx->CSSetConstantBuffers(slot, 1, &cb);
		}

		//CMD_SET_VERTEX
		if (type == CMD_SET_VERTEX) {
			auto vb = mbuf[name];
			auto data = c.buf.data();
			auto size = c.buf.size();

			if (vb == nullptr) {
				D3D11_BUFFER_DESC bd = {
					size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0
				};
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				auto hr = dev->CreateBuffer(&bd, nullptr, &vb);
				info_printf("CreateBuffer : name=%s, vb=%p\n", name.c_str(), vb);
				if (vb) {
					mbuf[name] = vb;
				} else {
					info_printf("CreateBuffer : name=%s, vb=%p\n", name.c_str(), vb);
					exit(0);
				}

				D3D11_MAPPED_SUBRESOURCE msr = {};
				ctx->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
				if (msr.pData) {
					memcpy(msr.pData, data, size);
					ctx->Unmap(vb, 0);
				} else {
					err_printf("ERROR CMD_SET_VERTEX name=%s Can't map\n", name.c_str());
				}
			}

			UINT stride = c.set_vertex.stride_size;
			UINT offset = 0;
			ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
			ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		//CMD_SET_SHADER
		if (type == CMD_SET_SHADER) {
			auto is_update = c.set_shader.is_update;
			auto pstate = mpstate[name];
			if (is_update) {
				if (pstate.dsstate) pstate.dsstate->Release();
				if (pstate.layout) pstate.layout->Release();
				if (pstate.vs) pstate.vs->Release();
				if (pstate.gs) pstate.gs->Release();
				if (pstate.ps) pstate.ps->Release();
				mpstate.erase(name);
				pstate = mpstate[name];
			}
			if (pstate.vs == nullptr && pstate.cs == nullptr) {
				ID3DBlob *pBlobVS = NULL;
				ID3DBlob *pBlobGS = NULL;
				ID3DBlob *pBlobPS = NULL;
				ID3DBlob *pBlobCS = NULL;
				CompileShaderFromFile(std::string(name + ".hlsl").c_str(), "VSMain", "vs_5_0", &pBlobVS);
				CompileShaderFromFile(std::string(name + ".hlsl").c_str(), "GSMain", "gs_5_0", &pBlobGS);
				CompileShaderFromFile(std::string(name + ".hlsl").c_str(), "PSMain", "ps_5_0", &pBlobPS);
				CompileShaderFromFile(std::string(name + ".hlsl").c_str(), "CSMain", "cs_5_0", &pBlobCS);

				if (pBlobVS)
					dev->CreateVertexShader(
						pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &pstate.vs);
				if (pBlobGS)
					dev->CreateGeometryShader(
						pBlobGS->GetBufferPointer(), pBlobGS->GetBufferSize(), NULL, &pstate.gs);
				if (pBlobPS)
					dev->CreatePixelShader(
						pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &pstate.ps);
				if (pBlobCS)
					dev->CreateComputeShader(
						pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &pstate.cs);

				if (pstate.vs) {
					D3D11_INPUT_ELEMENT_DESC layout[] = {
						{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
						{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
						{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					};
					dev->CreateInputLayout(
						layout, _countof(layout),
						pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), &pstate.layout);

					D3D11_DEPTH_STENCIL_DESC  dsstate_desc = {};
					dsstate_desc.DepthEnable = c.set_shader.is_enable_depth ? TRUE : FALSE;
					dsstate_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
					dsstate_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
					dsstate_desc.StencilEnable = FALSE;
					dsstate_desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
					dsstate_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
					dsstate_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
					dsstate_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
					dsstate_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
					dsstate_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
					dsstate_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
					dsstate_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
					dsstate_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
					dsstate_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
					dev->CreateDepthStencilState(&dsstate_desc, &pstate.dsstate);
				}
				info_printf("pstate.layout= %p\n", pstate.layout);
				info_printf("pstate.dsstate= %p\n", pstate.dsstate);
				info_printf("pstate.vs  = %p\n", pstate.vs);
				info_printf("pstate.ps  = %p\n", pstate.ps);
				info_printf("pstate.cs  = %p\n", pstate.cs);

				if (pstate.layout && pstate.vs && pstate.ps || pstate.cs) {
					mpstate[name] = pstate;
				} else {
					if (pstate.dsstate) pstate.dsstate->Release();
					if (pstate.layout) pstate.layout->Release();
					if (pstate.vs) pstate.vs->Release();
					if (pstate.gs) pstate.gs->Release();
					if (pstate.ps) pstate.ps->Release();
					if (pstate.cs) pstate.cs->Release();
					mpstate.erase(name);

					printf("Error SET_SHADER name=%s\n", name.c_str());
					Sleep(1000);
				}

				if (pBlobVS) pBlobVS->Release();
				if (pBlobGS) pBlobGS->Release();
				if (pBlobPS) pBlobPS->Release();
				if (pBlobCS) pBlobCS->Release();
			}
			if (pstate.vs) {
				ctx->OMSetDepthStencilState(pstate.dsstate, 0);
				ctx->IASetInputLayout(pstate.layout);
				ctx->VSSetShader(pstate.vs, NULL, 0);
				ctx->GSSetShader(pstate.gs, NULL, 0);
				ctx->PSSetShader(pstate.ps, NULL, 0);
				ctx->CSSetShader(NULL, NULL, 0);
			}
			if (pstate.cs) {
				info_printf("SET_SHADER CS : name=%s\n", name.c_str());
				ctx->CSSetShader(pstate.cs, NULL, 0);
			}
		}

		//CMD_CLEAR
		if (type == CMD_CLEAR) {
			auto rtv = mrtv[name];
			if (rtv)
				ctx->ClearRenderTargetView(rtv, c.clear.color);
			else
				printf("Error CMD_CLEAR name=%s not found\n", name.c_str());
		}

		//CMD_CLEAR_DEPTH
		if (type == CMD_CLEAR_DEPTH) {
			auto dsv = mdsv[name];
			if (dsv)
				ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, c.clear_depth.value, 0);
			else
				printf("Error CMD_CLEAR name=%s not found\n", name.c_str());
		}


		//CMD_SET_INDEX
		if (type == CMD_SET_INDEX) {
			auto ib = mbuf[name];
			auto data = c.buf.data();
			auto size = c.buf.size();
			if (size && ib == nullptr) {
				D3D11_BUFFER_DESC bd = {
					size, D3D11_USAGE_DYNAMIC, D3D11_BIND_INDEX_BUFFER, 0, 0, 0
				};
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				auto hr = dev->CreateBuffer(&bd, nullptr, &ib);
				mbuf[name] = ib;
				printf("name=%s, ib=%p size=%d\n", name.c_str(), ib, size);

				D3D11_MAPPED_SUBRESOURCE msr = {};
				ctx->Map(ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
				if (msr.pData) {
					memcpy(msr.pData, data, size);
					ctx->Unmap(ib, 0);
				} else {
					err_printf("ERROR CMD_SET_INDEX name=%s Can't map\n", name.c_str());
				}
			}
			if(size)
				ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
			else
				ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
		}

		//CMD_DRAW_INDEX
		if (type == CMD_DRAW_INDEX) {
			auto count = c.draw_index.count;
			ctx->DrawIndexedInstanced(count, 1, 0, 0, 0);
		}

		//CMD_DRAW
		if (type == CMD_DRAW) {
			auto count = c.draw.vertex_count;
			ctx->DrawInstanced(count, 1, 0, 0);
		}

		//CMD_DISPATCH
		if (type == CMD_DISPATCH) {
			auto x = c.dispatch.x;
			auto y = c.dispatch.y;
			auto z = c.dispatch.z;
			ctx->Dispatch(x, y, z);
		}
	}
	swapchain->Present(1, 0);
}

