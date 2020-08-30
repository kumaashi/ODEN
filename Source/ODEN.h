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

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>
#include <vector>
#include <algorithm>

namespace oden
{

enum {
	CMD_NOP,
	CMD_SET_BARRIER,
	CMD_SET_RENDER_TARGET,
	CMD_SET_TEXTURE,
	CMD_SET_TEXTURE_UAV,
	CMD_SET_VERTEX,
	CMD_SET_INDEX,
	CMD_SET_CONSTANT,
	CMD_SET_SHADER,
	CMD_CLEAR,
	CMD_CLEAR_DEPTH,
	CMD_DRAW_INDEX,
	CMD_DRAW,
	CMD_DISPATCH,
	CMD_MAX,
};

struct cmd {
	int type;
	std::string name;
	struct rect_t {
		int x, y, w, h;
	};
	union {
		struct set_barrier_t {
			bool to_present;
			bool to_rendertarget;
			bool to_texture;
		} set_barrier;

		struct set_render_target_t {
			int fmt;
			rect_t rect;
			bool is_backbuffer;
		} set_render_target;

		struct set_depth_render_target_t {
			int fmt;
			rect_t rect;
		} set_depth_render_target;

		struct set_texture_t {
			int fmt;
			int slot;
			void *data;
			size_t size;
			size_t stride_size;
			rect_t rect;
			int miplevel;
		} set_texture;

		struct set_vertex_t {
			void *data;
			size_t size;
			size_t stride_size;
		} set_vertex;

		struct set_index_t {
			void *data;
			size_t size;
		} set_index;

		struct set_constant_t {
			int slot;
			void *data;
			size_t size;
		} set_constant;

		struct set_shader_t {
			bool is_update;
			bool is_cull;
			bool is_enable_depth;
		} set_shader;


		struct clear_t {
			float color[4];
		} clear;

		struct clear_depth_t {
			float value;
		} clear_depth;

		struct draw_index_t {
			int start;
			int count;
		} draw_index;

		struct draw_t {
			int vertex_count;
		} draw;

		struct dispatch_t {
			int x, y, z;
		} dispatch;
	};
};

void
oden_present_graphics(const char * appname, std::vector<cmd> & vcmd,
	void *handle, uint32_t w, uint32_t h,
	uint32_t buffernum, uint32_t heapcount, uint32_t slotmax);

inline std::string
oden_get_backbuffer_name(int index)
{
	return std::string("__backbuffer__") + std::to_string(index);
}

inline std::string
oden_get_mipmap_name(std::string name, int index)
{
	return name + "_miplevel_" + std::to_string(index);
}

inline int
oden_get_mipmap_max(int w, int h)
{
	int len = (std::min)(w, h);
	int maxmips = 0;
	while (len) {
		maxmips++;
		len >>= 1;
	}
	return maxmips;
}

inline std::string
oden_get_depth_render_target_name(std::string name)
{
	return name + "_depth";
}

inline const char *
oden_get_cmd_name(int c)
{
	if (c == CMD_NOP)
		return "CMD_NOP";
	if (c == CMD_SET_BARRIER)
		return "CMD_SET_BARRIER";
	if (c == CMD_SET_RENDER_TARGET)
		return "CMD_SET_RENDER_TARGET";
	if (c == CMD_SET_TEXTURE)
		return "CMD_SET_TEXTURE";
	if (c == CMD_SET_TEXTURE_UAV)
		return "CMD_SET_TEXTURE_UAV";
	if (c == CMD_SET_VERTEX)
		return "CMD_SET_VERTEX";
	if (c == CMD_SET_INDEX)
		return "CMD_SET_INDEX";
	if (c == CMD_SET_CONSTANT)
		return "CMD_SET_CONSTANT";
	if (c == CMD_SET_SHADER)
		return "CMD_SET_SHADER";
	if (c == CMD_CLEAR)
		return "CMD_CLEAR";
	if (c == CMD_CLEAR_DEPTH)
		return "CMD_CLEAR_DEPTH";
	if (c == CMD_DRAW_INDEX)
		return "CMD_DRAW_INDEX";
	if (c == CMD_DRAW)
		return "CMD_DRAW";
	if (c == CMD_DISPATCH)
		return "CMD_DISPATCH";
	return "__CMD_UNKNOWN__";
}

} //oden
