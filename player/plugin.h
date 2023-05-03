//=============================================================================
// プラグインオブジェクト
//=============================================================================
#pragma once

#include <windows.h>
#include "luna_pi.h"

//-----------------------------------------------------------------------------
// プラグインオブジェクト
//-----------------------------------------------------------------------------
class Plugin
{
public:
	Plugin();
	~Plugin();

	// プラグインロード・解放
	bool Load(const wchar_t* path);
	bool Load(LunaPlugin* plugin);
	void Free();

	// DLLのモジュールハンドルを取得
	HINSTANCE GetModule() const { return m_inst; }

	// 有効・無効
	void SetEnable(bool enable){ m_enable = enable; }
	bool IsEnable() const { return m_enable; }

	// ビルトイン判定
	bool IsBuiltIn() const { return m_builtin; }

	// 対象チェック
	bool IsSupported(const wchar_t* path) const;

	// プロパティを表示
	bool HasProperty() const;
	void ShowProperty(HWND parent);

	// プラグイン情報取得
	const wchar_t* GetName() const { return m_plugin->plugin_name; }
	const wchar_t* GetType() const { return m_plugin->support_type; }

	// プラグイン処理実行
	bool Parse(const wchar_t* path, Metadata& meta);
	bool Open(const wchar_t* path, Output& out);
	void Close();
	UINT Render(BYTE* buffer, UINT size);
	UINT Seek(UINT time);

private:
	// コピーコンストラクタ、代入演算子を無効化
	Plugin(const Plugin&);
	Plugin& operator = (const Plugin&);

private:
	HINSTANCE	m_inst;
	LunaPlugin*	m_plugin;
	Handle		m_handle;
	bool		m_enable;
	bool		m_builtin;
};
