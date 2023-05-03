//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "wx_application.h"

namespace wx {

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Application::Application()
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Application::~Application()
{
}

//-----------------------------------------------------------------------------
// メイン実行関数
//-----------------------------------------------------------------------------
int Application::Main(HINSTANCE app_inst)
{
	if (!Initialize(app_inst)) {
		return EXIT_FAILED;
	}

	MSG msg;

	for (;;) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Finalize();
				return static_cast<int>(msg.wParam);
			}

			if (!Execute(&msg)) {
				Finalize();
				return EXIT_FAILED;
			}
		}

		WaitMessage();
	}
}

} //namespace wx
