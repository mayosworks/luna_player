//=============================================================================
// プラグインオブジェクト
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "lib/wx_misc.h"
#include "plugin.h"

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Plugin::Plugin()
	: m_inst(NULL)
	, m_plugin(NULL)
	, m_handle(NULL)
	, m_enable(true)
	, m_builtin(false)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Plugin::~Plugin()
{
	Free();
}

//-----------------------------------------------------------------------------
// プラグイン読み込み
//-----------------------------------------------------------------------------
bool Plugin::Load(const wchar_t* path)
{
	if (m_inst) {
		return false;
	}

	// クリティカルエラーなどのメッセージを非表示化
	UINT err_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	m_inst = LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

	// エラーモードを戻す
	SetErrorMode(err_mode);

	if (!m_inst) {
		WX_TRACE("モジュール読み込み失敗：%s\n", path);
		return false;
	}

	const char* EXPAPI_NAME = "GetLunaPlugin";
	typedef LunaPlugin* (LPAPI* pfGetLunaPlugin)(HINSTANCE instance);

	pfGetLunaPlugin GetLunaPlugin = (pfGetLunaPlugin)GetProcAddress(m_inst, EXPAPI_NAME);
	if (!GetLunaPlugin) {
		WX_TRACE("プラグインではありません：%s\n", path);
		Free();
		return false;
	}

	m_plugin = GetLunaPlugin(m_inst);
	if (!m_plugin) {
		WX_TRACE("プラグインを取得できません：%s\n", path);
		Free();
		return false;
	}

	// 必須の関数ポインタや拡張子等がない場合は、NGとする
	if (!m_plugin->plugin_name || !m_plugin->support_type || !m_plugin->Parse || !m_plugin->Open || !m_plugin->Close) {
		WX_TRACE("必須関数が実装されていません：%s\n", path);
		Free();
		return false;
	}

	if (m_plugin->plugin_kind != KIND_PLUGIN) {
		WX_TRACE("プラグインの種類が違います：%s\n", path);
		Free();
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// プラグイン読み込み
//-----------------------------------------------------------------------------
bool Plugin::Load(LunaPlugin* plugin)
{
	if (m_inst) {
		return false;
	}

	m_builtin = true;
	m_plugin = plugin;
	if (!m_plugin) {
		WX_TRACE("プラグインを取得できません：(null)\n");
		Free();
		return false;
	}

	// 必須の関数ポインタや拡張子等がない場合は、NGとする
	if (!m_plugin->plugin_name || !m_plugin->support_type || !m_plugin->Parse || !m_plugin->Open || !m_plugin->Close) {
		WX_TRACE("必須関数が実装されていません：(null)\n");
		Free();
		return false;
	}

	if (m_plugin->plugin_kind != KIND_PLUGIN) {
		WX_TRACE("プラグインの種類が違います：(null)\n");
		Free();
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// プラグイン解放
//-----------------------------------------------------------------------------
void Plugin::Free()
{
	if (m_plugin) {
		if (m_plugin->Release) {
			m_plugin->Release();
		}

		m_plugin = NULL;
	}

	if (m_inst) {
		FreeLibrary(m_inst);
		m_inst = NULL;
	}
}

//-----------------------------------------------------------------------------
// 対象パスのデータをサポートしているか調べる
//-----------------------------------------------------------------------------
bool Plugin::IsSupported(const wchar_t* path) const
{
	// 無効の場合は、チェックせずに未サポートとする
	if (!m_plugin || !m_enable) {
		return false;
	}

	// 拡張子のパターンマッチング
	if (!PathMatchSpec(path, m_plugin->support_type)) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// プロパティを持っているか
//-----------------------------------------------------------------------------
bool Plugin::HasProperty() const
{
	return (m_plugin->Property != NULL);
}

//-----------------------------------------------------------------------------
// プロパティを表示する
//-----------------------------------------------------------------------------
void Plugin::ShowProperty(HWND parent)
{
	if (m_plugin && m_plugin->Property) {
		m_plugin->Property(m_inst, parent);
	}
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
bool Plugin::Parse(const wchar_t* path, Metadata& meta)
{
	int result = m_plugin->Parse(path, &meta);
	return (result != FALSE);
}

//-----------------------------------------------------------------------------
// 開く
//-----------------------------------------------------------------------------
bool Plugin::Open(const wchar_t* path, Output& out)
{
	m_handle = m_plugin->Open(path, &out);
	return (m_handle != NULL);
}

//-----------------------------------------------------------------------------
// 閉じる
//-----------------------------------------------------------------------------
void Plugin::Close()
{
	if (m_handle) {
		m_plugin->Close(m_handle);
		m_handle = NULL;
	}
}

//-----------------------------------------------------------------------------
// レンダリング
//-----------------------------------------------------------------------------
UINT Plugin::Render(BYTE* buffer, UINT size)
{
	if (m_handle) {
		return m_plugin->Render(m_handle, buffer, size);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
UINT Plugin::Seek(UINT time)
{
	if (m_handle && m_plugin->Seek) {
		return m_plugin->Seek(m_handle, time);
	}

	return 0;
}
