//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2009 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "wx_misc.h"

namespace wx {

//-----------------------------------------------------------------------------
// アサート
//-----------------------------------------------------------------------------
void Assert(const char* file, int line, const char* exp, const char* msg, ...)
{
	char buf[1024];

	int len = wsprintfA(buf, "%s(%d):\n\nExp:%s\nMsg:", file, line, exp);
	if (len < 0) {
		len = 0;
	}

	if (msg) {
		va_list args;

		va_start(args, msg);
		wvsprintfA(&buf[len], msg, args);
		va_end(args);

		buf[sizeof(buf) - 1] = '\0';
	}
	else {
		lstrcpyA(&buf[len], "(No Message)");
	}

	OutputDebugStringA(buf);
	OutputDebugString(TEXT("\n"));

	MessageBoxA(GetForegroundWindow(), buf, "Fatal Error!", MB_ICONERROR);
	DebugBreak();

	const int EXIT_FAILED = 1;
	FatalExit(EXIT_FAILED);
}

//-----------------------------------------------------------------------------
// トレースログ出力
//-----------------------------------------------------------------------------
void Trace(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	wvsprintfA(buf, fmt, ap);
	va_end(ap);

	OutputDebugStringA(buf);
}

//-----------------------------------------------------------------------------
// システムエラー表示
//-----------------------------------------------------------------------------
void DisplayError()
{
	void* buf = NULL;
	TCHAR* msg = reinterpret_cast<TCHAR*>(&buf);

	// システムエラー取得
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, 0, NULL);

	OutputDebugString(msg);
	MessageBox(GetForegroundWindow(), msg, NULL, MB_ICONERROR);

	LocalFree(buf);
}

} //namespace wx
