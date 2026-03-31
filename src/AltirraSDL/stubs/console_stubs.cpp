//	Altirra SDL3 frontend - console.h stub implementations
//	Provides non-UI console/debug output functions for Linux build.

#include <stdafx.h>
#include <stdio.h>
#include <stdarg.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include "console.h"

void ATConsoleOpenLogFile(const wchar_t*) {}
void ATConsoleCloseLogFileNT() {}
void ATConsoleCloseLogFile() {}

void ATConsoleWrite(const char *s) {
	fputs(s, stderr);
}

void ATConsolePrintfImpl(const char *format, ...) {
	va_list val;
	va_start(val, format);
	vfprintf(stderr, format, val);
	va_end(val);
}

void ATConsoleTaggedPrintfImpl(const char *format, ...) {
	va_list val;
	va_start(val, format);
	vfprintf(stderr, format, val);
	va_end(val);
}

void ATConsoleSetTraceLogger(vdfunction<void(const char*, uint64)>) {}

void ATConsoleGetFont(struct tagLOGFONTW&, int&) {}
void ATConsoleGetCharMetrics(int&, int&) {}
void ATConsoleSetFont(const struct tagLOGFONTW&, int) {}
void ATConsoleSetFontDpi(unsigned) {}

bool ATConsoleShowSource(uint32) { return false; }

void ATShowConsole() {}
void ATOpenConsole() {}
void ATCloseConsole() {}
bool ATIsDebugConsoleActive() { return false; }

IATSourceWindow *ATGetSourceWindow(const wchar_t*) { return nullptr; }
IATSourceWindow *ATOpenSourceWindow(const wchar_t*) { return nullptr; }
IATSourceWindow *ATOpenSourceWindow(const ATDebuggerSourceFileInfo&, bool) { return nullptr; }
void ATUIShowSourceListDialog() {}

void ATGetUIPanes(vdfastvector<ATUIPane*>&) {}
ATUIPane *ATGetUIPane(uint32) { return nullptr; }
void *ATGetUIPaneAs(uint32, uint32) { return nullptr; }
ATUIPane *ATGetUIPaneByFrame(ATFrameWindow*) { return nullptr; }
void ATCloseUIPane(uint32) {}

ATUIPane *ATUIGetActivePane() { return nullptr; }
void *ATUIGetActivePaneAs(uint32) { return nullptr; }
uint32 ATUIGetActivePaneId() { return 0; }

bool ATRestorePaneLayout(const char*) { return false; }
void ATSavePaneLayout(const char*) {}
void ATLoadDefaultPaneLayout() {}

VDZHFONT ATGetConsoleFontW32() { return nullptr; }
int ATGetConsoleFontLineHeightW32() { return 16; }
VDZHFONT ATConsoleGetPropFontW32() { return nullptr; }
int ATConsoleGetPropFontLineHeightW32() { return 16; }
VDZHMENU ATUIGetSourceContextMenuW32() { return nullptr; }

void ATConsoleAddFontNotification(const vdfunction<void()>*) {}
void ATConsoleRemoveFontNotification(const vdfunction<void()>*) {}

void ATConsolePingBeamPosition(uint32, uint32, uint32) {}
