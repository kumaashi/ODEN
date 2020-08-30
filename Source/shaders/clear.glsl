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

#ifdef _VS_
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;

layout(location=0) out vec4 v_pos;
layout(location=1) out vec3 v_nor;
layout(location=2) out vec2 v_uv;

layout(binding=0) uniform buf {
	vec4 direction;
	vec4 time;
	vec4 misc;
	mat4 proj;
	mat4 view;
} ubuf;

void main()
{
	v_pos = vec4(position, 1.0);
	v_nor = vec3(0, 0, 1);
	v_uv = uv;
}
#endif //_VS_


#ifdef _PS_
layout(location=0) in vec4 v_pos;
layout(location=1) in vec3 v_nor;
layout(location=2) in vec2 v_uv;

layout(location=0) out vec4 out_color;
layout(binding=0) uniform sampler2D tex0;

void main()
{
	vec3 bgcol = vec3(0.2, 0.3, 0.5);
	float k = max(0.2, 1.0 - (v_uv.y * 0.5 + 0.5));
	out_color = vec4(bgcol * k,1);
}
#endif //_PS_
