//=============================================================================
// プレイヤー設定
//=============================================================================

#include <windows.h>
#include <shlobj.h>
#include "luna.h"
#include "config.h"
#include "player_config.h"
#include "driver_manager.h"
#include "engine.h"
#include "resource.h"

//-----------------------------------------------------------------------------
// プレイヤー設定ダイアログを表示する
//-----------------------------------------------------------------------------
BOOL PlayerConfig::ShowDialog(HWND parent)
{
	HINSTANCE inst = GetModuleHandle(NULL);
	return static_cast<BOOL>(DialogBox(inst, MAKEINTRESOURCE(IDD_CONFIG), parent, DialogProc));
}

//-----------------------------------------------------------------------------
// 設定ダイアログインスタンスを返す
//-----------------------------------------------------------------------------
PlayerConfig& PlayerConfig::GetInstance()
{
	static PlayerConfig conf;
	return conf;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
PlayerConfig::PlayerConfig()
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
PlayerConfig::~PlayerConfig()
{
}

//-----------------------------------------------------------------------------
// プレイヤー設定ダイアログプロシージャー
//-----------------------------------------------------------------------------
INT_PTR PlayerConfig::DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static PlayerConfig& conf = GetInstance();
	return conf.HandleEvent(dlg, msg, wp, lp);
}

//-----------------------------------------------------------------------------
// イベントハンドラ
//-----------------------------------------------------------------------------
INT_PTR PlayerConfig::HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
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
INT_PTR PlayerConfig::OnInitDialog(HWND dlg, WPARAM /*wp*/, LPARAM /*lp*/)
{
	SendDlgItemMessage(dlg, IDC_LRCDIR, EM_LIMITTEXT, MAX_PATH, 0);

	ResetConfig(dlg);
	return TRUE;
}

