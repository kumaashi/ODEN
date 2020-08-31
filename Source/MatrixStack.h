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

#include <stdio.h>
#include <windows.h>
#include <DirectXMath.h>

//Simple Matrix struct for windows.
struct MatrixStack {
	enum {
		Max = 32,
	};
	unsigned index = 0;
	DirectX::XMMATRIX data[Max];

	MatrixStack()
	{
		Reset();
	}

	auto & Get(int i)
	{
		return data[i];
	}

	auto & GetTop()
	{
		return Get(index);
	}

	void GetTop(float *a)
	{
		//XMStoreFloat4x4((DirectX::XMFLOAT4X4 *) a, XMMatrixTranspose(GetTop()));
		XMStoreFloat4x4((DirectX::XMFLOAT4X4 *) a, (GetTop()));
	}

	void Reset()
	{
		index = 0;
		for (int i = 0 ; i < Max; i++) {
			auto & m = Get(i);
			m = DirectX::XMMatrixIdentity();
		}
	}

	void Push()
	{
		auto & mb = GetTop();
		index = (index + 1) % Max;
		auto & m = GetTop();
		m = mb;
		printf("%s:index=%d\n", __FUNCTION__, index);
	}

	void Pop()
	{
		index = (index - 1) % Max;
		if (index < 0)
			printf("%s : under flow\n", __FUNCTION__);
		printf("%s:index=%d\n", __FUNCTION__, index);
	}

	void Load(DirectX::XMMATRIX a)
	{
		auto & m = GetTop();
		m = a;
	}

	void Load(float *a)
	{
		Load(* (DirectX::XMMATRIX *) a);
	}

	void Mult(DirectX::XMMATRIX a)
	{
		auto & m = GetTop();
		m *= a;
	}

	void Mult(float *a)
	{
		Mult(* (DirectX::XMMATRIX *) a);
	}

	void LoadIdentity()
	{
		Load(DirectX::XMMatrixIdentity());
	}

	void LoadLookAt(
		float px, float py, float pz,
		float ax, float ay, float az,
		float ux, float uy, float uz)
	{
		Load(DirectX::XMMatrixLookAtLH({px, py, pz}, {ax, ay, az}, {ux, uy, uz}));
	}

	void LoadPerspective(float ffov, float faspect, float fnear, float ffar)
	{
		Load(DirectX::XMMatrixPerspectiveFovLH(ffov, faspect, fnear, ffar));
	}

	void Translation(float x, float y, float z)
	{
		Mult(DirectX::XMMatrixTranslation(x, y, z));
	}

	void RotateAxis(float x, float y, float z, float angle)
	{
		Mult(DirectX::XMMatrixRotationAxis({x, y, z}, angle));
	}

	void RotateX(float angle)
	{
		Mult(DirectX::XMMatrixRotationX(angle));
	}

	void RotateY(float angle)
	{
		Mult(DirectX::XMMatrixRotationY(angle));
	}

	void RotateZ(float angle)
	{
		Mult(DirectX::XMMatrixRotationZ(angle));
	}

	void Scaling(float x, float y, float z)
	{
		Mult(DirectX::XMMatrixScaling(x, y, z));
	}

	void Transpose()
	{
		Load(DirectX::XMMatrixTranspose(GetTop()));
	}

	void Print(DirectX::XMMATRIX m)
	{
		int i = 0;
		for (auto & v : m.r) {
			for (auto & e : v.m128_f32) {
				if ((i % 4) == 0) printf("\n");
				printf("[%02d]%.4f, ", i++, e);
			}
		}
		printf("\n");
	}

	void Print()
	{
		Print(GetTop());
	}

	void PrintAll()
	{
		for (int i = 0; i < Max; i++) {
			Print(Get(i));
		}
	}
};

