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
	float2 uv : TEXCOORD0;
};

PSInput VSMain(
	float4 position : POSITION,
	float3 normal : NORMAL,
	float2 uv : TEXCOORD)
{
	PSInput result = (PSInput)0;
	result.position = position;
	result.uv = uv * 4.0;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
	float2 uv = input.uv;
	if(uv.x > 1.0) discard;
	if(uv.y > 1.0) discard;
	float4 col = tex0.SampleLevel(LinearSampler, uv, 0.0);
	return pow(col.xxxx, 64.0);
}
