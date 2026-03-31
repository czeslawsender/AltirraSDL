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

#include <stdafx.h>
#define D3D11_NO_HELPERS
#define INITGUID

#include <windows.h>

#include <dxgi1_3.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>

#include <d3d11.h>
#include <d3d11_1.h>

#include <vd2/system/w32assist.h>
#include <vd2/Tessa/Config.h>
#include <vd2/Tessa/Options.h>
#include <vd2/Tessa/holderdx.h>

///////////////////////////////////////////////////////////////////////////////

VDDXGIHolder::VDDXGIHolder() {
}

VDDXGIHolder::~VDDXGIHolder() {
	Shutdown();
}

void *VDDXGIHolder::AsInterface(uint32 iid) {
	return nullptr;
}

bool VDDXGIHolder::Init() {
	if (!mhmodDXGI) {
		if (VDTGetLibraryOverridesEnabled())
			mhmodDXGI = VDLoadSystemLibraryWithAllowedOverrideW32("dxgi");
		else
			mhmodDXGI = VDLoadSystemLibraryW32("dxgi");

		if (!mhmodDXGI) {
			Shutdown();
			return false;
		}
	}

	if (!mpCreateDXGIFactoryFn) {
		mpCreateDXGIFactoryFn = (CreateDXGIFactoryFn)GetProcAddress(mhmodDXGI, "CreateDXGIFactory1");

		if (!mpCreateDXGIFactoryFn) {
			Shutdown();
			return false;
		}
	}
	
	if (!mpCreateDXGIFactory2Fn)
		mpCreateDXGIFactory2Fn = (CreateDXGIFactory2Fn)GetProcAddress(mhmodDXGI, "CreateDXGIFactory2");

	return true;
}

void VDDXGIHolder::Shutdown() {
	mpCreateDXGIFactoryFn = nullptr;
	mpCreateDXGIFactory2Fn = nullptr;

	if (mhmodDXGI) {
		FreeLibrary(mhmodDXGI);
		mhmodDXGI = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

VDD3D11Holder::VDD3D11Holder() {
}

VDD3D11Holder::~VDD3D11Holder() {
	Shutdown();
}

void *VDD3D11Holder::AsInterface(uint32 iid) {
	return nullptr;
}

bool VDD3D11Holder::Init() {
	if (!mhmodD3D11) {
		if (VDTGetLibraryOverridesEnabled())
			mhmodD3D11 = VDLoadSystemLibraryWithAllowedOverrideW32("D3D11");
		else
			mhmodD3D11 = VDLoadSystemLibraryW32("D3D11");

		if (!mhmodD3D11) {
			Shutdown();
			return false;
		}
	}

	if (!mpCreateDeviceFn) {
		mpCreateDeviceFn = (CreateDeviceFn)GetProcAddress(mhmodD3D11, "D3D11CreateDevice");
		if (!mpCreateDeviceFn) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDD3D11Holder::Shutdown() {
	mpCreateDeviceFn = NULL;

	if (mhmodD3D11) {
		FreeLibrary(mhmodD3D11);
		mhmodD3D11 = NULL;
	}
}
