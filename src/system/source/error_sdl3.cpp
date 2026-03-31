//	Altirra - Atari 800/800XL/5200 emulator
//	System library - error display for non-Windows

#include <stdafx.h>
#include <stdio.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/error.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

void VDPostException(VDExceptionPostContext context, const char *message, const char *title) {
	fprintf(stderr, "Error [%s]: %s\n", title ? title : "", message ? message : "");
	fflush(stderr);
}

void VDPostException(VDExceptionPostContext context, const wchar_t *message, const wchar_t *title) {
	VDStringA msg  = message ? VDTextWToU8(message, -1) : VDStringA();
	VDStringA ttl  = title   ? VDTextWToU8(title,   -1) : VDStringA();
	fprintf(stderr, "Error [%s]: %s\n", ttl.c_str(), msg.c_str());
	fflush(stderr);
}

