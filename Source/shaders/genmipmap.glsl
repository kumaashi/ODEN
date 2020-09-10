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

layout(push_constant) uniform pushconstlocal {
	uint id;
} pushconst;

layout(set=2, binding=0, rgba16) uniform image2D imagebuf[];

layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
void main()
{
	uint id = pushconst.id;
	ivec2 loc_dst = ivec2(gl_GlobalInvocationID.xy);
	ivec2 loc_src = loc_dst;
	int miplevel = int(gl_GlobalInvocationID.z);
	if(miplevel == 0)
		return;
	loc_src <<= miplevel;
	vec4 c = imageLoad(imagebuf[id + 0], loc_src);
	imageStore(imagebuf[miplevel], loc_dst, c);
}
