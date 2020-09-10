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
layout(set=1, binding=0) uniform object_buffer {
	vec4 time;
	vec4 misc;
	mat4 world;
	mat4 proj;
	mat4 view;
} ubufs[];

#ifdef _VS_
layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;

layout(location=0) out vec4 v_pos;
layout(location=1) out vec3 v_nor;
layout(location=2) out vec2 v_uv;

void main()
{
	int id = gl_InstanceIndex; //deprecated, gl_InstanceID
	mat4 wvp  = ubufs[id].proj * ubufs[id].view * ubufs[id].world;
	v_pos = vec4(position.xyz, 1.0);
	if(ubufs[id].misc.x > 0.5 ) {
		if(v_pos.y > 0.0)
			v_pos.x *= 0.0;
		v_pos.y += 2.1;
	}
	v_nor = normal;
	v_uv = uv;
	v_pos = wvp * vec4(v_pos.xyz, 1.0);
	gl_Position = v_pos;
}
#endif //_VS_

#ifdef _PS_
layout(location=0) in vec4 v_pos;
layout(location=1) in vec3 v_nor;
layout(location=2) in vec2 v_uv;
layout(location=0) out vec4 out_color;

void main()
{
	vec4 col = texture(tex[0], v_uv) + vec4(0.1, 0.2, 0.3, 1.0);
	col.w = v_pos.z / v_pos.w;
	out_color = col;
}

#endif //_PS_

