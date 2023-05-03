//=============================================================================
// 追加中のキャンセルダイアログ
//=============================================================================
#pragma once

//-----------------------------------------------------------------------------
// 追加中のキャンセルダイアログ
//-----------------------------------------------------------------------------
class CancelDialog
{
public:
	static bool ShowDialog(HWND parent);
	static void HideDialog();

	static void SetPath(const wchar_t* path);

	static bool IsCanceled();

private:
	static CancelDialog& GetInstance();

	CancelDialog();
	~CancelDialog();

	static INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
	BOOL HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

	// メッセージ毎ハンドラ
	BOOL OnInitDialog(HWND dlg, WPARAM wp, LPARAM lp);
	BOOL OnCommand   (HWND dlg, WPARAM wp, LPARAM lp);


private:
	HWND	m_prog;
	bool	m_cancel;
	DWORD	m_time;
};
