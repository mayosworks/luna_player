//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================

// バルーンを使用するために必要な、IEバージョン定義
#ifdef _WIN32_IE
#if _WIN32_IE < 0x0501
#undef _WIN32_IE
#define _WIN32_IE 0x0501	
#endif //_WIN32_IE < 0x0501
#else
#define _WIN32_IE 0x0501
#endif //_WIN32_IE

#include <windows.h>
#include "wx_misc.h"
#include "wx_task_tray.h"

namespace wx {

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
TaskTray::TaskTray()
	: m_hwnd(NULL)
	, m_id(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
TaskTray::~TaskTray()
{
}

//-----------------------------------------------------------------------------
// タスクバー再起動のメッセージ(WM_TASKBARCREATED)を登録
//-----------------------------------------------------------------------------
UINT TaskTray::RegisterTaskbarMessage()
{
	return RegisterWindowMessage(TEXT("TaskbarCreated"));
}

//-----------------------------------------------------------------------------
// 通知領域アイコンを設定する
//-----------------------------------------------------------------------------
bool TaskTray::Initialize(HWND hwnd, UINT id, PCTSTR tip, HICON icon, UINT msg)
{
	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize	= sizeof(nid);
	nid.uFlags	= NIF_ICON | (tip? NIF_TIP : 0) | (msg? NIF_MESSAGE : 0);
	nid.hWnd	= hwnd;
	nid.uID		= id;
	nid.hIcon	= icon;

	// コールバックメッセージ
	nid.uCallbackMessage = msg;

	if (tip) {
		lstrcpyn(nid.szTip, tip, sizeof(nid.szTip) / sizeof(TCHAR));
	}

	if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
		return false;
	}

	m_hwnd = hwnd;
	m_id = id;
	return true;
}

//-----------------------------------------------------------------------------
// 通知領域アイコンを消す
//-----------------------------------------------------------------------------
void TaskTray::Finalize()
{
	if (!m_id) {
		return;
	}

	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize	= sizeof(nid);
	nid.uFlags	= NIF_ICON | NIF_INFO;
	nid.hWnd	= m_hwnd;
	nid.uID		= m_id;

	if (Shell_NotifyIcon(NIM_DELETE, &nid)) {
		m_hwnd = NULL;
		m_id   = 0;
	}
}

//-----------------------------------------------------------------------------
// 通知領域アイコンを再設定する
//-----------------------------------------------------------------------------
bool TaskTray::Reset(PCTSTR tip, HICON icon, UINT msg)
{
	if (!m_id) {
		return false;
	}

	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize	= sizeof(nid);
	nid.uFlags	= NIF_ICON | (tip? NIF_TIP : 0) | (msg? NIF_MESSAGE : 0);
	nid.hWnd	= m_hwnd;
	nid.uID		= m_id;
	nid.hIcon	= icon;

	// コールバックメッセージ
	nid.uCallbackMessage = msg;

	if (tip) {
		lstrcpyn(nid.szTip, tip, sizeof(nid.szTip) / sizeof(TCHAR));
	}

	if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// バルーンを表示
//-----------------------------------------------------------------------------
bool TaskTray::ShowInfo(PCTSTR title, PCTSTR message, UINT timeout)
{
	if (!m_id) {
		return false;
	}

	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize		= sizeof(nid);
	nid.uFlags		= NIF_INFO;
	nid.hWnd		= m_hwnd;
	nid.uID			= m_id;
	nid.uTimeout	= timeout;
	nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;

	lstrcpyn(nid.szInfoTitle, title, sizeof(nid.szInfoTitle) / sizeof(TCHAR));
	lstrcpyn(nid.szInfo,    message, sizeof(nid.szInfo)      / sizeof(TCHAR));

	Shell_NotifyIcon(NIM_MODIFY, &nid);
	return true;
}

//-----------------------------------------------------------------------------
// バルーンを消す
//-----------------------------------------------------------------------------
void TaskTray::HideInfo()
{
	if (!m_id) {
		return;
	}

	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));

	nid.cbSize		= sizeof(nid);
	nid.uFlags		= NIF_INFO;
	nid.hWnd		= m_hwnd;
	nid.uID			= m_id;

	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

} //namespace wx
