//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2025 Avery Lee
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

#ifndef f_AT_ATIO_DISKIMAGEUTIL_H
#define f_AT_ATIO_DISKIMAGEUTIL_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>

class ATDiskImageFreeList {
public:
	uint32 GetAvailable() const;
	uint32 GetLargestFree() const;

	void Clear();
	uint32 Allocate(uint32 size, vdfastvector<uint8>& imageBuffer);
	void Free(uint32 offset, uint32 size, vdfastvector<uint8>& imageBuffer);
	bool Validate(const vdfastvector<uint8>& imageBuffer) const;

private:
	struct Span {
		uint32 mOffset;
		uint32 mSize;

		bool operator<(const Span& other) const {
			return mOffset < other.mOffset;
		}
	};

	// free spans, sorted by offset
	vdfastvector<Span> mFreeList;
};

#endif
