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

Texture2D<float4> tex0 : register(t0);
Texture2D<float4> tex1 : register(t1);
SamplerState PointSampler   : register(s0);
SamplerState LinearSampler  : register(s1);

cbuffer constdata : register(b0)
{
	float4 time;
	float4 misc;
	float4x4 proj;
	float4x4 view;
};

struct PSInput {
	float4 position : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
};

PSInput VSMain(
	float4 position : POSITION,
	float3 normal : NORMAL,
	float2 uv : TEXCOORD)
{
	PSInput result = (PSInput)0;
	result.position = position;
	result.uv = uv;
	return result;
}

void PSMain(PSInput input, out float4 mrt0 : SV_TARGET)
{
	float3 bgcol = float3(0.2, 0.3, 0.5);
	float k = max(0.2, 1.0 - (input.uv.y * 0.5 + 0.5));
	mrt0 = float4(bgcol * k,1);
}
