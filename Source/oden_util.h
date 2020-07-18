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
#pragma once

#include "ODEN.h"

namespace odenutil
{
using namespace oden;
void ClearDepthRenderTarget(std::vector<cmd> & vcmd, std::string name, float value);
void ClearRenderTarget(std::vector<cmd> & vcmd, std::string name, float col[4]);
void DebugPrint(std::vector<cmd> & vcmd);
void Dispatch(std::vector<cmd> & vcmd, std::string name, int x, int y, int z);
void Draw(std::vector<cmd> & vcmd, std::string name, int vertex_count);
void DrawIndex(std::vector<cmd> & vcmd, std::string name, int start, int count);
void SetBarrierToPresent(std::vector<cmd> & vcmd, std::string name);
void SetBarrierToRenderTarget(std::vector<cmd> & vcmd, std::string name);
void SetBarrierToTexture(std::vector<cmd> & vcmd, std::string name);
void SetConstant(std::vector<cmd> & vcmd, std::string name, int slot, void *data, size_t size);
void SetIndex(std::vector<cmd> & vcmd, std::string name, void *data, size_t size);
void SetRenderTarget(std::vector<cmd> & vcmd, std::string name, int w, int h);
void SetShader(std::vector<cmd> & vcmd, std::string name, bool is_update, bool is_cull = false, bool is_enable_depth = false);
void SetTexture(std::vector<cmd> & vcmd, std::string name, int slot, int w = 0, int h = 0, void *data = nullptr, size_t size = 0, size_t stride_size = 0);
void SetTextureUav(std::vector<cmd> & vcmd, std::string name, int slot, int w = 0, int h = 0, int miplevel = 0, void *data = nullptr, size_t size = 0, size_t stride_size = 0);
void SetVertex(std::vector<cmd> & vcmd, std::string name, void *data, size_t size, size_t stride_size);

};
