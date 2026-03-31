//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.
#ifndef f_VD2_SYSTEM_STRUTIL_H
#define f_VD2_SYSTEM_STRUTIL_H

#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define _stricmp  strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp  wcscasecmp
#define _wcsnicmp wcsncasecmp
#endif
#include <vd2/system/vdtypes.h>

char *strncpyz(char *strDest, const char *strSource, size_t count);
wchar_t *wcsncpyz(wchar_t *strDest, const wchar_t *strSource, size_t count);
const char *strskipspace(const char *s);

inline char *strskipspace(char *s) {
	return const_cast<char *>(strskipspace((const char *)s));
}

size_t vdstrlcpy(char *dst, const char *src, size_t sizeChars);
size_t vdwcslcpy(wchar_t *dst, const wchar_t *src, size_t sizeChars);

size_t vdstrlcat(char *dst, const char *src, size_t sizeChars);

inline int vdstricmp(const char *s, const char *t) {
	#ifdef _WIN32
		return _stricmp(s, t);
	#else
		return strcasecmp(s, t);
	#endif
}

inline int vdstricmp(const char *s, const char *t, size_t maxlen) {
	#ifdef _WIN32
		return _strnicmp(s, t, maxlen);
	#else
		return strncasecmp(s, t, maxlen);
	#endif
}

inline int vdwcsicmp(const wchar_t *s, const wchar_t *t) {
	#ifdef _WIN32
		return _wcsicmp(s, t);
	#else
		return wcscasecmp(s, t);
	#endif
}

inline int vdwcsnicmp(const wchar_t *s, const wchar_t *t, size_t maxlen) {
	#ifdef _WIN32
		return _wcsnicmp(s, t, maxlen);
	#else
		return wcsncasecmp(s, t, maxlen);
	#endif
}

#endif
