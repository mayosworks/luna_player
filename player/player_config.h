//=============================================================================
// プレイヤー設定
//=============================================================================
#pragma once

#include <windows.h>


//-----------------------------------------------------------------------------
// プレイヤー設定ダイアログ
//-----------------------------------------------------------------------------
class PlayerConfig
{
public:
	static BOOL ShowDialog(HWND parent);


private:
	static PlayerConfig& GetInstance();

	PlayerConfig();
	~PlayerConfig();

	static INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
	INT_PTR HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

	// メッセージ毎ハンドラ
	INT_PTR OnInitDialog(HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnCommand   (HWND dlg, WPARAM wp, LPARAM lp);

	void ApplyConfig(HWND dlg);
	void ResetConfig(HWND dlg);
};
