//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2026 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef f_VD2_TESSA_HOLDERDX_H
#define f_VD2_TESSA_HOLDERDX_H

#include <windows.h>
#include <d3dcommon.h>
#include <vd2/system/refcount.h>
#include <vd2/system/unknown.h>

struct IDXGIAdapter;
struct ID3D11Device;
struct ID3D11DeviceContext;

class VDDXGIHolder : public vdrefcounted<IVDRefUnknown> {
public:
	VDDXGIHolder();
	~VDDXGIHolder();

	void *AsInterface(uint32 iid);

	typedef HRESULT (APIENTRY *CreateDXGIFactoryFn)(REFIID riid, void **ppFactory);
	typedef HRESULT (APIENTRY *CreateDXGIFactory2Fn)(UINT flags, REFIID riid, void **ppFactory);

	CreateDXGIFactoryFn GetCreateDXGIFactoryFn() const { return mpCreateDXGIFactoryFn; }
	CreateDXGIFactory2Fn GetCreateDXGIFactory2Fn() const { return mpCreateDXGIFactory2Fn; }

	bool Init();
	void Shutdown();

protected:
	HMODULE mhmodDXGI = nullptr;
	CreateDXGIFactoryFn mpCreateDXGIFactoryFn = nullptr;
	CreateDXGIFactory2Fn mpCreateDXGIFactory2Fn = nullptr;
};

class VDD3D11Holder : public vdrefcounted<IVDRefUnknown> {
public:
	VDD3D11Holder();
	~VDD3D11Holder();

	void *AsInterface(uint32 iid);

	typedef HRESULT (APIENTRY *CreateDeviceFn)(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext);

	CreateDeviceFn GetCreateDeviceFn() const { return mpCreateDeviceFn; }

	HMODULE GetD3D11() const { return mhmodD3D11; }

	bool Init();
	void Shutdown();

protected:
	HMODULE mhmodD3D11 = nullptr;
	CreateDeviceFn mpCreateDeviceFn = nullptr;
};

#endif
