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

#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable

layout(set=0, binding=0) uniform sampler2D tex[];
layout(set=1, binding=0) uniform buf {
	vec4 time;
	vec4 misc;
	mat4 world;
	mat4 proj;
	mat4 view;
	uint matid[4];
} ubufs[];

#ifdef _VS_
layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;

layout(location=0) out vec4 v_pos;
layout(location=1) out vec3 v_nor;
layout(location=2) out vec2 v_uv;
layout(location=3) flat out int v_id;

void main()
{
	v_pos = position;
	v_nor = vec3(0, 0, 1);
	v_uv = uv;
	gl_Position = v_pos;
	v_id = int(gl_InstanceIndex);
}
#endif //_VS_


#ifdef _PS_

layout(location=0) in vec4 v_pos;
layout(location=1) in vec3 v_nor;
layout(location=2) in vec2 v_uv;
layout(location=3) flat in int v_id;

layout(location=0) out vec4 out_color;

//https://github.com/Jam3/glsl-fast-gaussian-blur/
vec4 blur13(uint matid, vec2 uv, vec2 resolution, vec2 direction, float miplevel)
{
	vec4 color = vec4(0.0);
	vec2 off1 = 1.411764705882353 * direction;
	vec2 off2 = 3.2941176470588234 * direction;
	vec2 off3 = 5.176470588235294 * direction;
	color += texture(tex[matid], uv, miplevel) * 0.1964825501511404;
	color += texture(tex[matid], uv + (off1 / resolution), miplevel) * 0.2969069646728344;
	color += texture(tex[matid], uv - (off1 / resolution), miplevel) * 0.2969069646728344;
	color += texture(tex[matid], uv + (off2 / resolution), miplevel) * 0.09447039785044732;
	color += texture(tex[matid], uv - (off2 / resolution), miplevel) * 0.09447039785044732;
	color += texture(tex[matid], uv + (off3 / resolution), miplevel) * 0.010381362401148057;
	color += texture(tex[matid], uv - (off3 / resolution), miplevel) * 0.010381362401148057;
	color.a = 1.0;
	return color;
}


void main() {
	vec2 uv = v_uv;
	int id = v_id; //deprecated, gl_InstanceID
	vec2 dir = ubufs[id].misc.zw;
	vec2 rtsize = ubufs[id].misc.xy;
	uint matid = ubufs[id].matid[0];
	vec4 col = vec4(0, 0, 0, 0);

	const int MAX_LEVEL = 10;
	const float INV_MAX_LEVEL = (1.0 / float(MAX_LEVEL));
	for (int i = MAX_LEVEL ; i >= 0; i--)
	{
		float level  = float(i);
		col += blur13(matid, uv, rtsize, dir, level);
		col *= 0.5;
	}
	out_color = col;
}
#endif //_PS_
