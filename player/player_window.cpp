//=============================================================================
// プレイヤーウィンドウ
//=============================================================================

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include "lib/wx_misc.h"
#include "player_window.h"
#include "player_config.h"
#include "config.h"
#include "media.h"
#include "media_manager.h"
#include "plugin_manager.h"
#include "plugin_config.h"
#include "cancel_dialog.h"
#include "luna.h"
#include "timer.h"
#include "resource.h"

namespace {

// 各種ID
enum ControlID
{
	IDC_BASE = 2000,

	// コントロールID
	IDC_NOTIFY,		// タスクトレイのアプリアイコン
	IDC_PLAYSTOP,	// タスクトレイの再生・停止アイコン
	IDC_PREVNEXT,	// タスクトレイの前へ・次へアイコン

	IDT_BASE = 3000,

	// タイマーID
	IDT_DISP,		// 表示更新
	IDT_SEEK,		// シーク操作
	IDT_NEXT,		// 連続再生
	IDT_HIDE,		// ふきだし消去タイマ
	IDT_INFO,		// 歌詞等
};

const UINT DISP_TIME = 200;		// 表示更新サイクル（ミリ秒）
const UINT WAIT_TIME = 500;		// 操作待機時間
const UINT INFO_TIME = 50;		// 表示更新サイクル（ミリ秒）
const UINT HIDE_TIME = 5000;	// バルーン消し時間
const UINT TIME_OUT  = 1000;	// タイムアウト秒数

const TCHAR* const VERSION_TEXT = TEXT("Luna v1.22.\n\nCopyright (c) 2009-2023 MAYO.");

//-----------------------------------------------------------------------------
// 関数定義
//-----------------------------------------------------------------------------
void ShowManual(HWND owner);
void ShowAbout(HWND owner);

// 「操作説明」(readme.txt)を表示
void ShowManual(HWND owner)
{
	TCHAR dir[MAX_PATH], path[MAX_PATH];

	GetModuleFileName(NULL, dir, WX_NUMBEROF(dir));
	PathRemoveFileSpec(dir);
	PathCombine(path, dir, TEXT("readme.txt"));
	ShellExecute(owner, NULL, path, NULL, dir, SW_SHOWDEFAULT);
}

// 「Lunaについて」を表示
void ShowAbout(HWND owner)
{
	MSGBOXPARAMS params;
	ZeroMemory(&params, sizeof(params));

	params.cbSize		= sizeof(params);
	params.hwndOwner	= owner;
	params.hInstance	= GetModuleHandle(NULL);
	params.lpszText		= VERSION_TEXT;
	params.lpszCaption	= TEXT("Lunaについて");
	params.dwStyle		= MB_OK | MB_USERICON;
	params.lpszIcon		= MAKEINTRESOURCE(IDI_APP);

	MessageBoxIndirect(&params);
}

} //namespace

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Player::Player()
	: m_root(NULL)
	, m_list(NULL)
	, m_tbcm(0)
	, m_moving(false)
	, m_direct(false)
	, m_seekable(false)
	, m_popup(NULL)
	, m_icon_ps(NULL)
	, m_icon_pn(NULL)
#if defined(SUPPORT_ARTWORK)
	, m_artwork(NULL)
	, m_artdata(NULL)
#endif //defined(SUPPORT_ARTWORK)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Player::~Player()
{		
#if defined(SUPPORT_ARTWORK)
	if (m_artwork) {
		delete m_artwork;
		m_artwork = NULL;
	}

	if (m_artdata) {
		GlobalFree(m_artdata);
		m_artdata = NULL;
	}
#endif //defined(SUPPORT_ARTWORK)
}

//-----------------------------------------------------------------------------
// プレイヤー初期化
//-----------------------------------------------------------------------------
HWND Player::Initialize(HINSTANCE inst)
{
	LPARAM lp = reinterpret_cast<LPARAM>(this);
	HWND root = CreateDialogParam(inst, MAKEINTRESOURCE(IDD_PLAYER), NULL, DialogProc, lp);
	if (!root) {
		return NULL;
	}

	ShowWindow(root, SW_SHOW);
	UpdateWindow(root);

	m_root = root;

	ApplyConfig();
	Luna::Stop();
	return root;
}

//-----------------------------------------------------------------------------
// プレイヤー終了
//-----------------------------------------------------------------------------
void Player::Finalize()
{
	if (m_root) {
		DestroyWindow(m_root);
		m_root = NULL;
		m_list = NULL;
	}
}

