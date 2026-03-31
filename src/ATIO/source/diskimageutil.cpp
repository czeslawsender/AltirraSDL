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

#include <stdafx.h>
#include <at/atio/diskimageutil.h>
#include <at/attest/test.h>

uint32 ATDiskImageFreeList::GetAvailable() const {
	uint32 avail = 0;

	for(const Span& span : mFreeList)
		avail += span.mSize;

	return avail;
}

uint32 ATDiskImageFreeList::GetLargestFree() const {
	uint32 largest = 0;

	for(const Span& span : mFreeList)
		largest = std::max<uint32>(largest, span.mSize);

	return largest;
}

void ATDiskImageFreeList::Clear() {
	mFreeList.clear();
}

uint32 ATDiskImageFreeList::Allocate(uint32 size, vdfastvector<uint8>& imageBuffer) {
	if (!size)
		return 0;

	// try to reuse existing free space
	auto it = mFreeList.begin(), itEnd = mFreeList.end();
	for(; it != itEnd; ++it) {
		Span& span = *it;

		if (span.mSize >= size) {
			uint32 offset = span.mOffset;

			if (span.mSize == size) {
				mFreeList.erase(it);
			} else {
				it->mOffset += size;
				it->mSize -= size;
			}

			return offset;
		}
	}

	// extend image
	uint32 offset = (uint32)imageBuffer.size();
	imageBuffer.resize(offset + size);

	return offset;
}

void ATDiskImageFreeList::Free(uint32 offset, uint32 size, vdfastvector<uint8>& imageBuffer) {
	if (!size)
		return;

	Span span { offset, size };

	auto it = std::lower_bound(mFreeList.begin(), mFreeList.end(), span);

	if (it == mFreeList.end()) {
		// at end of free list -- check if we're freeing at the end of the image
		if (offset + size >= imageBuffer.size()) {
			VDASSERT(offset + size == imageBuffer.size());

			// yes -- check for a possible merge against previous as well
			if (it != mFreeList.begin() && it[-1].mOffset + it[-1].mSize == offset) {
				offset = it[-1].mOffset;
				mFreeList.pop_back();
			}

			// trim the image
			imageBuffer.resize(offset);
			return;
		}
	} else {
		// check for merge w/next
		if (it->mOffset <= offset + size) {
			VDASSERT(it->mOffset == offset + size);

			// check for merge w/ both prev and next
			if (it != mFreeList.begin() && it[-1].mSize == offset) {
				it[-1].mSize += size + it->mSize;

				mFreeList.erase(it);
			} else {
				it->mSize += size;
				it->mOffset = offset;
			}
			return;
		}
	}
	
	if (it != mFreeList.begin()) {
		// check for merge w/prev
		if (it[-1].mOffset + it[-1].mSize >= offset) {
			VDASSERT(it[-1].mOffset + it[-1].mSize == offset);

			it[-1].mSize += size;
			return;
		}
	}

	// insert new span
	mFreeList.insert(it, span);
}

bool ATDiskImageFreeList::Validate(const vdfastvector<uint8>& imageBuffer) const {
	size_t n = imageBuffer.size();

	uint32 prev = 0;

	for(const Span& span : mFreeList) {
		if (span.mOffset <= prev && prev)
			return false;

		if (!span.mSize)
			return false;

		prev = span.mOffset + span.mSize;
		if (prev >= n)
			return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef AT_TESTS_ENABLED
AT_DEFINE_TEST(IO_DiskImageUtil) {
	ATDiskImageFreeList freeList;
	vdfastvector<uint8> image;

	uint32 offset = freeList.Allocate(4096, image);
	AT_TEST_ASSERT(offset == 0);
	AT_TEST_ASSERT(image.size() == 4096);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	offset = freeList.Allocate(4096, image);
	AT_TEST_ASSERT(offset == 4096);
	AT_TEST_ASSERT(image.size() == 8192);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(0, 4096, image);
	AT_TEST_ASSERT(image.size() == 8192);
	AT_TEST_ASSERT(freeList.GetAvailable() == 4096);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 4096);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(4096, 4096, image);
	AT_TEST_ASSERT(image.size() == 0);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	AT_TEST_ASSERT(0 == freeList.Allocate(4096, image));
	AT_TEST_ASSERT(4096 == freeList.Allocate(4096, image));
	AT_TEST_ASSERT(8192 == freeList.Allocate(4096, image));
	AT_TEST_ASSERT(12288 == freeList.Allocate(4096, image));
	AT_TEST_ASSERT(16384 == freeList.Allocate(4096, image));
	AT_TEST_ASSERT(image.size() == 20480);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(16384, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(0, 4096, image);
	freeList.Free(4096, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 8192);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 8192);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Allocate(4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 4096);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 4096);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Allocate(4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(8192, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 4096);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 4096);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(4096, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 8192);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 8192);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Allocate(4096, image);
	freeList.Allocate(4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 0);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 0);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(0, 4096, image);
	freeList.Free(8192, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 8192);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 4096);
	AT_TEST_ASSERT(freeList.Validate(image));

	freeList.Free(4096, 4096, image);
	AT_TEST_ASSERT(image.size() == 16384);
	AT_TEST_ASSERT(freeList.GetAvailable() == 12288);
	AT_TEST_ASSERT(freeList.GetLargestFree() == 12288);
	AT_TEST_ASSERT(freeList.Validate(image));

	return 0;
}
#endif
