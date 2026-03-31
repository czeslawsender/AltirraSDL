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

#include "stdafx.h"
#include <vd2/system/binary.h>
#include <vd2/system/file.h>
#include <vd2/system/memory.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>

namespace {
	IVDDisplayImageDecoder *g_pVDDisplayImageDecoder;
}

void VDDisplaySetImageDecoder(IVDDisplayImageDecoder *p) {
	g_pVDDisplayImageDecoder = p;
}

////////////////////////////////////////////////////////////////////////////////

VDFileParseException::VDFileParseException(uint32 line, const char *error)
	: VDException("Parse error at line %u: %s.", line, error) {}

////////////////////////////////////////////////////////////////////////////////

VDDCsPropKey::VDDCsPropKey(VDStringSpanA name)
	: VDDCsPropKey(VDDCsPropKeyView(name))
{
}

////////////////////////////////////////////////////////////////////////////////

VDDCsPropKeyView::VDDCsPropKeyView(VDStringSpanA name)
	: mCounter(UINT32_MAX)
{
	uint32 count = 0;
	bool hasDigits = false;

	VDStringSpanA name2(name);
	uint32 digit = 1;
	while(!name2.empty()) {
		const char c = name2.back();

		if (c < '0' || c > '9') {
			if (!hasDigits)
				break;

			mBaseName = name2;
			mCounter = count;
			return;
		}

		hasDigits = true;

		if (count > (UINT32_MAX / 10))
			break;

		const uint32 inc = (uint32)(c - '0');
		if ((UINT32_MAX - count) / digit < inc)
			break;

		count += inc * digit;
		digit *= 10;

		name2.remove_suffix(1);
	}

	mBaseName = name;
}

VDStringA VDDCsPropKeyView::ToString() const {
	VDStringA s(mBaseName);

	if (mCounter != UINT32_MAX)
		s.append_sprintf("%u", mCounter);

	return s;
}

////////////////////////////////////////////////////////////////////////////////

void VDDisplayCustomShaderProps::ParseFromFile(VDTextInputFile& file) {
	uint32 lineno = 0;

	const VDCharMaskA whitespace(" \r\n\t\v");
	const VDCharMaskA hashOrEqual("#=");
	const VDCharMaskA whitespaceOrHash(" \r\n\t\v#");

	for(;;) {
		++lineno;

		const char *s = file.GetNextLine();
		if (!s)
			break;

		VDStringRefA line(s);

		line = line.trim(whitespace);
		if (line.empty() || line[0] == '#')
			continue;

		if (line.starts_with("//"))
			continue;

		auto eqpos = line.find_first_of(hashOrEqual);

		if (eqpos == line.npos || line[eqpos] == '#')
			throw VDFileParseException(lineno, "expected '=' after key");

		VDStringSpanA key(line.subspan(0, eqpos).trim(whitespace));
		if (key.empty())
			throw VDFileParseException(lineno, "expected key");

		line = line.subspan(eqpos + 1).trim_start(whitespace);

		if (line.empty() || line[0] == '#')
			throw VDFileParseException(lineno, "expected value");

		VDStringSpanA value;
		if (line[0] == '"') {
			auto quotePos = line.find('"', 1);

			if (quotePos == line.npos)
				throw VDFileParseException(lineno, "missing '\"' at end of value string");

			value = line.subspan(1, quotePos - 1);

			line.remove_prefix(quotePos + 1);

			// consume whitespace 
			line = line.trim(whitespace);

			// check for comment or garbage
			if (!line.empty() && !line.starts_with('#') && !line.starts_with("//"))
				throw VDFileParseException(lineno, "expected end of line");
		} else {
			value = line.subspan(0, line.find('#')).trim(whitespace);
		}

		if (!Add(VDDCsPropKeyView(key), value)) {
			VDStringA err;
			err.sprintf("duplicate key '%.*hs'", (int)key.size(), key.data());
			throw VDFileParseException(lineno, err.c_str());
		}
	}
}

const char *VDDisplayCustomShaderProps::GetString(const VDDCsPropKeyView& key) const {
	auto it = mProps.find_as(key);

	return it != mProps.end() ? it->second.c_str() : nullptr;
}

std::optional<int> VDDisplayCustomShaderProps::TryGetInt(const VDDCsPropKeyView& key) const {
	const char *s = GetString(key);

	if (!s)
		return std::nullopt;

	errno = 0;

	long v = strtol(s, nullptr, 10);
	if (errno || v < INT_MIN || v > INT_MAX)
		throw VDException("Expected integer for '%s'", key.ToString().c_str());

	return (int)v;
}

bool VDDisplayCustomShaderProps::GetBool(const VDDCsPropKeyView& key, bool defaultValue) const {
	auto it = mProps.find_as(key);
	if (it == mProps.end())
		return defaultValue;

	const auto& value = it->second;

	return value != "false" && value != "0";
}

