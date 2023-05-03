//=============================================================================
// プラグイン設定
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include "plugin.h"
#include "plugin_config.h"
#include "plugin_manager.h"
#include "resource.h"

//-----------------------------------------------------------------------------
// プラグイン設定ダイアログ表示
//-----------------------------------------------------------------------------
BOOL PluginConfig::ShowDialog(HWND parent)
{
	HINSTANCE inst = GetModuleHandle(NULL);
	return static_cast<BOOL>(DialogBox(inst, MAKEINTRESOURCE(IDD_PLUGIN), parent, DialogProc));
}

//-----------------------------------------------------------------------------
// 設定ダイアログインスタンスを返す
//-----------------------------------------------------------------------------
PluginConfig& PluginConfig::GetInstance()
{
	static PluginConfig conf;
	return conf;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
PluginConfig::PluginConfig()
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
PluginConfig::~PluginConfig()
{
}

//-----------------------------------------------------------------------------
// プラグイン設定ダイアログプロシージャー
//-----------------------------------------------------------------------------
INT_PTR PluginConfig::DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static PluginConfig& conf = GetInstance();
	return conf.HandleEvent(dlg, msg, wp, lp);
}

//-----------------------------------------------------------------------------
// イベントハンドラ
//-----------------------------------------------------------------------------
INT_PTR PluginConfig::HandleEvent(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_INITDIALOG:
		return OnInitDialog(dlg, wp, lp);

	case WM_COMMAND:
		return OnCommand(dlg, wp, lp);

	case WM_NOTIFY:
		return OnNotify(dlg, wp, lp);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// ダイアログ初期化メッセージ処理
//-----------------------------------------------------------------------------
INT_PTR PluginConfig::OnInitDialog(HWND dlg, WPARAM /*wp*/, LPARAM /*lp*/)
{
	HWND list = GetDlgItem(dlg, IDC_PLGLIST);

	LVCOLUMN lc;

	ZeroMemory(&lc, sizeof(lc));
	lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
	lc.fmt  = LVCFMT_LEFT;
	lc.cx   = 250;

	lc.pszText = L"名称";
	ListView_InsertColumn(list, 0, &lc);

	lc.pszText = L"対応形式";
	ListView_InsertColumn(list, 1, &lc);

	// グリッド線、チェックボックス表示、行選択を設定
	DWORD style = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
	ListView_SetExtendedListViewStyle(list, style);

	ResetConfig(dlg);
	return TRUE;
}

//-----------------------------------------------------------------------------
// コマンドメッセージ処理
//-----------------------------------------------------------------------------
INT_PTR PluginConfig::OnCommand(HWND dlg, WPARAM wp, LPARAM /*lp*/)
{
	switch (LOWORD(wp)) {
	case IDOK:
		ApplyConfig(dlg);
		EndDialog(dlg, TRUE);
		break;

	case IDCANCEL:
		EndDialog(dlg, FALSE);
		break;

	case IDC_PLGPROP:
		ShowProperty(dlg);
		break;

	default:
		return 0;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 通知メッセージ処理
//-----------------------------------------------------------------------------
INT_PTR PluginConfig::OnNotify(HWND dlg, WPARAM /*wp*/, LPARAM lp)
{
	LPNMHDR      nmh = reinterpret_cast<LPNMHDR>(lp);
	LPNMLISTVIEW nlv = reinterpret_cast<LPNMLISTVIEW>(lp);

	switch (nmh->code) {
	case LVN_ITEMCHANGED:
		{
			int index = ListView_GetNextItem(nlv->hdr.hwndFrom, -1, LVNI_SELECTED);
			BOOL enable = (index >= 0)? PluginManager::GetPlugin(index)->HasProperty() : FALSE;

			EnableWindow(GetDlgItem(dlg, IDC_PLGPROP), enable);
		}
		break;

	case NM_RCLICK:
		{
			int index = ListView_GetNextItem(nlv->hdr.hwndFrom, -1, LVNI_SELECTED);
			if (index >= 0) {
				BOOL checked = ListView_GetCheckState(nmh->hwndFrom, index);
				ListView_SetCheckState(nmh->hwndFrom, nlv->iItem, !checked);
			}
		}
		break;

	case NM_DBLCLK:
		ShowProperty(dlg);
		break;

	default:
		return 0;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 変更内容を反映
//-----------------------------------------------------------------------------
void PluginConfig::ApplyConfig(HWND dlg)
{
	HWND list = GetDlgItem(dlg, IDC_PLGLIST);

	int num = PluginManager::GetNum();
	for (int i= 0; i < num; ++i) {
		Plugin* plugin = PluginManager::GetPlugin(i);

		BOOL check = ListView_GetCheckState(list, i);
		plugin->SetEnable(check != FALSE);
	}
}

//-----------------------------------------------------------------------------
// 変更内容を取り消す
//-----------------------------------------------------------------------------
void PluginConfig::ResetConfig(HWND dlg)
{
	HWND list = GetDlgItem(dlg, IDC_PLGLIST);

	ListView_DeleteAllItems(list);

	LVITEM li;

	ZeroMemory(&li, sizeof(li));

	li.mask = LVIF_PARAM;

	int num = PluginManager::GetNum();
	for (int i= 0; i < num; ++i) {
		li.iItem  = i;
		li.lParam = i;

		ListView_InsertItem(list, &li);

		Plugin* plugin = PluginManager::GetPlugin(i);
		SetPluginInfo(list, i, plugin, plugin->IsEnable());
	}
}

//-----------------------------------------------------------------------------
// プラグイン自身のプロパティを表示
//-----------------------------------------------------------------------------
void PluginConfig::ShowProperty(HWND dlg)
{
	HWND list = GetDlgItem(dlg, IDC_PLGLIST);

	// 現在の選択を取得する
	int index = ListView_GetNextItem(list, -1, LVNI_SELECTED);
	if (index >= 0) {
		LVITEM li;

		ZeroMemory(&li, sizeof(li));

		li.mask  = LVIF_PARAM;
		li.iItem = index;

		ListView_GetItem(list, &li);

		PluginManager::GetPlugin(static_cast<int>(li.lParam))->ShowProperty(dlg);
	}
}

//-----------------------------------------------------------------------------
// プラグイン情報をリストに設定する
//-----------------------------------------------------------------------------
void PluginConfig::SetPluginInfo(HWND list, int index, Plugin* plugin, BOOL checked)
{
	ListView_SetItemText(list, index, 0, const_cast<wchar_t*>(plugin->GetName()));
	ListView_SetItemText(list, index, 1, const_cast<wchar_t*>(plugin->GetType()));
	ListView_SetCheckState(list, index, checked);
}
