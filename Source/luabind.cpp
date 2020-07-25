/*
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

extern "C" {
#include <lua.hpp>
}

#include "oden_util.h"
#include "Win.h"

#pragma warning(disable:4838)

namespace
{

union color {
	struct {
		uint8_t r, g, b, a;
	};
	void
	set(float x, float y, float z, float w) {
		r = uint8_t(x * float(0xFF));
		g = uint8_t(y * float(0xFF));
		b = uint8_t(z * float(0xFF));
		a = uint8_t(w * float(0xFF));
	}
	uint32_t rgba;
};

struct float4 {
	union {
		struct {
			float x, y, z, w;
		};
		float data[4];
	};

	void
	print()
	{
		printf("%.5f, %.5f, %.5f, %.5f\n", x, y, z, w);
	}
};

struct vector3 {
	union {
		struct {
			float x, y, z;
		};
		float data[3];
	};

	void
	print()
	{
		printf("%.5f, %.5f\n", x, y, z);
	}
};

struct vector2 {
	union {
		struct {
			float x, y;
		};
		float data[2];
	};

	void
	print()
	{
		printf("%.5f, %.5f\n", x, y);
	}
};

struct float4x4 {
	union {
		struct {
			float4 v[4];
		};
		float data[16];
	};

	void
	print()
	{
		printf("print:%p\n", this);
		for (auto & x : v)
			x.print();
	}
};

struct vertex_format {
	float4 pos;
	vector3 nor;
	vector2 uv;

	void
	print()
	{
		pos.print();
		nor.print();;
		uv.print();;
	}
};

template<typename T>
struct texture {
	std::vector<T> vbuf;
	int width, height;

	int
	get_width()
	{
		return width;
	}

	int
	get_height()
	{
		return height;
	}

	size_t
	get_size()
	{
		return width * height * sizeof(T);
	}

	size_t
	get_stride_size()
	{
		return sizeof(T) * width;
	}

	void *
	data()
	{
		return vbuf.data();
	}

	void
	copy(void *src, int size)
	{
		if (!src || size <= 0)
			return;
		vbuf.resize(size);
		memcpy(vbuf.data(), src, size);
	}

	void
	create(int w, int h)
	{
		width = w;
		height = h;
		vbuf.resize(width * height);
	}

	T
	get_pixel(int x, int y)
	{
		ret.rgba = 0;
		if (x < 0 || x >= width || y < 0 || y >= height)
			return ret;
		return vbuf[x + y * width];
	}

	void
	set_pixel(int x, int y, float r, float g, float b, float a)
	{
		if (x < 0 || x >= width || y < 0 || y >= height)
			return;
		color col;
		col.set(r, g, b, a);
		vbuf[x + y * width] = col;
	}

	void
	release()
	{
	}
};

struct constant_format {
	float4 v[16];
	float4x4 m[4];

	void
	print()
	{
		printf("%s : this=%p ------------------\n", __func__, this);
		for (auto & x : v) {
			x.print();
		}
		printf("%s : this=%p ------------------\n", __func__, this);
		for (auto & x : m) {
			x.print();
		}
	}
};

static lua_State *L = nullptr;
enum {
	I_NAME = 1,
	I_ARGS = 2,
};

static std::vector<oden::cmd> vcmd;
static std::map<std::string, std::vector<vertex_format>> mvertex;
static std::map<std::string, std::vector<uint32_t>> mindex;
static std::map<std::string, texture<color> > mtexture;
static std::map<std::string, constant_format> mconstant;

static void
l_show_table(lua_State* L)
{
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		switch (lua_type(L, -2)) {
		case LUA_TNUMBER :
			printf("key=%d, ", (int)lua_tointeger(L, -2));
			break;
		case LUA_TSTRING :
			printf("key=%s, ", lua_tostring(L, -2));
			break;
		}
		switch (lua_type(L, -1)) {
		case LUA_TNUMBER :
			printf("value=%d\n", (int)lua_tointeger(L, -1));
			break;
		case LUA_TSTRING :
			printf("value=%s\n", lua_tostring(L, -1));
			break;
		case LUA_TBOOLEAN :
			printf("value=%d\n", lua_toboolean(L, -1));
			break;
		default:
			printf("value=%s\n", lua_typename(L, lua_type(L, -1)));
			break;
		}
		lua_pop(L, I_NAME);
	}
}

static std::string
get_vertex_buffer_name(const char *name)
{
	return name + std::string("_vb");
}

static std::string
get_index_buffer_name(const char *name)
{
	return name + std::string("_ib");
}

static std::string
get_texture_name(const char *name)
{
	return name + std::string("_tex");
}

static int
l_create_vertex(lua_State *L)
{
	auto name = get_vertex_buffer_name(luaL_checkstring(L, I_NAME));
	lua_getfield(L, I_ARGS, "size");
	auto size = static_cast<size_t>(luaL_checknumber(L, -1));
	if (mvertex.count(name) == 0) {
		mvertex[name].resize(size);
	} else {
		printf("%s : DEBUG : already created name=%s\n",
			__func__, name.c_str());
	}
	return 0;
}

static int
l_update_vertex(lua_State *L)
{
	auto name = get_vertex_buffer_name(luaL_checkstring(L, I_NAME));
	if (mvertex.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & ref = mvertex[name];
	vertex_format data = {};

	//index
	lua_getfield(L, I_ARGS, "index");
	auto index = int(luaL_checknumber(L, -1));
	lua_pop(L, I_NAME);
	if (index >= ref.size()) {
		printf("ERROR : %s : name=%s, out of range index=%d\n",
			__func__, name.c_str(), index);
		return 0;
	}

	//position
	lua_getfield(L, I_ARGS, "x");
	lua_getfield(L, I_ARGS, "y");
	lua_getfield(L, I_ARGS, "z");
	lua_getfield(L, I_ARGS, "w");
	data.pos.x = float(luaL_checknumber(L, -4));
	data.pos.y = float(luaL_checknumber(L, -3));
	data.pos.z = float(luaL_checknumber(L, -2));
	data.pos.w = float(luaL_checknumber(L, -1));
	lua_pop(L, _countof(data.pos.data));

	//normal
	lua_getfield(L, I_ARGS, "nx");
	lua_getfield(L, I_ARGS, "ny");
	lua_getfield(L, I_ARGS, "nz");
	data.nor.x = float(luaL_checknumber(L, -3));
	data.nor.y = float(luaL_checknumber(L, -2));
	data.nor.z = float(luaL_checknumber(L, -1));
	lua_pop(L, _countof(data.nor.data));

	//uv
	lua_getfield(L, I_ARGS, "u");
	lua_getfield(L, I_ARGS, "v");
	data.uv.x = float(luaL_checknumber(L, -2));
	data.uv.y = float(luaL_checknumber(L, -1));
	lua_pop(L, _countof(data.uv.data));

	ref[index] = data;
	return 0;
}

static int
l_set_vertex(lua_State *L)
{
	auto name = get_vertex_buffer_name(luaL_checkstring(L, I_NAME));
	if (mvertex.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & ref = mvertex[name];
	odenutil::SetVertex(
		vcmd,
		name,
		ref.data(),
		ref.size() * sizeof(vertex_format),
		sizeof(vertex_format));
	return 0;
}

static int
l_create_index(lua_State *L)
{
	auto name = get_index_buffer_name(luaL_checkstring(L, I_NAME));
	lua_getfield(L, I_ARGS, "size");
	auto size = static_cast<size_t>(luaL_checknumber(L, -1));
	if (mindex.count(name) == 0) {
		mindex[name].resize(size);
	} else {
		printf("%s : DEBUG : already created name=%s\n",
			__func__, name.c_str());
	}
	return 0;
}

static int
l_update_index(lua_State *L)
{
	auto name = get_index_buffer_name(luaL_checkstring(L, I_NAME));
	if (mindex.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & ref = mindex[name];
	lua_getfield(L, I_ARGS, "index");
	lua_getfield(L, I_ARGS, "value");
	auto index = int(luaL_checknumber(L, -2));
	auto value = int(luaL_checknumber(L, -1));
	if (index >= ref.size()) {
		printf("WARNING : %s : name=%s, increase index=%d\n",
			__func__, name.c_str(), index);
		ref.resize(index + 1);
		printf("WARNING : %s : name=%s, increase done size=%zd\n",
			__func__, name.c_str(), ref.size());
	}
	ref[index] = value;
	return 0;
}

static int
l_set_index(lua_State *L)
{
	auto name = get_index_buffer_name(luaL_checkstring(L, I_NAME));
	if (mindex.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & ref = mindex[name];
	odenutil::SetIndex(
		vcmd,
		name,
		ref.data(),
		ref.size() * sizeof(uint32_t));
	return 0;
}

static int
l_create_texture(lua_State *L)
{
	auto name = get_texture_name(luaL_checkstring(L, I_NAME));
	lua_getfield(L, I_ARGS, "width");
	lua_getfield(L, I_ARGS, "height");
	auto width = static_cast<int>(luaL_checknumber(L, -2));
	auto height = static_cast<int>(luaL_checknumber(L, -1));
	if (mtexture.count(name) == 0) {
		mtexture[name].create(width, height);
	} else {
		printf("%s : DEBUG : already created name=%s\n",
			__func__, name.c_str());
	}
	return 0;
}

static int
l_update_texture(lua_State *L)
{
	auto name = get_texture_name(luaL_checkstring(L, I_NAME));
	if (mtexture.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & ref = mtexture[name];
	lua_getfield(L, I_ARGS, "x");
	lua_getfield(L, I_ARGS, "y");
	lua_getfield(L, I_ARGS, "r");
	lua_getfield(L, I_ARGS, "g");
	lua_getfield(L, I_ARGS, "b");
	lua_getfield(L, I_ARGS, "a");
	auto x = int(luaL_checknumber(L, -6));
	auto y = int(luaL_checknumber(L, -5));
	auto r = float(luaL_checknumber(L, -4));
	auto g = float(luaL_checknumber(L, -3));
	auto b = float(luaL_checknumber(L, -2));
	auto a = float(luaL_checknumber(L, -1));
	ref.set_pixel(x, y, r, g, b, a);
	return 0;
}

static int
l_clear_depthrendertarget(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "value");
	auto value = luaL_checknumber(L, -1);
	odenutil::ClearDepthRenderTarget(vcmd, name, value);
	return 0;
}

static int
l_clear_rendertarget(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "r");
	lua_getfield(L, I_ARGS, "g");
	lua_getfield(L, I_ARGS, "b");
	lua_getfield(L, I_ARGS, "a");
	float col[4] = {
		float(luaL_checknumber(L, -4)),
		float(luaL_checknumber(L, -3)),
		float(luaL_checknumber(L, -2)),
		float(luaL_checknumber(L, -1)),
	};
	odenutil::ClearRenderTarget(vcmd, name, col);
	return 0;
}

static int
l_dispatch(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "x");
	lua_getfield(L, I_ARGS, "y");
	lua_getfield(L, I_ARGS, "z");
	auto x = luaL_checknumber(L, -3);
	auto y = luaL_checknumber(L, -2);
	auto z = luaL_checknumber(L, -1);
	odenutil::Dispatch(vcmd, name, x, y, z);
	return 0;
}

static int
l_draw(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "count");
	auto vertex_count = luaL_checknumber(L, -1);
	odenutil::Draw(vcmd, name, vertex_count);
	return 0;
}

static int
l_draw_index(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "start");
	lua_getfield(L, I_ARGS, "count");
	auto start = int(luaL_checknumber(L, -2));
	auto count = int(luaL_checknumber(L, -1));
	odenutil::DrawIndex(vcmd, name, start, count);
	return 0;
}

static int
l_set_barrier_present(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	odenutil::SetBarrierToPresent(vcmd, name);
	return 0;
}

static int
l_set_barrier_rendertarget(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	odenutil::SetBarrierToRenderTarget(vcmd, name);
	return 0;
}

static int
l_set_barrier_texture(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	odenutil::SetBarrierToRenderTarget(vcmd, name);
	return 0;
}

static int
l_set_rendertarget(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "width");
	lua_getfield(L, I_ARGS, "height");
	auto width = luaL_checknumber(L, -2);
	auto height = luaL_checknumber(L, -1);
	odenutil::SetRenderTarget(vcmd, name, width, height);
	return 0;
}

static int
l_set_shader(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "is_update");
	lua_getfield(L, I_ARGS, "is_cull");
	lua_getfield(L, I_ARGS, "is_enable_depth");
	auto is_update = lua_toboolean(L, -3);
	auto is_cull = lua_toboolean(L, -2);
	auto is_enable_depth = lua_toboolean(L, -1);
	odenutil::SetShader(vcmd, name, is_update, is_cull, is_enable_depth);
	return 0;
}

static int
l_set_texture(lua_State *L)
{
	auto name = get_texture_name(luaL_checkstring(L, I_NAME));
	if (mtexture.count(name) == 0) {
		printf("%s : cant found : name=%s\n",
			__func__, name.c_str());
		return 0;
	}
	auto & tex = mtexture[name];
	lua_getfield(L, I_ARGS, "slot");
	auto slot = int(luaL_checknumber(L, -1));

	odenutil::SetTexture(vcmd, name, slot,
		tex.get_width(), tex.get_height(), tex.data(),
		tex.get_size(), tex.get_stride_size());

	return 0;
}

static int
l_set_textureuav(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	printf("%s : Not impl\n", __func__);
	return 0;
}

static int
l_set_constant(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "slot");
	auto slot = int(luaL_checknumber(L, -1));
	auto & c = mconstant[name];
	odenutil::SetConstant(vcmd, name, slot, &c, sizeof(constant_format));
	return 0;
}

static int
l_update_constant_float4(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "index");
	lua_getfield(L, I_ARGS, "value");
	auto index = int(luaL_checknumber(L, -2));

	float4 v;
	for (int i = 0 ; i < 4; i++) {
		lua_rawgeti(L, -1, i + 1);
		v.data[i] = lua_tonumber(L, -1);;
		lua_pop(L, I_NAME);
	}
	auto & ref = mconstant[name];
	ref.v[index] = v;

	return 0;
}

static int
l_update_constant_float4x4(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "index");
	lua_getfield(L, I_ARGS, "value");
	auto index = int(luaL_checknumber(L, -2));
	float4x4 m;

	for (int i = 0 ; i < 16; i++) {
		lua_rawgeti(L, -1, i + 1);
		m.data[i] = lua_tonumber(L, -1);
		lua_pop(L, I_NAME);
	}
	auto & ref = mconstant[name];
	ref.m[index] = m;

	return 0;
}

static int
l_get_backbuffer_name(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	lua_getfield(L, I_ARGS, "index");
	auto index = int(luaL_checknumber(L, -1));
	auto result = oden::oden_get_backbuffer_name(index);
	lua_pushstring(L, result.c_str());
	return 1;
}

static int
l_get_depthrendertarget_name(lua_State *L)
{
	auto name = luaL_checkstring(L, I_NAME);
	auto result = oden::oden_get_depth_render_target_name(name);
	lua_pushstring(L, result.c_str());
	return 1;
}

static int
oden_funcs(lua_State* l)
{
	static const luaL_Reg lua_funcs[] = {
		//ODEN API
		{"clear_depthrendertarget", l_clear_depthrendertarget},
		{"clear_rendertarget", l_clear_rendertarget},
		{"dispatch", l_dispatch},
		{"draw", l_draw},
		{"draw_index", l_draw_index},
		{"set_barrier_present", l_set_barrier_present},
		{"set_barrier_rendertarget", l_set_barrier_rendertarget},
		{"set_barrier_texture", l_set_barrier_texture},
		{"set_constant", l_set_constant},
		{"set_index", l_set_index},
		{"set_rendertarget", l_set_rendertarget},
		{"set_shader", l_set_shader},
		{"set_texture", l_set_texture},
		{"set_textureuav", l_set_textureuav},
		{"set_vertex", l_set_vertex},

		//Bind utils.
		{"create_vertex", l_create_vertex},
		{"create_index", l_create_index},
		{"update_vertex", l_update_vertex},
		{"update_index", l_update_index},
		{"create_texture", l_create_texture},
		{"update_texture", l_update_texture},

		{"update_constant_float4", l_update_constant_float4},
		{"update_constant_float4x4", l_update_constant_float4x4},

		{"get_backbuffer_name", l_get_backbuffer_name},
		{"get_depthrendertarget_name", l_get_depthrendertarget_name},
		{NULL, NULL}
	};
	luaL_newlib(l, lua_funcs);
	return 1;
}

static void
lua_term()
{
	if (L)
		lua_close(L);
	L = nullptr;
}

static void
lua_reload()
{
	lua_term();

	L = luaL_newstate();

	luaL_openlibs(L);
	luaL_requiref(L, "oden", oden_funcs, 1);
	luaL_loadfile(L, "update.lua");

	printf("BOOT... : %s\n", __func__);
	lua_pcall(L, 0, 0, 0);
	printf("DONE... : %s\n", __func__);
}

static void
lua_init()
{
	lua_reload();
}

static void
lua_update(uint64_t frame, int width, int height)
{
	lua_getglobal(L, "update");
	lua_pushnumber(L, frame);
	lua_pushnumber(L, width);
	lua_pushnumber(L, height);
	if (lua_pcall(L, 3, 0, 0)) {
		printf("ERROR %s\n", lua_tostring(L, -1));
	}
}

} //namespace

int
main(int argc, char *argv[])
{
	using namespace odenutil;
	enum {
		Width = 1280,
		Height = 720,
		BufferMax = 2,
		ShaderSlotMax = 8,
		ResourceMax = 1024,
	};

	uint64_t frame = 0;

	//Initialize Lua
	lua_init();

	auto app_name = "luabind";
	auto hwnd = InitWindow(app_name, Width, Height);
	while (Update()) {
		if (GetAsyncKeyState(VK_F5) & 0x0001)
			lua_reload();

		lua_update(frame, Width, Height);
		oden_present_graphics(
			app_name, vcmd, hwnd,
			Width, Height, BufferMax,
			ResourceMax, ShaderSlotMax);
		vcmd.clear();
		frame++;
	}

	//Terminate Oden.
	oden_present_graphics(
		app_name, vcmd, nullptr,
		Width, Height, BufferMax,
		ResourceMax, ShaderSlotMax);

	//Terminate Lua
	lua_term();

	return 0;
}

