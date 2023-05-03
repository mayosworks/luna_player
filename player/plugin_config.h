//=============================================================================
// プラグイン設定
//=============================================================================
#pragma once

class Plugin;

//-----------------------------------------------------------------------------
// プラグイン設定ダイアログ
//-----------------------------------------------------------------------------
class PluginConfig
{
public:
	static BOOL ShowDialog(HWND parent);

private:
	static PluginConfig& GetInstance();

	PluginConfig();
	~PluginConfig();

	static INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
	INT_PTR HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

	// メッセージ毎ハンドラ
	INT_PTR OnInitDialog(HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnCommand   (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnNotify    (HWND dlg, WPARAM wp, LPARAM lp);

	void ApplyConfig(HWND dlg);
	void ResetConfig(HWND dlg);
	void ShowProperty(HWND dlg);

	void SetPluginInfo(HWND list, int index, Plugin* plugin, BOOL checked);
};