//-----------------------------------------------------------------------------
// コマンドメッセージ処理
//-----------------------------------------------------------------------------
INT_PTR PlayerConfig::OnCommand(HWND dlg, WPARAM wp, LPARAM /*lp*/)
{
	switch (LOWORD(wp)) {
	case IDOK:
		ApplyConfig(dlg);
		EndDialog(dlg, TRUE);
		break;

	case IDCANCEL:
		EndDialog(dlg, FALSE);
		break;

	case IDC_BROWSE:
		{
			BROWSEINFO bi;
			IMalloc* malloc = NULL;
			ITEMIDLIST* iidl = NULL;

			if (FAILED(SHGetMalloc(&malloc))) {
				break;
			}

			if (FAILED(SHGetSpecialFolderLocation(dlg, CSIDL_DRIVES, &iidl))) {
				malloc->Release();
				break;
			}

			ZeroMemory(&bi, sizeof(bi));

			bi.hwndOwner = dlg;
			bi.pidlRoot  = iidl;
			bi.ulFlags	 = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
			bi.lpszTitle = TEXT("歌詞フォルダを指定してください");

			LPITEMIDLIST pil = SHBrowseForFolder(&bi);
			if (pil) {
				TCHAR path[MAX_PATH];

				SHGetPathFromIDList(pil, path);
				malloc->Free(pil);
				SetDlgItemText(dlg, IDC_LRCDIR, path);
			}

			malloc->Free(iidl);
			malloc->Release();
		}
		break;

	default:
		return 0;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 変更内容を反映
//-----------------------------------------------------------------------------
void PlayerConfig::ApplyConfig(HWND dlg)
{
	Config& conf = Luna::GetConfig();

	conf.always_top    = (IsDlgButtonChecked(dlg, IDC_TOPMOST) == BST_CHECKED);
	conf.use_task_tray = (IsDlgButtonChecked(dlg, IDC_USETRAY) == BST_CHECKED);
	conf.use_balloon   = (IsDlgButtonChecked(dlg, IDC_BALLOON) == BST_CHECKED);
	conf.use_tray_ctrl = (IsDlgButtonChecked(dlg, IDC_TRAYCTRL) == BST_CHECKED);
	conf.hide_task_bar = (IsDlgButtonChecked(dlg, IDC_HIDETBAR) == BST_CHECKED);
	conf.fx_reverb     = (IsDlgButtonChecked(dlg, IDC_REVERB) == BST_CHECKED);
	conf.fx_compress   = (IsDlgButtonChecked(dlg, IDC_COMPRESS) == BST_CHECKED);
	conf.vc_cancel     = (IsDlgButtonChecked(dlg, IDC_VCCANCEL) == BST_CHECKED);
	conf.vc_boost      = (IsDlgButtonChecked(dlg, IDC_VCBOOST) == BST_CHECKED);
	conf.show_lyrics   = (IsDlgButtonChecked(dlg, IDC_SHOWLRC) == BST_CHECKED);
	conf.scan_sub_dir  = (IsDlgButtonChecked(dlg, IDC_SCANSUB) == BST_CHECKED);

	TCHAR path[MAX_PATH];

	GetDlgItemText(dlg, IDC_LRCDIR, path, MAX_PATH);
	conf.lyrcis_dir = path;

	int index = static_cast<int>(SendDlgItemMessage(dlg, IDC_DEVLIST, CB_GETCURSEL, 0, 0));
	if (index >= 0) {
		DriverManager::SelectDevice(index);
		conf.device_name = DriverManager::GetDeviceName();
	}

	SaveConfig(conf);

	UINT fx_mode = FX_NONE;
	fx_mode |= conf.fx_reverb?   FX_WAVES_REVERB : FX_NONE;
	fx_mode |= conf.fx_compress? FX_COMPRESSOR   : FX_NONE;
	fx_mode |= conf.vc_cancel?   FX_VOCAL_CANCEL : FX_NONE;
	fx_mode |= conf.vc_boost?    FX_VOCAL_BOOST  : FX_NONE;

	Luna::GetEngine().SetEffect(fx_mode);
}

//-----------------------------------------------------------------------------
// 変更内容を取り消す
//-----------------------------------------------------------------------------
void PlayerConfig::ResetConfig(HWND dlg)
{
	Config& conf = Luna::GetConfig();

	CheckDlgButton(dlg, IDC_TOPMOST,  conf.always_top );
	CheckDlgButton(dlg, IDC_USETRAY,  conf.use_task_tray);
	CheckDlgButton(dlg, IDC_BALLOON,  conf.use_balloon);
	CheckDlgButton(dlg, IDC_TRAYCTRL, conf.use_tray_ctrl);
	CheckDlgButton(dlg, IDC_HIDETBAR, conf.hide_task_bar);
	CheckDlgButton(dlg, IDC_REVERB,   conf.fx_reverb);
	CheckDlgButton(dlg, IDC_COMPRESS, conf.fx_compress);
	CheckDlgButton(dlg, IDC_VCCANCEL, conf.vc_cancel);
	CheckDlgButton(dlg, IDC_VCBOOST,  conf.vc_boost);
	CheckDlgButton(dlg, IDC_SHOWLRC,  conf.show_lyrics);
	CheckDlgButton(dlg, IDC_SCANSUB,  conf.scan_sub_dir);
	SetDlgItemText(dlg, IDC_LRCDIR,   conf.lyrcis_dir );
 
	HWND cb = GetDlgItem(dlg, IDC_DEVLIST);
	SendMessage(cb, CB_RESETCONTENT, 0, 0);

	DriverManager::EnumDevices();

	int device_num = DriverManager::GetDeviceNum();
	int current = -1;

	for (int i = 0; i < device_num; ++i) {
		const wchar_t* device_name = DriverManager::GetDeviceName(i);
		if (conf.device_name == device_name) {
			DriverManager::SelectDevice(i);
			current = i;
		}

		SendMessage(cb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(device_name));
	}

	if (current >= 0) {
		SendMessage(cb, CB_SETCURSEL, current, 0);
	}
}
