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

RWTexture2D<float4> tex0 : register(u0);
RWTexture2D<float4> tex1 : register(u1);

[numthreads(1, 1, 1)]
void CSMain(
	uint3 groupId : SV_GroupID,
	uint3 groupThreadId : SV_GroupThreadID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint groupIndex : SV_GroupIndex)
{
	uint2 idx = (dispatchThreadId.xy * 2);
	float4 c = float4(0, 0, 0, 0);
	c += tex0[idx + uint2( 0,  0)];
	/*
	c += tex0[idx + uint2(-1, -1)];
	c += tex0[idx + uint2(-1,  0)];
	c += tex0[idx + uint2(-1,  1)];
	c += tex0[idx + uint2( 0, -1)];
	c += tex0[idx + uint2( 0,  1)];
	c += tex0[idx + uint2( 1, -1)];
	c += tex0[idx + uint2( 1,  0)];
	c += tex0[idx + uint2( 1,  1)];
	c /= 9;
	*/
	tex1[dispatchThreadId.xy] = c;
}
