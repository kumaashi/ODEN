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
struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
	PSInput result = (PSInput)0;
	float2 tuv = uv.xy * 0.5 + 0.5;
	result.position = position;
	result.color = tuv.xyxy;
	result.color.y = 1.0 - result.color.y;
	//result.position.xy *= 0.5;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float2 uv = input.color.xy;
	float2 uvs = input.color.xy;
	uvs = uvs * 2.0 - 1.0;
	if(abs(uvs.y) > 0.8) return float4(0, 0, 0, 1);
	float vig = 1.0 - dot(uvs * 0.5, uvs);
	uvs *= 0.95;
	uvs = uvs * 0.5 + 0.5;
	float edge = 0.5 * tex0.SampleLevel(LinearSampler, uvs, 0) - tex0.SampleLevel(LinearSampler, uv, 0);
	edge = step(0.1, max(0, edge)) * max(0.3, abs(sin(time.x * 64.0)));
	float4 col = tex0.SampleLevel(LinearSampler, uv, 0);
	col.x = tex0.SampleLevel(LinearSampler, uv + float2(0.001, 0.001), 0).x;
	col.z = tex0.SampleLevel(LinearSampler, uv - float2(0.001, 0.001), 0).z;
	return (col);// + edge * float4(1, 1, 0, 1)) * vig;
}
