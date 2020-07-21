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
SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

cbuffer binfo : register(b0)
{
	float4 direction;
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
	result.uv = uv;
	return result;
}

//https://github.com/Jam3/glsl-fast-gaussian-blur/
float4 blur13(Texture2D<float4> image, float2 uv, float2 resolution, float2 direction, float miplevel)
{
	float4 color = 0;
	float2 off1 = 1.411764705882353 * direction;
	float2 off2 = 3.2941176470588234 * direction;
	float2 off3 = 5.176470588235294 * direction;
	color += tex0.SampleLevel(LinearSampler, uv, miplevel) * 0.1964825501511404;
	color += tex0.SampleLevel(LinearSampler, uv + (off1 / resolution), miplevel) * 0.2969069646728344;
	color += tex0.SampleLevel(LinearSampler, uv - (off1 / resolution), miplevel) * 0.2969069646728344;
	color += tex0.SampleLevel(LinearSampler, uv + (off2 / resolution), miplevel) * 0.09447039785044732;
	color += tex0.SampleLevel(LinearSampler, uv - (off2 / resolution), miplevel) * 0.09447039785044732;
	color += tex0.SampleLevel(LinearSampler, uv + (off3 / resolution), miplevel) * 0.010381362401148057;
	color += tex0.SampleLevel(LinearSampler, uv - (off3 / resolution), miplevel) * 0.010381362401148057;
	color.a = 1.0;
	return color;
}

float4 PSMain(PSInput input) : SV_TARGET {
	float2 uv = input.uv;
	float2 dir = direction.zw;
	float2 rtsize = direction.xy;
	float4 col = float4(0, 0, 0, 0);

	const int MAX_LEVEL = 10;
	const float INV_MAX_LEVEL = (1.0 / float(MAX_LEVEL));
	for (int i = MAX_LEVEL ; i >= 0; i--)
	{
		float level  = float(i);
		col += blur13(tex0, uv, rtsize, dir, level);
		col *= 0.5;
	}

	return col;
}
