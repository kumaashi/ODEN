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

layout(binding=0) uniform sampler2D tex0;
layout(binding=3) uniform sampler2D tex1;
layout(binding=1) uniform buf {
	vec4 time;
	vec4 misc;
	mat4 world;
	mat4 proj;
	mat4 view;
} ubuf;

#ifdef _VS_
layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=0) out vec4 v_pos;
layout(location=1) out vec3 v_nor;
layout(location=2) out vec2 v_uv;

void main()
{
	v_pos = position;
	v_nor = vec3(0, 0, 1);
	v_uv = uv;
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
	vec2 uv = v_uv;
	vec4 col = texture(tex0, uv, 0.0);
	vec4 blurcol = texture(tex1, uv, 0.0);
	col.x = texture(tex0, uv + vec2(0.001, 0.001), 0).x;
	col.z = texture(tex0, uv - vec2(0.001, 0.001), 0).z;
	out_color = col + blurcol;
}
#endif //_PS_