//-----------------------------------------------------------------------------
// 設定変更を適用
//-----------------------------------------------------------------------------
void Player::ApplyConfig()
{
	Config& conf = Luna::GetConfig();

	HWND insertAfter = conf.always_top? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(m_root, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	if (conf.use_task_tray) {
		// タスクトレイには、作成と逆の順で入るので、後ろからつくる
		if (conf.use_tray_ctrl) {
			m_ctrl_pn.Initialize(m_root, IDC_PREVNEXT, L"前へ／次へ", m_icon_pn, MSG_TASKTRAY);
			m_ctrl_ps.Initialize(m_root, IDC_PLAYSTOP, L"再生／停止", m_icon_ps, MSG_TASKTRAY);
		}
		else {
			m_ctrl_ps.Finalize();
			m_ctrl_pn.Finalize();
		}

		HICON h_icon = reinterpret_cast<HICON>(SendMessage(m_root, WM_GETICON, ICON_SMALL, 0));
		m_tray.Initialize(m_root, IDC_NOTIFY, APP_NAME, h_icon, MSG_TASKTRAY);

		if (!conf.use_balloon) {
			m_tray.HideInfo();
			KillTimer(m_root, IDT_HIDE);
		}
	}
	else {
		m_tray.Finalize();
		m_ctrl_ps.Finalize();
		m_ctrl_pn.Finalize();
	}

	HMENU menu = GetMenu(m_root);

	CheckMenuItem(menu, IDM_TOPMOST, MF_BYCOMMAND | (conf.always_top? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, IDM_NORMAL, MF_BYCOMMAND | ((conf.play_mode == PLAY_NORMAL)? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, IDM_REPEAT, MF_BYCOMMAND | ((conf.play_mode == PLAY_REPEAT)? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, IDM_SINGLE, MF_BYCOMMAND | ((conf.play_mode == PLAY_SINGLE)? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, IDM_RANDOM, MF_BYCOMMAND | ((conf.play_mode == PLAY_RANDOM)? MF_CHECKED : MF_UNCHECKED));
}

//-----------------------------------------------------------------------------
// プレイリスト更新
//-----------------------------------------------------------------------------
void Player::UpdatePlayList()
{
	int num = MediaManager::GetNum();

	ListView_DeleteAllItems(m_list);
	ListView_SetItemCountEx(m_list, num, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

	UpdateListInfo();
}

//-----------------------------------------------------------------------------
// リスト情報更新
//-----------------------------------------------------------------------------
void Player::UpdateListInfo()
{
#if 0 // 無効化
	SetInfoText2(TEXT(""));

	int total = MediaManager::GetRawNum();
	int num = MediaManager::GetNum();
 
	// 条件に一致しない時はそのメッセージを出す
	if (MediaManager::IsFiltered() && (total != 0) && (num == 0)) {
		SetInfoText2(TEXT("検索条件にあう曲はありません"));
	}
#endif //0
}

//-----------------------------------------------------------------------------
// メインメッセージ１設定
//-----------------------------------------------------------------------------
void Player::SetInfoText1(const wchar_t* text1)
{
	SetDlgItemText(m_root, IDC_MSG1, text1);
}

//-----------------------------------------------------------------------------
// メインメッセージ２設定
//-----------------------------------------------------------------------------
void Player::SetInfoText2(const wchar_t* text2)
{
	SetDlgItemText(m_root, IDC_MSG2, text2);
}

//-----------------------------------------------------------------------------
// 再生情報設定
//-----------------------------------------------------------------------------
void Player::SetPlayInfo(const wchar_t* play_info)
{
	m_play_info = play_info;

	UINT state = m_play_info.IsEmpty()? (MF_DISABLED | MF_GRAYED) : (MF_ENABLED);
	EnableMenuItem(GetMenu(m_root), IDM_PLAYINFO, MF_BYCOMMAND | state);
}

//-----------------------------------------------------------------------------
// シークバーの有効設定
//-----------------------------------------------------------------------------
void Player::SeekbarEnable(bool on)
{
	HWND seekbar = GetDlgItem(m_root, IDC_SEEK);
	EnableWindow(seekbar, on);
	if (on) {
		SetFocus(seekbar);
	}
}

//-----------------------------------------------------------------------------
// シークバーの設定
//-----------------------------------------------------------------------------
void Player::SetSeekbar(UINT time, bool seekable)
{
	int seek_time = time / 100;

	HWND seekbar = GetDlgItem(m_root, IDC_SEEK);

	SendMessage(seekbar, TBM_SETRANGEMIN, TRUE, 0);
	SendMessage(seekbar, TBM_SETRANGEMAX, TRUE, seek_time);
	SendMessage(seekbar, TBM_SETPOS,      TRUE, 0);
	SendMessage(seekbar, TBM_SETPAGESIZE, 0, 50);
	SendMessage(seekbar, TBM_SETLINESIZE, 0, 30);
	EnableWindow(seekbar, seekable);

	TCHAR buf[32];

	if (time > 0) {
		int disp_time = time / 1000;
		wsprintf(buf, TEXT("%d:%02d"), disp_time / 60, disp_time % 60);
		SetDlgItemText(m_root, IDC_LENGTH, buf);
	}
	else {
		SetDlgItemText(m_root, IDC_TIME,   TEXT("--:--"));
		SetDlgItemText(m_root, IDC_LENGTH, TEXT("--:--"));
	}

	m_seekable = seekable;
}

//-----------------------------------------------------------------------------
// 表示更新タイマーを開始する
//-----------------------------------------------------------------------------
void Player::StartTimer()
{
	SetTimer(m_root, IDT_DISP, DISP_TIME, NULL);

	if (Luna::GetConfig().show_lyrics && Luna::GetLyrics().IsLoaded()) {
		SetTimer(m_root, IDT_INFO, INFO_TIME, NULL);
	}
}

//-----------------------------------------------------------------------------
// 表示更新タイマーを停止する
//-----------------------------------------------------------------------------
void Player::StopTimer()
{
	KillTimer(m_root, IDT_INFO);
	KillTimer(m_root, IDT_DISP);
}

//-----------------------------------------------------------------------------
// 演奏時間変更を表示
//-----------------------------------------------------------------------------
void Player::ChangeTime(UINT time)
{
	// 移動中は変更しない
	if (m_moving || m_direct) {
		return;
	}

	TCHAR buf[16];

	int disp_time = time / 1000;
	int seek_time = time / 100;

	wsprintf(buf, TEXT("%d:%02d"), disp_time / 60, disp_time % 60);
	SetDlgItemText(m_root, IDC_TIME, buf);

	SendDlgItemMessage(m_root, IDC_SEEK, TBM_SETPOS, TRUE, seek_time);

#if defined(SUPPORT_ARTWORK)
	if (m_artwork && m_artdata) {
		HWND ctrl = GetDlgItem(m_root, IDC_ARTWORK);
		HDC hdc = GetDC(ctrl);
		RECT rt;
		GetWindowRect(ctrl, &rt);

		Gdiplus::RectF rect(0, 0, Gdiplus::REAL(rt.right - rt.left), Gdiplus::REAL(rt.bottom - rt.top));
		Gdiplus::Graphics graphics(hdc);
		graphics.DrawImage(m_artwork, rect);
		ReleaseDC(ctrl, hdc);
	}
#endif //defined(SUPPORT_ARTWORK)
}

//-----------------------------------------------------------------------------
// 通知領域にバルーンを表示する
//-----------------------------------------------------------------------------
void Player::ShowBalloon(const wchar_t* title, const wchar_t* msg)
{
	if (!Luna::GetConfig().use_task_tray || !Luna::GetConfig().use_balloon) {
		return;
	}

	m_tray.ShowInfo(title, msg, TIME_OUT);
	SetTimer(m_root, IDT_HIDE, HIDE_TIME, NULL);
}

//-----------------------------------------------------------------------------
// 通知領域のバルーンを消す
//-----------------------------------------------------------------------------
void Player::HideBalloon()
{
	if (!Luna::GetConfig().use_task_tray || !Luna::GetConfig().use_balloon) {
		return;
	}

	m_tray.HideInfo();
	KillTimer(m_root, IDT_HIDE);
}

#if defined(SUPPORT_ARTWORK)
//-----------------------------------------------------------------------------
// アートワーク設定
//-----------------------------------------------------------------------------
void Player::set_artwork(HANDLE artwork)
{
	if (m_artdata) {
		GlobalFree(m_artdata);
		m_artdata = NULL;
	}

	if (m_artwork) {
		delete m_artwork;
		m_artwork = NULL;
	}

	m_artdata = artwork;
	if (artwork) {
		IStream* stream = NULL;
		CreateStreamOnHGlobal(artwork, FALSE, &stream);
		if (stream) {
			m_artwork = new Gdiplus::Image(stream, FALSE);
			stream->Release();
		}
	}

	HWND ctrl = GetDlgItem(m_root, IDC_ARTWORK);
	HDC hdc = GetDC(ctrl);
	RECT rt;
	GetWindowRect(ctrl, &rt);
	FillRect(hdc, &rt, (HBRUSH)GetStockObject(GRAY_BRUSH));
	ReleaseDC(ctrl, hdc);
}
#endif //defined(SUPPORT_ARTWORK)

//-----------------------------------------------------------------------------
// Windowsメッセージ処理
//-----------------------------------------------------------------------------
bool Player::ProcessMessage(MSG* msg)
{
	if (IsDialogMessage(m_root, msg)) {
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// ファイルを開くためのコモンダイアログを開く
//-----------------------------------------------------------------------------
void Player::OpenFile(HWND owner)
{
	OPENFILENAME ofn;
	TCHAR path[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	path[0] = TEXT('\0');

	ofn.lStructSize	= sizeof(ofn);
	ofn.Flags		= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	ofn.hwndOwner	= owner;
	ofn.lpstrTitle	= TEXT("追加するファイルを選択してください");
	ofn.lpstrFilter	= TEXT("すべてのファイル(*.*)\0*.*\0\0");
	ofn.lpstrFile	= path;
	ofn.nMaxFile	= MAX_PATH;

	if (GetOpenFileName(&ofn)) {
		int num = MediaManager::GetNum();
		Luna::Add(path);

		// 追加があった場合だけ
		if (num < MediaManager::GetNum()) {
			UpdatePlayList();
			ListView_EnsureVisible(m_list, num, FALSE);
			ListView_SetItemState(m_list, num, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		}
	}
}

//-----------------------------------------------------------------------------
// フォルダを開くためのコモンダイアログを開く
//-----------------------------------------------------------------------------
void Player::OpenFolder(HWND owner)
{
	IMalloc* malloc = NULL;
	ITEMIDLIST* iidl = NULL;

	if (FAILED(SHGetMalloc(&malloc))) {
		return;
	}

	if (FAILED(SHGetSpecialFolderLocation(owner, CSIDL_DRIVES, &iidl))) {
		malloc->Release();
		return;
	}

	BROWSEINFO bi;

	ZeroMemory(&bi, sizeof(bi));

	bi.hwndOwner = owner;
	bi.pidlRoot  = iidl;
	bi.ulFlags	 = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	bi.lpszTitle = TEXT("追加するフォルダを指定してください");

	LPITEMIDLIST pil = SHBrowseForFolder(&bi);
	if (pil) {
		TCHAR path[MAX_PATH];

		SHGetPathFromIDList(pil, path);
		malloc->Free(pil);

		CancelDialog::ShowDialog(owner);
		EnableWindow(owner, false);

		int num = MediaManager::GetNum();
		Luna::Add(path);

		EnableWindow(m_root, true);
		CancelDialog::HideDialog();

		// 追加があった場合だけ
		if (num < MediaManager::GetNum()) {
			UpdatePlayList();
			ListView_EnsureVisible(m_list, num, FALSE);
			ListView_SetItemState(m_list, num, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		}
	}

	malloc->Free(iidl);
	malloc->Release();
}

//-----------------------------------------------------------------------------
// プレイリストの読み込み
//-----------------------------------------------------------------------------
void Player::LoadPlayList(HWND owner)
{
	OPENFILENAME ofn;
	wchar_t path[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	path[0] = L'\0';

	ofn.lStructSize	= sizeof(ofn);
	ofn.hwndOwner	= owner;
	ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrFile	= path;
	ofn.nMaxFile	= MAX_PATH;
	ofn.lpstrTitle	= TEXT("プレイリストを開く");
	ofn.lpstrFilter	= TEXT("プレイリスト(*.lst;*.m3u;*.m3u8;*.kbm;*.kbmu)\0*.lst;*.m3u;*.m3u8;*.kbm;*.kbmu\0\0");
	ofn.lpstrDefExt	= TEXT(".lst");

	if (GetOpenFileName(&ofn)) {
		MediaManager::LoadList(path);
		UpdatePlayList();
	}
}

//-----------------------------------------------------------------------------
// プレイリストの保存
//-----------------------------------------------------------------------------
void Player::SavePlayList(HWND owner)
{
	OPENFILENAME ofn;
	wchar_t path[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	lstrcpyn(path, L"luna.lst", MAX_PATH);

	ofn.lStructSize	= sizeof(ofn);
	ofn.hwndOwner	= owner;
	ofn.Flags		= OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	ofn.lpstrFile	= path;
	ofn.nMaxFile	= MAX_PATH;
	ofn.lpstrTitle	= TEXT("プレイリストを名前をつけて保存");
	ofn.lpstrFilter	= TEXT("Lunaプレイリスト(*.lst)\0*.lst\0M3Uプレイリスト(*.m3u;*.m3u8)\0*.m3u;*.m3u8\0\0");
	ofn.lpstrDefExt	= TEXT(".lst");

	if (GetSaveFileName(&ofn)) {
		MediaManager::SaveList(path);
	}
}

//-----------------------------------------------------------------------------
// ドロップされたファイルを処理する
//-----------------------------------------------------------------------------
void Player::DropFiles(HDROP hdrop)
{
	UINT files = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
	TCHAR path[MAX_PATH];

	int num = MediaManager::GetNum();
	CancelDialog::ShowDialog(m_root);
	EnableWindow(m_root, false);

	for (UINT i = 0; i < files; ++i) {
		DragQueryFile(hdrop, i, path, MAX_PATH);
		Luna::Add(path);
	}

	EnableWindow(m_root, true);
	CancelDialog::HideDialog();

	DragFinish(hdrop);

	// 追加があった場合だけ
	if (num < MediaManager::GetNum()) {
		UpdatePlayList();
		ListView_EnsureVisible(m_list, num, FALSE);
		ListView_SetItemState(m_list, num, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	}
}

//-----------------------------------------------------------------------------
// プレイヤーダイアログプロシージャー
//-----------------------------------------------------------------------------
INT_PTR Player::DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static Player* player = NULL;

	// ダイアログ初期化メッセージで設定
	if (msg == WM_INITDIALOG) {
		player = reinterpret_cast<Player*>(lp);
	}

	INT_PTR result = 0;

	// プレイヤーイベントハンドラを呼び出す
	if (player) {
		result = player->HandleEvent(dlg, msg, wp, lp);
	}

	// ダイアログ破棄メッセージでクリア
	if (msg == WM_DESTROY) {
		player = NULL;
	}

	return result;
}

//-----------------------------------------------------------------------------
// プレイヤー共通イベントハンドラ
//-----------------------------------------------------------------------------
INT_PTR Player::HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == m_tbcm) {
		WX_TRACE("シェルの再起動を検知しました。\n");

		m_tbcm = wx::TaskTray::RegisterTaskbarMessage();
		ApplyConfig();
		return TRUE;
	}

	switch (msg) {
	case WM_INITDIALOG:
		return OnInitDialog(dlg, wp, lp);

	case WM_CLOSE:
		DestroyWindow(dlg);
		break;

	case WM_ENDSESSION:
		//ログオフ時は、PostQuitMessage()の後の処理が動かないので、プレイリストだけでも保存する
		MediaManager::SaveDefaultList();
		DestroyWindow(dlg);
		break;

	case WM_DESTROY:
		OnDestroy(dlg, wp, lp);
		PostQuitMessage(0);
		break;

	case WM_DROPFILES:
		DropFiles(reinterpret_cast<HDROP>(wp));
		return TRUE;

	case WM_LBUTTONDOWN:
		return DefWindowProc(dlg, WM_NCLBUTTONDOWN, HTCAPTION, 0);

	case WM_COMMAND:
		return OnCommand(dlg, wp, lp);

	case WM_NOTIFY:
		return OnNotify(dlg, wp, lp);

	case WM_TIMER:
		return OnTimer(dlg, wp, lp);

	case WM_HSCROLL:
		return OnHScroll(dlg, wp, lp);

	case WM_VSCROLL:
		return OnVScroll(dlg, wp, lp);

	case WM_SIZE:
		// このオプションが有効なら、タスクバーを消す
		if (Luna::GetConfig().use_task_tray && Luna::GetConfig().hide_task_bar) {
			if (wp == SIZE_MINIMIZED) {
				ShowWindow(dlg, SW_HIDE);
			}
			else {
				ShowWindow(dlg, SW_SHOW);
			}
		}
		break;

	case MSG_PLAYEND:
	case MSG_PLAYERR:
		Luna::Stop();
		SetTimer(m_root, IDT_NEXT, 1, NULL);
		break;

	case MSG_TASKTRAY:
		switch (wp) {
		case IDC_NOTIFY:
			switch (lp) {
			case WM_LBUTTONDOWN:
				ShowWindow(dlg, SW_SHOWNORMAL);
				SetForegroundWindow(dlg);
				PostMessage(dlg, WM_NULL, 0, 0);
				break;

			case WM_RBUTTONDOWN:
				{
					POINT pt;

					GetCursorPos(&pt);

					HMENU menu = GetSubMenu(m_popup, 0);
					UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON;

					// 下記３コードはセットでやらないとダメ。Windowsの仕様。
					SetForegroundWindow(dlg);
					TrackPopupMenu(menu, flags, pt.x, pt.y, 0, dlg, NULL);
					PostMessage(dlg, WM_NULL, 0, 0);
				}
				break;
			}
			break;

		case IDC_PLAYSTOP:
			if (lp == WM_LBUTTONDOWN) {
				PostMessage(dlg, WM_COMMAND, IDM_PLAY, 0);
			}
			else if (lp == WM_RBUTTONDOWN) {
				PostMessage(dlg, WM_COMMAND, IDM_STOP, 0);
			}
			break;

		case IDC_PREVNEXT:
			if (lp == WM_LBUTTONDOWN) {
				PostMessage(dlg, WM_COMMAND, IDM_PREV, 0);
			}
			else if (lp == WM_RBUTTONDOWN) {
				PostMessage(dlg, WM_COMMAND, IDM_NEXT, 0);
			}
			break;

		default:
			break;
		}
		break;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// ダイアログ初期化メッセージ処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnInitDialog(HWND dlg, WPARAM /*wp*/, LPARAM /*lp*/)
{
	SetInfoText1(L"Loading...");

	HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(dlg, GWLP_HINSTANCE));

	// セットするアイコンを読み込む
	HANDLE icon_l = LoadImage(inst, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	HANDLE icon_s = LoadImage(inst, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	SendMessage(dlg, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(icon_l));
	SendMessage(dlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_s));

	SHFILEINFO sfi;
	HIMAGELIST himls = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	HIMAGELIST himll = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);

	const Config& conf = Luna::GetConfig();

	HWND list = GetDlgItem(dlg, IDC_VLIST);

	LVCOLUMN lc;
	ZeroMemory(&lc, sizeof(lc));

	lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
	lc.fmt = LVCFMT_LEFT;

	lc.pszText = TEXT("タイトル");
	lc.cx  = (conf.title_width < 0)? 200 : conf.title_width;
	ListView_InsertColumn(list, 2, &lc);

	lc.pszText = TEXT("アーティスト");
	lc.cx  = (conf.artist_width < 0)? 170 : conf.artist_width;
	ListView_InsertColumn(list, 3, &lc);

	lc.pszText = TEXT("アルバム");
	lc.cx  = (conf.album_width < 0)? 170 : conf.album_width;
	ListView_InsertColumn(list, 4, &lc);

	lc.pszText = TEXT("時間");
	lc.cx  = (conf.duration_width < 0)? 48 : conf.duration_width;
	lc.fmt = LVCFMT_RIGHT;
	ListView_InsertColumn(list, 5, &lc);

	ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);
	ListView_SetItemCountEx(list, 0, LVSICF_NOINVALIDATEALL);

	ListView_SetImageList(list, himll, LVSIL_NORMAL);
	ListView_SetImageList(list, himls, LVSIL_SMALL);
	ListView_SetImageList(list, himls, LVSIL_GROUPHEADER);

	// uxtheme.dll がない Windows2000でも動くように動的読み込み
	//SetWindowTheme(list, L"Explorer", NULL);
	HMODULE themeModule = LoadLibrary(TEXT("uxtheme.dll"));
	if (themeModule) {
		typedef void (WINAPI *SetWindowThemeProc)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
		SetWindowThemeProc setWindowThemeProc = (SetWindowThemeProc)GetProcAddress(themeModule, "SetWindowTheme");
		if (setWindowThemeProc) {
			(*setWindowThemeProc)(list, L"Explorer", NULL);
		}
	}

	SendDlgItemMessage(dlg, IDC_SEARCH, EM_LIMITTEXT, 64, 0);

	int volume = wx::Clamp(100 - Luna::GetConfig().out_volume, 0, 100);
	HWND hvol = GetDlgItem(dlg, IDC_VOLUME);

	// ボリュームの最小・最大と移動量・現在値設定
	SendMessage(hvol, TBM_SETRANGEMIN, TRUE,   0);
	SendMessage(hvol, TBM_SETRANGEMAX, TRUE, 100);
	SendMessage(hvol, TBM_SETLINESIZE,    0,   1);
	SendMessage(hvol, TBM_SETPAGESIZE,    0,   5);
	SendMessage(hvol, TBM_SETPOS,      TRUE, volume);
	SetSeekbar(0, false);

	m_list = list;
	m_popup = LoadMenu(inst, MAKEINTRESOURCE(IDR_TRAYMENU));

	// タスクトレイのアイコンを読み込む
	m_icon_ps = reinterpret_cast<HICON>(LoadImage(inst,
		MAKEINTRESOURCE(IDI_PLAY), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
	m_icon_pn = reinterpret_cast<HICON>(LoadImage(inst,
		MAKEINTRESOURCE(IDI_SKIP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

	// タスクバー再起動の検知用
	m_tbcm = wx::TaskTray::RegisterTaskbarMessage();
	return TRUE;
}

//-----------------------------------------------------------------------------
// ダイアログ破棄メッセージ処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnDestroy(HWND /*dlg*/, WPARAM /*wp*/, LPARAM /*lp*/)
{
	Luna::Stop();

	Config& conf = Luna::GetConfig();

	conf.title_width    = ListView_GetColumnWidth(m_list, 0);
	conf.artist_width   = ListView_GetColumnWidth(m_list, 1);
	conf.album_width    = ListView_GetColumnWidth(m_list, 2);
	conf.duration_width = ListView_GetColumnWidth(m_list, 3);

	conf.sort_key  = MediaManager::GetSortKey();
	conf.sort_type = MediaManager::GetSortType();

	SaveConfig(conf);

	m_ctrl_pn.Finalize();
	m_ctrl_ps.Finalize();
	m_tray.Finalize();

	if (m_icon_pn) {
		DestroyIcon(m_icon_pn);
		m_icon_pn = NULL;
	}

	if (m_icon_ps) {
		DestroyIcon(m_icon_ps);
		m_icon_ps = NULL;
	}

	if (m_popup) {
		DestroyMenu(m_popup);
		m_popup = NULL;
	}

	// この後、DestroyWindowが呼ばれるので、ハンドルは不要
	m_root = NULL;
	m_list = NULL;

	return TRUE;
}

//-----------------------------------------------------------------------------
// コマンドイベント処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnCommand(HWND dlg, WPARAM wp, LPARAM /*lp*/)
{
	switch (LOWORD(wp)) {
	case IDM_OPENFILE:
		OpenFile(dlg);
		break;

	case IDM_OPENDIR:
		OpenFolder(dlg);
		break;

	case IDM_LOADLIST:
		LoadPlayList(dlg);
		break;

	case IDM_SAVELIST:
		SavePlayList(dlg);
		break;

	case IDM_CLEARLIST:
		MediaManager::Clear();
		UpdatePlayList();
		break;

	case IDM_REMOVENOTEXISTS:
		MediaManager::RemoveNotExists();
		UpdatePlayList();
		break;

	case IDM_EXIT:
		if (IsWindowEnabled(dlg)) {
			PostMessage(dlg, WM_CLOSE, 0, 0);
		}
		break;

	case IDOK:
		if (GetFocus() == m_list) {
			int pos = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
			if (pos >= 0) {
				Luna::Stop();

				Luna::SetNexIndex(pos);
				Luna::Play();
			}
		}
		return TRUE;

	case IDCANCEL:
		SetFocus(m_list);
		break;

	case IDC_PLAY:
	case IDM_PLAY:
		if (IsWindowEnabled(dlg)) {
			Luna::Play();
		}
		break;

	case IDC_STOP:
	case IDM_STOP:
		if (IsWindowEnabled(dlg)) {
			KillTimer(dlg, IDT_NEXT);
			Luna::Stop();
		}
		break;

	case IDM_PREV:
		if (IsWindowEnabled(dlg)) {
			Luna::Prev();
		}
		break;

	case IDM_NEXT:
		if (IsWindowEnabled(dlg)) {
			Luna::Next();
		}
		break;

	case IDM_NORMAL:
		Luna::GetConfig().play_mode = PLAY_NORMAL;
		ApplyConfig();
		break;

	case IDM_REPEAT:
		Luna::GetConfig().play_mode = PLAY_REPEAT;
		ApplyConfig();
		break;

	case IDM_SINGLE:
		Luna::GetConfig().play_mode = PLAY_SINGLE;
		ApplyConfig();
		break;

	case IDM_RANDOM:
		Luna::GetConfig().play_mode = PLAY_RANDOM;
		ApplyConfig();
		break;

	case IDC_SEARCH:
		if (HIWORD(wp) == EN_UPDATE) {
			TCHAR buf[256];
			GetDlgItemText(dlg, IDC_SEARCH, buf, 256);

			MediaManager::SetFilter(buf);
			UpdatePlayList();
		}
		break;

	case IDM_CONFIG:
		if (PlayerConfig::ShowDialog(dlg)) {
			ApplyConfig();
		}
		break;

	case IDM_PLUGIN:
		PluginConfig::ShowDialog(dlg);
		break;

	case IDM_TOPMOST:
		Luna::GetConfig().always_top = !Luna::GetConfig().always_top;
		ApplyConfig();
		break;

	case IDM_PLAYINFO:
		MessageBox(dlg, m_play_info, TEXT("再生情報"), MB_ICONINFORMATION);
		break;

	case IDM_MANUAL:
		ShowManual(dlg);
		break;

	case IDM_ABOUT:
		ShowAbout(dlg);
		break;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// タイマーイベント処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnTimer(HWND dlg, WPARAM wp, LPARAM /*lp*/)
{
	switch (wp) {
	case IDT_DISP:
		ChangeTime(Luna::GetEngine().GetPlayTime());
		break;

	case IDT_SEEK:
		{
			KillTimer(dlg, IDT_SEEK);

			LRESULT pos = SendDlgItemMessage(dlg, IDC_SEEK, TBM_GETPOS, 0, 0);
			Luna::Seek(static_cast<UINT>(pos * 100));
			m_moving = false;
		}
		return TRUE;

	case IDT_NEXT:
		KillTimer(dlg, IDT_NEXT);
		Luna::Next();
		break;

	case IDT_HIDE:
		HideBalloon();
		break;

	case IDT_INFO:
		{
			Lyrics& lrc = Luna::GetLyrics();
			UINT time = Luna::GetEngine().GetPlayTime();

			const wchar_t* cur = lrc.GetCurLine(time);
			const wchar_t* pre = lrc.GetPreLine(time);

			if (cur && pre) {
				SetInfoText1(cur? cur : L"");
				SetInfoText2(pre? pre : L"");
			}
		}
		break;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// 横スクロールイベント処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnHScroll(HWND dlg, WPARAM wp, LPARAM lp)
{
	HWND hctrl = reinterpret_cast<HWND>(lp);
	LRESULT pos = SendMessage(hctrl, TBM_GETPOS, 0, 0);
	if (pos < 0) {
		pos = 0;
	}

	TCHAR buf[16];

	int disp_time = static_cast<int>(pos) / 10;
	wsprintf(buf, TEXT("%d:%02d"), disp_time / 60, disp_time % 60);
	SetDlgItemText(dlg, IDC_TIME, buf);

	switch (LOWORD(wp)) {
	// 直ぐに絶対位置への移動
	case SB_LINELEFT:
	case SB_LINERIGHT:
	case SB_PAGELEFT:
	case SB_PAGERIGHT:
		m_direct = true;
		break;

	// 指定時間後絶対位置への移動
	case SB_THUMBPOSITION:
		m_moving = true;
		SetTimer(dlg, IDT_SEEK, WAIT_TIME, NULL);
		return TRUE;

	case SB_ENDSCROLL:
		if (m_moving) {
			SetTimer(dlg, IDT_SEEK, WAIT_TIME, NULL);
		}
		return TRUE;

	case SB_THUMBTRACK:
		m_moving = true;
		return TRUE;
	}

	LRESULT maxpos = SendMessage(hctrl, TBM_GETRANGEMAX, 0, 0);
	if (pos <= maxpos) {
		Luna::Seek(static_cast<UINT>(pos * 100));
	}

	m_moving = false;
	m_direct = false;
	return TRUE;
}

//-----------------------------------------------------------------------------
// 縦スクロールイベント処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnVScroll(HWND dlg, WPARAM /*wp*/, LPARAM lp)
{
	HWND hctrl = reinterpret_cast<HWND>(lp);
	if (hctrl == GetDlgItem(dlg, IDC_VOLUME)) {
		LRESULT pos = SendMessage(hctrl, TBM_GETPOS, 0, 0);
		UINT volume = 100 - static_cast<UINT>(pos);
		Luna::SetVolume(volume);
		return TRUE;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// 通知メッセージ処理
//-----------------------------------------------------------------------------
INT_PTR Player::OnNotify(HWND dlg, WPARAM /*wp*/, LPARAM lp)
{
	NMHDR* nmh = reinterpret_cast<NMHDR*>(lp);
	//NMLISTVIEW* nlv = reinterpret_cast<NMLISTVIEW*>(lp);
	HWND from = nmh->hwndFrom;

	switch (nmh->code) {
	case LVN_GETDISPINFO:
		{
			NMLVDISPINFO* ndi = reinterpret_cast<NMLVDISPINFO*>(lp);
			Media* media = MediaManager::Get(ndi->item.iItem);
			if (media && ((ndi->item.mask & LVIF_TEXT) == LVIF_TEXT)) {
				const wchar_t* text[] = { media->GetTitle(), media->GetArtist(), media->GetAlbum() };
				int sub_item = ndi->item.iSubItem;
				if (sub_item == 0) {
					if (PathFileExists(media->GetPath())) {
						SHFILEINFO sfi;
						SHGetFileInfo(media->GetPath(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
						ndi->item.iImage = sfi.iIcon;
					}
					else {
						ndi->item.iImage = 0;
					}
				}

				if (sub_item >= 0 && sub_item < WX_NUMBEROF(text)) {
					lstrcpyn(ndi->item.pszText, text[sub_item], ndi->item.cchTextMax);
				}
				else if (sub_item == 3) {
					Media::FormatTime(ndi->item.pszText, media->GetDuration());
				}
			}
		}
		return TRUE;

	case LVN_GETINFOTIP:
		{
			NMLVGETINFOTIP* tip = reinterpret_cast<NMLVGETINFOTIP*>(lp);
			Media* media = MediaManager::Get(tip->iItem);
			if (media) {
				wchar_t time[32];

				Media::FormatTime(time, media->GetDuration());
				wsprintf(tip->pszText, L"タイトル：%s\nアーティスト：%s\nアルバム：%s\n時間：%s (%d秒)",
					media->GetTitle(), media->GetArtist(), media->GetAlbum(), time, media->GetDuration() / 1000);
			}
		}
		return TRUE;

	case LVN_KEYDOWN:
		{
			LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
			switch (pnkd->wVKey) {
			case VK_RETURN:
				PostMessage(dlg, WM_KEYDOWN, VK_RETURN, 0);
				return TRUE;

			case VK_DELETE:
				for (int pos = 0; pos >= 0;) {
					pos = ListView_GetNextItem(from, pos - 1, LVNI_SELECTED);
					if (pos >= 0) {
						MediaManager::Remove(pos);
						ListView_DeleteItem(pnkd->hdr.hwndFrom, pos);
					}
				}

				ListView_SetItemCountEx(from, MediaManager::GetNum(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
				UpdateListInfo();
				return TRUE;

			case VK_LEFT:
				if (GetKeyState(VK_CONTROL) & 0x80) {
					PostMessage(dlg, WM_COMMAND, IDM_PREV, 0);
				}
				else {
					PostMessage(from, WM_KEYDOWN, VK_PRIOR, 0);
				}
				return TRUE;

			case VK_RIGHT:
				if (GetKeyState(VK_CONTROL) & 0x80) {
					PostMessage(dlg, WM_COMMAND, IDM_NEXT, 0);
				}
				else {
					PostMessage(from, WM_KEYDOWN, VK_NEXT, 0);
				}
				return TRUE;

			case VK_F5:
				UpdateListInfo();
				break;

			case 'F':
				if (GetKeyState(VK_CONTROL) & 0x80) {
					SetFocus(GetDlgItem(dlg, IDC_SEARCH));
					return TRUE;
				}
				break;
			}
		}
		break;

	case NM_DBLCLK:
		PostMessage(dlg, WM_KEYDOWN, VK_RETURN, 0);
		return TRUE;

	case LVN_COLUMNCLICK:
		{
			NM_LISTVIEW* nmlv = (NM_LISTVIEW*)lp;
			switch (nmlv->iSubItem) {
			case 0:
				MediaManager::Sort(MediaManager::SORT_KEY_TITLE, MediaManager::SORT_TYPE_AUTO);
				UpdatePlayList();
				break;

			case 1:
				MediaManager::Sort(MediaManager::SORT_KEY_ARTIST, MediaManager::SORT_TYPE_AUTO);
				UpdatePlayList();
				break;

			case 2:
				MediaManager::Sort(MediaManager::SORT_KEY_ALBUM, MediaManager::SORT_TYPE_AUTO);
				UpdatePlayList();
				break;

			case 3:
				MediaManager::Sort(MediaManager::SORT_KEY_DURATION, MediaManager::SORT_TYPE_AUTO);
				UpdatePlayList();
				break;
			}
		}
		break;

	default:
		break;
	}

	return FALSE;
}
