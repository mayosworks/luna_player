//=============================================================================
// Music player Luna
//                                                Copyright (c) 2005-2023 MAYO.
//=============================================================================

#include <windows.h>
#include <crtdbg.h>
#include "luna.h"

//-----------------------------------------------------------------------------
// アプリケーションエントリポイント
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE cur_inst, HINSTANCE /*prev_inst*/, LPSTR /*cmd_line*/, int /*cmd_show*/)
{
#ifdef _DEBUG
	// メモリリークを検出させる
	int tmp_flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmp_flag |= _CRTDBG_LEAK_CHECK_DF;
	_CrtSetDbgFlag(tmp_flag);
#endif // _DEBUG

	static Luna luna;
	int result = luna.Main(cur_inst);

	return result;
}
