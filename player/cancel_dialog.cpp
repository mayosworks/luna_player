//=============================================================================
// プラグイン設定
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include "cancel_dialog.h"
#include "resource.h"

// 定数定義
namespace {
const DWORD ELAPSED_TIME = 500;

} //namespace

//-----------------------------------------------------------------------------
// 進行状況ダイアログ表示
//-----------------------------------------------------------------------------
bool CancelDialog::ShowDialog(HWND parent)
{
	HINSTANCE inst = GetModuleHandle(NULL);
	HWND prog = CreateDialog(inst, MAKEINTRESOURCE(IDD_CANCEL), parent, DialogProc);
	if (!prog) {
		return false;
	}

	RECT prt, crt;

	GetWindowRect(parent, &prt);
	GetWindowRect(prog, &crt);

	int x = prt.left + ((prt.right - prt.left) - (crt.right- crt.left)) / 2;
	int y = prt.top  + ((prt.bottom - prt.top) - (crt.bottom- crt.top)) / 2;

	SetWindowPos(prog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	ShowWindow(prog, SW_SHOWNORMAL);
	UpdateWindow(prog);

	GetInstance().m_prog = prog;
	GetInstance().m_cancel = false;
	return true;
}

//-----------------------------------------------------------------------------
// 進行状況ダイアログ消し
//-----------------------------------------------------------------------------
void CancelDialog::HideDialog()
{
	if (GetInstance().m_prog) {
		DestroyWindow(GetInstance().m_prog);
		GetInstance().m_prog = NULL;
	}
}

//-----------------------------------------------------------------------------
// ファイルパス設定
//-----------------------------------------------------------------------------
void CancelDialog::SetPath(const wchar_t* path)
{
	HWND prog = GetInstance().m_prog;
	if (prog) {
		DWORD time = GetTickCount();
		DWORD elapsed = time - GetInstance().m_time;

		// 一定間隔おきに更新
		if (elapsed > ELAPSED_TIME) {
			SetDlgItemText(prog, IDC_PATH, path);
			GetInstance().m_time = time;
		}
	}
}

//-----------------------------------------------------------------------------
// キャンセルしたかどうか？
//-----------------------------------------------------------------------------
bool CancelDialog::IsCanceled()
{
	return GetInstance().m_cancel;
}

//-----------------------------------------------------------------------------
// 設定ダイアログインスタンスを返す
//-----------------------------------------------------------------------------
CancelDialog& CancelDialog::GetInstance()
{
	static CancelDialog prg;
	return prg;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CancelDialog::CancelDialog()
	: m_prog(NULL)
	, m_cancel(false)
	, m_time(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CancelDialog::~CancelDialog()
{
}

//-----------------------------------------------------------------------------
// プラグイン設定ダイアログプロシージャー
//-----------------------------------------------------------------------------
INT_PTR CancelDialog::DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static CancelDialog& prg = GetInstance();
	return prg.HandleEvent(dlg, msg, wp, lp);
}

//-----------------------------------------------------------------------------
// イベントハンドラ
//-----------------------------------------------------------------------------
BOOL CancelDialog::HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_INITDIALOG:
		return OnInitDialog(dlg, wp, lp);

	case WM_COMMAND:
		return OnCommand(dlg, wp, lp);
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// ダイアログ初期化メッセージ処理
//-----------------------------------------------------------------------------
BOOL CancelDialog::OnInitDialog(HWND dlg, WPARAM /*wp*/, LPARAM /*lp*/)
{
	SetDlgItemText(dlg, IDC_PATH, L"");
	return TRUE;
}

//-----------------------------------------------------------------------------
// コマンドメッセージ処理
//-----------------------------------------------------------------------------
BOOL CancelDialog::OnCommand(HWND /*dlg*/, WPARAM wp, LPARAM /*lp*/)
{
	switch (LOWORD(wp)) {
	case IDCANCEL:
		m_cancel = true;
		break;

	default:
		return FALSE;
	}

	return TRUE;
}
