// Compatibility shim: provides minimal tchar.h on non-Windows
#ifndef _COMPAT_TCHAR_H
#define _COMPAT_TCHAR_H

#include <string.h>

typedef char TCHAR;
#define _T(x) x
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcscat strcat
#define _tcscmp strcmp

#endif
