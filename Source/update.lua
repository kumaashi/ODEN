local TEX_WIDTH = 256
local TEX_HEIGHT = 256

local name_tex = "tex"
local name_rect = "rect"

local time = 0
function init_resources()
	oden.create_texture(name_tex, {width = TEX_WIDTH, height = TEX_HEIGHT})
	for y = 0, TEX_HEIGHT do
		for x = 0, TEX_WIDTH do
			oden.update_texture(name_tex, {x = x, y = y, r = 0, g = x / TEX_WIDTH, b = 0, a = 1})
		end
	end

	oden.create_vertex(name_rect, {size = 4})
	oden.update_vertex(name_rect, {index = 0, x = -1, y = -1, z = 0, w = 1, nx = 1, ny = 0, nz = 0,  u = 0, v = 0})
	oden.update_vertex(name_rect, {index = 1, x =  1, y = -1, z = 0, w = 1, nx = 0, ny = 1, nz = 0,  u = 1, v = 0})
	oden.update_vertex(name_rect, {index = 2, x = -1, y =  1, z = 0, w = 1, nx = 0, ny = 0, nz = 1,  u = 0, v = 1})
	oden.update_vertex(name_rect, {index = 3, x =  1, y =  1, z = 0, w = 1, nx = 0, ny = 0, nz = 1,  u = 1, v = 1})

	oden.create_index(name_rect, {size = 6})
	oden.update_index(name_rect, {index = 0, value = 0})
	oden.update_index(name_rect, {index = 1, value = 1})
	oden.update_index(name_rect, {index = 2, value = 2})
	oden.update_index(name_rect, {index = 3, value = 1})
	oden.update_index(name_rect, {index = 4, value = 2})
	oden.update_index(name_rect, {index = 5, value = 3})
end

function update_frame_prapare(frame)
	if frame == 0 then
		init_resources()
	end
end

function update(frame, width, height)
	local index = frame % 2
	time = time + 1.0 / 60.0
	update_frame_prapare(frame);
	oden.update_constant_float4(name_rect, {index=0, value={1,index,1,1}})
	oden.update_constant_float4(name_rect, {index=1, value={0,index,1,1}})
	local backbuffer_name = oden.get_backbuffer_name("frame", {index=index})
	oden.set_rendertarget(backbuffer_name, { width = width, height = height})
	local clear_color = {
		r = (index + 1) & 1,
		g = 0,
		b = (index & 1),
		a = 1}
	clear_color.r = clear_color.r * 0.01;
	oden.clear_rendertarget(backbuffer_name, clear_color)
	oden.set_shader("clear.hlsl", {is_update = false, is_cull = false, is_enable_depth = false})
	oden.set_constant(name_rect, {slot=0})
	oden.set_vertex(name_rect)
	oden.set_index(name_rect)
	oden.set_texture(name_tex, {slot = 0})
	oden.draw_index(name_rect, {start = 0, count = 6});
	oden.set_barrier_present(backbuffer_name)
end