bool VDDisplayCustomShaderProps::Add(const VDDCsPropKeyView& key, const VDStringSpanA& value) {
	auto r = mProps.insert_as(key);
	if (!r.second)
		return false;

	r.first->second = value;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void VDDLoadCustomShaderTexture(VDPixmapBuffer& buf, const wchar_t *path) {
	VDFileStream fs(path);
	VDBufferedRandomAccessStream bs(&fs, 4096);

	// read TARGA header
	uint8 header[18];
	bs.Read(header, 18);

	// check if it might actually be a PNG
	static constexpr uint8 kPNGHeader[8]={
		137, 80, 78, 71, 13, 10, 26, 10
	};

	if (g_pVDDisplayImageDecoder && !memcmp(header, kPNGHeader, 8)) {
		bs.Seek(0);
		if (!g_pVDDisplayImageDecoder->DecodeImage(buf, bs))
			throw VDException(L"Unsupported image format: %ls", path);

		// ensure that buffer is BGRA with top-down orientation
		if (buf.format != nsVDPixmap::kPixFormat_XRGB8888 || buf.pitch < 0) {
			VDPixmapBuffer buf2(buf.w, buf.h, nsVDPixmap::kPixFormat_XRGB8888);

			VDPixmapBlt(buf2, buf);

			buf = std::move(buf2);
		}

	} else {
		// load as TARGA
		const uint8 idSize = header[0];
		const uint8 dataType = header[2];
		const uint32 width = VDReadUnalignedLEU16(&header[12]);
		const uint32 height = VDReadUnalignedLEU16(&header[14]);
		const uint8 imagePixelSize = header[16];
		const uint8 imageAlphaBits = header[17] & 15;

		if (!width || !height || width > 16384 || height > 16384 || (width & (width - 1)) || (height & (height - 1)))
			throw VDException("Unsupported TARGA image size: %ux%u", width, height);

		uint32 bpp;
		if ((dataType == 2 || dataType == 10) && imagePixelSize == 24 && imageAlphaBits == 0) {
			bpp = 3;
		} else if ((dataType == 2 || dataType == 10) && imagePixelSize == 32 && imageAlphaBits == 0) {
			bpp = 4;
		} else if ((dataType == 2 || dataType == 10) && imagePixelSize == 32 && imageAlphaBits == 8) {
			bpp = 4;
		} else
			throw VDException("TARGA image must be 24-bit or 32-bit.");

		// read image raw buffer
		vdblock<uint8> rawBuffer;
	
		if (dataType == 10)
			rawBuffer.resize(std::min<uint32>(bpp * width * height * 2, (uint32)(fs.size() - (18 + idSize))));
		else
			rawBuffer.resize(bpp * width * height);

		bs.Seek(18 + idSize);
		bs.Read(rawBuffer.data(), (sint32)rawBuffer.size());

		// read/decompress image
		class DecompressionException : public MyError {
		public:
			DecompressionException() : MyError("TARGA decompression error.") {}
		};

		if (dataType == 10) {
			vdblock<uint8> decompBuffer(bpp * width * height);

			const uint8 *src = rawBuffer.data();
			const uint8 *srcEnd = src + rawBuffer.size();
			uint8 *dst = decompBuffer.data();
			uint8 *dstEnd = dst + decompBuffer.size();

			while(dstEnd != dst) {
				if ((size_t)(srcEnd - src) < bpp + 1)
					throw DecompressionException();

				const uint8 code = *src++;
				const uint32 rawcnt = (uint32)(code & 0x7F) + 1;
				const uint32 rawlen = rawcnt * bpp;
				if ((size_t)(dstEnd - dst) < rawlen)
					throw DecompressionException();

				if (code & 0x80) {
					uint8 c0 = *src++;
					uint8 c1 = *src++;
					uint8 c2 = *src++;
					if (bpp == 3) {
						for(uint32 i=0; i<rawcnt; ++i) {
							*dst++ = c0;
							*dst++ = c1;
							*dst++ = c2;
						}
					} else {
						uint8 c3 = *src++;

						for(uint32 i=0; i<rawcnt; ++i) {
							*dst++ = c0;
							*dst++ = c1;
							*dst++ = c2;
							*dst++ = c3;
						}
					}
				} else {
					if ((size_t)(srcEnd - src) < rawlen)
						throw DecompressionException();

					memcpy(dst, src, rawlen);
					dst += rawlen;
					src += rawlen;
				}
			}

			rawBuffer.swap(decompBuffer);
		}

		buf.init(width, height, nsVDPixmap::kPixFormat_ARGB8888);

		const void *src0 = rawBuffer.data();
		ptrdiff_t srcModulo = 0;

		if (!(header[17] & 0x20)) {
			src0 = (const char *)src0 + width * bpp * (height - 1);
			srcModulo = 0 - 2*width*bpp;
		}

		if (bpp == 3) {
			const uint8 *src = (const uint8 *)src0;
			uint8 *dstRow = (uint8 *)buf.data;

			for(uint32 y=0; y<height; ++y) {
				uint8 *dst = dstRow;
				dstRow += buf.pitch;

				for(uint32 x=0; x<width; ++x) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = (uint8)0xFF;
					dst += 4;
					src += 3;
				}

				src += srcModulo;
			}
		} else if (bpp == 4) {
			if (imageAlphaBits == 0) {
				const uint32 *src = (const uint32 *)src0;
				uint32 *dstRow = (uint32 *)buf.data;

				for(uint32 y=0; y<height; ++y) {
					uint32 *dst = dstRow;
					dstRow = (uint32 *)((char *)dstRow + buf.pitch);

					for(uint32 x=0; x<width; ++x) {
						dst[x] = src[x] | UINT32_C(0xFF000000);
					}

					src = (const uint32 *)((const char *)(src + width) + srcModulo);
				}				
			} else
				VDMemcpyRect(buf.data, buf.pitch, rawBuffer.data(), width*4, width*4, height);
		}
	}
}
