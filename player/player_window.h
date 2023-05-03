//=============================================================================
// プレイヤーウィンドウ
//=============================================================================
#pragma once

//#include <gdiplus.h>
#include "lib/wx_misc.h"
#include "lib/wx_string.h"
#include "lib/wx_task_tray.h"


//-----------------------------------------------------------------------------
// プレイヤー管理クラス
//-----------------------------------------------------------------------------
class Player
{
public:
	Player();
	virtual ~Player();

	// 初期化と終了
	HWND Initialize(HINSTANCE inst);
	void Finalize();

	// メッセージ処理
	bool ProcessMessage(MSG* msg);

	// 設定変更を適用
	void ApplyConfig();

	// 表示更新
	void UpdatePlayList();
	void UpdateListInfo();
	
	// 情報表示
	void SetInfoText1(const wchar_t* text1);
	void SetInfoText2(const wchar_t* text2);

	void SetPlayInfo(const wchar_t* play_info);

	// シークバー設定
	void SeekbarEnable(bool on);
	void SetSeekbar(UINT time, bool seekable);

	// 表示更新タイマー
	void StartTimer();
	void StopTimer();

	// プレイ時間変更
	void ChangeTime(UINT time);

	// 通知領域バルーンメッセージ
	void ShowBalloon(const wchar_t* title, const wchar_t* msg);
	void HideBalloon();

private:
	void OpenFile(HWND owner);
	void OpenFolder(HWND owner);
	void LoadPlayList(HWND owner);
	void SavePlayList(HWND owner);
	void DropFiles(HDROP hdrop);

	// APIへ渡すためのダイアログプロシージャ
	static INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

	// Windowsメッセージハンドラ
	INT_PTR HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

	// メッセージ毎ハンドラ
	INT_PTR OnInitDialog(HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnDestroy   (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnCommand   (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnTimer     (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnHScroll   (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnVScroll   (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnDropFiles (HWND dlg, WPARAM wp, LPARAM lp);
	INT_PTR OnNotify    (HWND dlg, WPARAM wp, LPARAM lp);

private:
	HWND			m_root;
	HWND			m_list;
	UINT			m_tbcm;
	bool			m_moving;
	bool			m_direct;
	bool			m_seekable;
	wx::String		m_play_info;

	// タスクトレイコントロール
	wx::TaskTray	m_tray;
	HMENU			m_popup;
	wx::TaskTray	m_ctrl_ps;
	wx::TaskTray	m_ctrl_pn;
	HICON			m_icon_ps;
	HICON			m_icon_pn;

#if defined(SUPPORT_ARTWORK)
	Gdiplus::Image*	m_artwork;
	HANDLE			m_artdata;
#endif //defined(SUPPORT_ARTWORK)
};
