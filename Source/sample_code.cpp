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

#include <stdio.h>
#include <windows.h>
#include <map>
#include <vector>
#include <string>
#include <vector>
#include <algorithm>

#include "oden_util.h"

#include "MatrixStack.h"
#include "Win.h"


struct matrix4x4 {
	float data[16];
};

struct vector4 {
	union {
		struct {
			float x, y, z, w;
		};
		float data[4];
	};
	void print()
	{
		printf("%.5f, %.5f, %.5f, %.5f\n", x, y, z, w);
	}
};

struct vector3 {
	float x, y, z;
	void print()
	{
		printf("%.5f, %.5f, %.5f\n", x, y, z);
	}
};

struct vector2 {
	float x, y;
	void print()
	{
		printf("%.5f, %.5f\n", x, y);
	}
};

struct vertex_format {
	vector4 pos;
	vector3 nor;
	vector2 uv;
};

struct constdata {
	vector4 time;
	vector4 misc;
	matrix4x4 world;
	matrix4x4 proj;
	matrix4x4 view;
	uint32_t matid[4];
};


void
GenerateMipmap(std::vector<oden::cmd> & vcmd, std::string name, int w, int h)
{
	using namespace odenutil;
	GenMipmap(vcmd, name);
}

int main()
{
	using namespace oden;
	using namespace odenutil;
	enum {
		Width = 1280,
		Height = 720,
		BloomWidth = Width >> 2,
		BloomHeight = Height >> 2,

		BufferMax = 2,
		ResourceMax = 1024,
		ShaderSlotMax = 1024,

		TextureHeight = 256,
		TextureWidth = 256,
	};

	vertex_format vtx_rect[] = {
		{{-1, 1, 0, 1}, {0,  1,  1}, { 0, 1}},
		{{-1, -1, 0, 1}, {0,  1,  1}, { 0, 0}},
		{{ 1, 1, 0, 1}, {0,  1,  1}, { 1, 1}},
		{{ 1, -1, 0, 1}, {0,  1,  1}, { 1, 0}},
	};
	uint32_t idx_rect[] = {
		0, 1, 2,
		2, 1, 3
	};

	vertex_format vtx_cube[] = {
		{{-1, -1,  1, 1}, {0, 0, -1}, {-1, -1}},
		{{ 1, -1,  1, 1}, {0, 0, -1}, { 1, -1}},
		{{ 1,  1,  1, 1}, {0, 0, -1}, { 1,  1}},
		{{-1,  1,  1, 1}, {0, 0, -1}, {-1,  1}},
		{{-1, -1, -1, 1}, {0, 0,  1}, {-1, -1}},
		{{ 1, -1, -1, 1}, {0, 0,  1}, { 1, -1}},
		{{ 1,  1, -1, 1}, {0, 0,  1}, { 1,  1}},
		{{-1,  1, -1, 1}, {0, 0,  1}, {-1,  1}},
	};
	uint32_t idx_cube[] = {
		// front
		0, 1, 2,
		2, 3, 0,
		// top
		3, 2, 6,
		6, 7, 3,
		// back
		7, 6, 5,
		5, 4, 7,
		// bottom
		4, 5, 1,
		1, 0, 4,
		// left
		4, 0, 3,
		3, 7, 4,
		// right
		1, 5, 6,
		6, 2, 1,
	};

	auto app_name = "oden_sample_code";
	auto hwnd = InitWindow(app_name, Width, Height);
	int index = 0;
	static std::vector<uint32_t> vtex;
	for (int y = 0  ; y < TextureHeight; y++) {
		for (int x = 0  ; x < TextureWidth; x++) {
			vtex.push_back((x ^ y) * 1110);
		}
	}


	constdata cdata {};
	constdata binfoX {};
	constdata binfoY {};

	MatrixStack stack;
	std::vector<cmd> vcmd;

	auto tex_name = "testtex";
	uint64_t frame = 0;
	while (Update()) {
		auto buffer_index = frame % BufferMax;
		auto index_name = std::to_string(buffer_index);
		auto backbuffer_name = oden_get_backbuffer_name(buffer_index);
		auto offscreen_name = "offscreen" + index_name;
		auto offscreen_depth_name = oden_get_depth_render_target_name(offscreen_name);
		auto constant_name = "constcommon" + index_name;
		auto constmip_name = "constmip" + index_name;
		auto constbloom_name = "constbloom" + index_name;
		auto bloomscreen_name = "bloomtexture" + index_name;
		auto bloomscreen_nameX = bloomscreen_name + index_name + "_X";

		bool is_update = false;

		if (GetAsyncKeyState(VK_F5) & 0x0001) {
			is_update = true;
		}

		cdata.time.data[0] = float (frame) / 1000.0f;
		cdata.time.data[1] = 0.0;
		cdata.time.data[2] = 1.0;
		cdata.time.data[3] = 1.0;

		//matrix : world
		stack.Reset();
		stack.Scaling(16, 16, 16);
		stack.GetTop(cdata.world.data);

		//matrix : view
		stack.Reset();
		float tm = float (frame) * 0.01;
		float rad = 64.0f;
		float height = 64.0f;
		stack.LoadLookAt(
			rad * cos(tm), height * sin(tm * 0.3), rad * sin(tm * 0.8),
			0, 0, 0,
			0, 1, 0);
		stack.GetTop(cdata.view.data);

		//matrix : proj
		stack.Reset();
		stack.LoadPerspective((3.141592653f / 180.0f) * 90.0f, float (Width) / float (Height), 0.5f, 1024.0f);
		stack.GetTop(cdata.proj.data);

		//Clear offscreenbuffer.
		float clear_color[] = {0, 1, 1, 1};
		float clear_color_present[2][4] = {
			{0, 1, 1, 1},
			{1, 1, 1, 1},
		};
		SetRenderTarget(vcmd, offscreen_name, Width, Height);
		SetShader(vcmd, "./shaders/clear", is_update, false, false);
		ClearRenderTarget(vcmd, offscreen_name, clear_color);
		ClearDepthRenderTarget(vcmd, offscreen_name, 1.0f);
		SetVertex(vcmd, "clear_vb", vtx_rect, sizeof(vtx_rect), sizeof(vertex_format));
		SetIndex(vcmd, "clear_ib", idx_rect, sizeof(idx_rect));
		SetConstant(vcmd, constant_name, 0, &cdata, sizeof(cdata));
		DrawIndex(vcmd, "clear_draw", 0, _countof(idx_rect), 0);

		//Draw Cube to offscreenbuffer.
		cdata.misc.data[0] = 0.0;
		ClearDepthRenderTarget(vcmd, offscreen_name, 1.0f);
		SetShader(vcmd, "./shaders/model", is_update, false, true);
		SetVertex(vcmd, "cube_vb", vtx_cube, sizeof(vtx_cube), sizeof(vertex_format));
		SetIndex(vcmd, "cube_ib", idx_cube, sizeof(idx_cube));
		SetTexture(vcmd, tex_name, 0, TextureWidth, TextureHeight, vtex.data(), vtex.size() * sizeof(uint32_t), 256 * sizeof(uint32_t));
		SetConstant(vcmd, constant_name, 1, &cdata, sizeof(cdata));
		DrawIndex(vcmd, "cube_draw", 0, _countof(idx_cube), 1);

		cdata.misc.data[0] = 1.0;
		SetShader(vcmd, "./shaders/model", is_update, false, true);
		SetTexture(vcmd, tex_name, 0);
		SetConstant(vcmd, constant_name + "head", 2, &cdata, sizeof(cdata));
		DrawIndex(vcmd, "cube_draw", 0, _countof(idx_cube), 2);

		GenMipmap(vcmd, offscreen_name);

		//Create Bloom X
		SetRenderTarget(vcmd, bloomscreen_nameX, BloomWidth, BloomHeight);
		SetShader(vcmd, "./shaders/bloom", is_update, false, false);
		ClearRenderTarget(vcmd, bloomscreen_nameX, clear_color);
		ClearDepthRenderTarget(vcmd, bloomscreen_nameX, 1.0f);
		SetVertex(vcmd, "present_vb", vtx_rect, sizeof(vtx_rect), sizeof(vertex_format));
		SetIndex(vcmd, "present_ib", idx_rect, sizeof(idx_rect));
		SetTexture(vcmd, offscreen_name, 1);
		binfoX.misc.x = BloomWidth;
		binfoX.misc.y = BloomHeight;
		binfoX.misc.z = 1.0;
		binfoX.misc.w = 0.0;
		binfoX.matid[0] = 1;
		SetConstant(vcmd, constbloom_name + "X", 3, &binfoX, sizeof(binfoX));
		DrawIndex(vcmd, "bloomX", 0, _countof(idx_rect), 3);
		GenMipmap(vcmd, bloomscreen_nameX);

		//Create Bloom Y
		SetRenderTarget(vcmd, bloomscreen_name, BloomWidth, BloomHeight);
		SetShader(vcmd, "./shaders/bloom", is_update, false, false);
		ClearRenderTarget(vcmd, bloomscreen_name, clear_color);
		ClearDepthRenderTarget(vcmd, bloomscreen_name, 1.0f);
		SetTexture(vcmd, bloomscreen_nameX, 2);
		SetVertex(vcmd, "present_vb", vtx_rect, sizeof(vtx_rect), sizeof(vertex_format));
		SetIndex(vcmd, "present_ib", idx_rect, sizeof(idx_rect));
		binfoY.misc.x = BloomWidth;
		binfoY.misc.y = BloomHeight;
		binfoY.misc.z = 0.0;
		binfoY.misc.w = 1.0;
		binfoY.matid[0] = 2;
		SetConstant(vcmd, constbloom_name + "Y", 4, &binfoY, sizeof(binfoY));
		DrawIndex(vcmd, "bloomY", 0, _countof(idx_rect), 4);
		GenMipmap(vcmd, bloomscreen_name);

		//Draw offscreen buffer to present buffer.
		SetRenderTarget(vcmd, backbuffer_name, Width, Height, true);
		SetShader(vcmd, "./shaders/present", is_update, false, false);
		ClearRenderTarget(vcmd, backbuffer_name, clear_color_present[frame & 1]);
		ClearDepthRenderTarget(vcmd, backbuffer_name, 1.0f);
		SetTexture(vcmd, offscreen_name, 1);
		SetTexture(vcmd, bloomscreen_name, 3);
		SetVertex(vcmd, "present_vb", vtx_rect, sizeof(vtx_rect), sizeof(vertex_format));
		SetIndex(vcmd, "present_ib", idx_rect, sizeof(idx_rect));
		DrawIndex(vcmd, "present_draw", 0, _countof(idx_rect), 5);

		//Draw Depth
		SetRenderTarget(vcmd, backbuffer_name, Width, Height, true);
		SetShader(vcmd, "./shaders/showdepth", is_update, false, false);
		SetTexture(vcmd, offscreen_depth_name, 4);
		SetVertex(vcmd, "present_vb", vtx_rect, sizeof(vtx_rect), sizeof(vertex_format));
		SetIndex(vcmd, "present_ib", idx_rect, sizeof(idx_rect));
		DrawIndex(vcmd, "present_draw", 0, _countof(idx_rect), 6);

		//Present CMD to ODEN.
		SetBarrierToPresent(vcmd, backbuffer_name);
		oden_present_graphics(app_name, vcmd, hwnd, Width, Height, BufferMax, ResourceMax, ShaderSlotMax);

		vcmd.clear();
		frame++;
	}

	//Terminate Oden.
	oden_present_graphics(app_name, vcmd, nullptr, Width, Height, BufferMax, ResourceMax, ShaderSlotMax);
	return 0;
}

