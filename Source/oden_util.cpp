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

#include "oden_util.h"

namespace odenutil
{

void SetBarrierToPresent(std::vector<cmd> & vcmd, std::string name)
{
	cmd c;
	c.type = CMD_SET_BARRIER;
	c.name = name;
	c.set_barrier.to_present = true;
	c.set_barrier.to_rendertarget = false;
	c.set_barrier.to_texture = false;
	vcmd.push_back(c);
}

void SetBarrierToRenderTarget(std::vector<cmd> & vcmd, std::string name)
{
	cmd c;
	c.type = CMD_SET_BARRIER;
	c.name = name;
	c.set_barrier.to_present = false;
	c.set_barrier.to_rendertarget = true;
	c.set_barrier.to_texture = false;
	vcmd.push_back(c);
}

void SetBarrierToTexture(std::vector<cmd> & vcmd, std::string name)
{
	cmd c;
	c.type = CMD_SET_BARRIER;
	c.name = name;
	c.set_barrier.to_present = false;
	c.set_barrier.to_rendertarget = false;
	c.set_barrier.to_texture = true;
	vcmd.push_back(c);
}

void SetRenderTarget(std::vector<cmd> & vcmd, std::string name,
	int w, int h)
{
	SetBarrierToRenderTarget(vcmd, name);

	cmd c;
	c.type = CMD_SET_RENDER_TARGET;
	c.name = name;
	c.set_render_target.fmt = 0;
	c.set_render_target.rect.x = 0;
	c.set_render_target.rect.y = 0;
	c.set_render_target.rect.w = w;
	c.set_render_target.rect.h = h;
	vcmd.push_back(c);
}

void
SetTexture(
	std::vector<cmd> & vcmd, std::string name,
	int slot, int w, int h, void *data, size_t size, size_t stride_size)
{
	SetBarrierToTexture(vcmd, name);

	cmd c;
	c.type = CMD_SET_TEXTURE;
	c.name = name;
	c.set_texture.fmt = 0;
	c.set_texture.slot = slot;
	c.set_texture.data = data;
	c.set_texture.size = size;
	c.set_texture.stride_size = stride_size;
	c.set_texture.rect.x = 0;
	c.set_texture.rect.y = 0;
	c.set_texture.rect.w = w;
	c.set_texture.rect.h = h;
	vcmd.push_back(c);
}

void
SetTextureUav(
	std::vector<cmd> & vcmd, std::string name,
	int slot, int w, int h, int miplevel,
	void *data, size_t size, size_t stride_size)
{
	SetBarrierToTexture(vcmd, name);

	cmd c;
	c.type = CMD_SET_TEXTURE_UAV;
	c.name = name;
	c.set_texture.fmt = 0;
	c.set_texture.slot = slot;
	c.set_texture.data = data;
	c.set_texture.size = size;
	c.set_texture.stride_size = stride_size;
	c.set_texture.rect.x = 0;
	c.set_texture.rect.y = 0;
	c.set_texture.rect.w = w;
	c.set_texture.rect.h = h;
	c.set_texture.miplevel = miplevel;
	vcmd.push_back(c);
}

void SetVertex(std::vector<cmd> & vcmd, std::string name,
	void *data, size_t size, size_t stride_size)
{
	cmd c;
	c.type = CMD_SET_VERTEX;
	c.name = name;
	c.set_vertex.data = data;
	c.set_vertex.size = size;
	c.set_vertex.stride_size = stride_size;
	vcmd.push_back(c);
}

void SetIndex(std::vector<cmd> & vcmd, std::string name,
	void *data, size_t size)
{
	cmd c;
	c.type = CMD_SET_INDEX;
	c.name = name;
	c.set_index.data = data;
	c.set_index.size = size;
	vcmd.push_back(c);
}

void SetConstant(std::vector<cmd> & vcmd, std::string name,
	int slot, void *data, size_t size)
{
	cmd c;
	c.type = CMD_SET_CONSTANT;
	c.name = name;
	c.set_constant.slot = slot;
	c.set_constant.data = data;
	c.set_constant.size = size;
	vcmd.push_back(c);
}

void SetShader(
	std::vector<cmd> & vcmd, std::string name,
	bool is_update, bool is_cull, bool is_enable_depth)
{
	cmd c;
	c.type = CMD_SET_SHADER;
	c.name = name;
	c.set_shader.is_update = is_update;
	c.set_shader.is_cull = is_cull;
	c.set_shader.is_enable_depth = is_enable_depth;
	vcmd.push_back(c);
}

void ClearRenderTarget(std::vector<cmd> & vcmd, std::string name,
	float col[4])
{
	cmd c;
	c.type = CMD_CLEAR;
	c.name = name;
	for (int i = 0 ; i < 4; i++)
		c.clear.color[i] = col[i];
	vcmd.push_back(c);
}

void ClearDepthRenderTarget(std::vector<cmd> & vcmd, std::string name,
	float value)
{
	cmd c;
	c.type = CMD_CLEAR_DEPTH;
	c.name = name;
	c.clear_depth.value = value;
	vcmd.push_back(c);
}

void DrawIndex(std::vector<cmd> & vcmd, std::string name,
	int start, int count)
{
	cmd c;
	c.type = CMD_DRAW_INDEX;
	c.name = name;
	c.draw_index.start = start;
	c.draw_index.count = count;
	vcmd.push_back(c);
}

void Draw(std::vector<cmd> & vcmd, std::string name,
	int vertex_count)
{
	cmd c;
	c.type = CMD_DRAW;
	c.name = name;
	c.draw.vertex_count = vertex_count;
	vcmd.push_back(c);
}

void Dispatch(std::vector<cmd> & vcmd, std::string name,
	int x, int y, int z)
{
	cmd c;
	c.type = CMD_DISPATCH;
	c.name = name;
	c.dispatch.x = x;
	c.dispatch.y = y;
	c.dispatch.z = z;
	vcmd.push_back(c);
}

void DebugPrint(std::vector<cmd> & vcmd)
{
	printf("%s ================================\n", __func__);
	for (auto & c : vcmd) {
		printf("%s : ", c.name.c_str());
		auto type = c.type;
		switch (type) {
		case CMD_NOP:
			printf("CMD_NOP\n");
			break;
		case CMD_SET_BARRIER:
			printf("CMD_SET_BARRIER\n");
			break;
		case CMD_SET_RENDER_TARGET:
			printf("CMD_SET_RENDER_TARGET\n");
			break;
		case CMD_SET_TEXTURE:
			printf("CMD_SET_TEXTURE\n");
			break;
		case CMD_SET_VERTEX:
			printf("CMD_SET_VERTEX\n");
			break;
		case CMD_SET_INDEX:
			printf("CMD_SET_INDEX\n");
			break;
		case CMD_SET_CONSTANT:
			printf("CMD_SET_CONSTANT\n");
			break;
		case CMD_SET_SHADER:
			printf("CMD_SET_SHADER\n");
			break;
		case CMD_CLEAR:
			printf("CMD_CLEAR\n");
			break;
		case CMD_CLEAR_DEPTH:
			printf("CMD_CLEAR_DEPTH\n");
			break;
		case CMD_DRAW_INDEX:
			printf("CMD_DRAW_INDEX\n");
			break;
		case CMD_DRAW:
			printf("CMD_DRAW\n");
			break;
		default:
			printf("CMD_UNKNOWN %d\n", type);
			break;
		}
	}
}

};
