//	Altirra SDL3 frontend - oshelper.h stub implementations
//	On Linux, built-in AltirraOS kernel ROMs are embedded as C arrays.
//	ATLoadKernelResource loads from these embedded blobs instead of
//	Windows resources.

#include <stdafx.h>
#include <string.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "oshelper.h"
#include "resource.h"

// Embedded ROM data (built from src/Kernel/ and src/ATBasic/ via MADS)
#include "../romdata/kernel_rom.h"
#include "../romdata/kernelxl_rom.h"
#include "../romdata/nokernel_rom.h"

struct EmbeddedROM {
	int resourceId;
	const unsigned char *data;
	unsigned int size;
};

static const EmbeddedROM kEmbeddedROMs[] = {
	{ IDR_KERNEL,   kernel_rom,   kernel_rom_len },
	{ IDR_KERNELXL, kernelxl_rom, kernelxl_rom_len },
	{ IDR_NOKERNEL, nokernel_rom, nokernel_rom_len },
};

static const EmbeddedROM *FindROM(int resId) {
	for (const auto& rom : kEmbeddedROMs) {
		if (rom.resourceId == resId)
			return &rom;
	}
	return nullptr;
}

const void *ATLockResource(uint32 resId, size_t& size) {
	const EmbeddedROM *rom = FindROM(resId);
	if (rom) {
		size = rom->size;
		return rom->data;
	}
	size = 0;
	return nullptr;
}

bool ATLoadKernelResource(int resId, void *dst, uint32 offset, uint32 len, bool) {
	const EmbeddedROM *rom = FindROM(resId);
	if (!rom)
		return false;

	if (offset >= rom->size)
		return false;

	uint32 avail = rom->size - offset;
	uint32 toCopy = len < avail ? len : avail;
	memcpy(dst, rom->data + offset, toCopy);

	// Zero-fill remainder if requested more than available
	if (toCopy < len)
		memset((char *)dst + toCopy, 0, len - toCopy);

	return true;
}

bool ATLoadKernelResource(int resId, vdfastvector<uint8>& buf) {
	const EmbeddedROM *rom = FindROM(resId);
	if (!rom)
		return false;

	buf.resize(rom->size);
	memcpy(buf.data(), rom->data, rom->size);
	return true;
}

bool ATLoadKernelResourceLZPacked(int, vdfastvector<uint8>&) {
	return false;
}

bool ATLoadMiscResource(int, vdfastvector<uint8>&) {
	return false;
}

bool ATLoadImageResource(uint32, VDPixmapBuffer&) {
	return false;
}

void ATFileSetReadOnlyAttribute(const wchar_t*, bool) {}
