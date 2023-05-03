//=============================================================================
// プラグイン管理
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include <map>
#include <vector>
#include "lib/wx_misc.h"
#include "lib/wx_string.h"
#include "lib/wx_text_rw.h"
#include "lib/wx_search_file.h"
#include "plugin.h"
#include "plugin_manager.h"
#include "builtin_plugin.h"

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
namespace {
const wchar_t* PLGDAT_EXT = L".dat";
const wchar_t* PLUGIN_EXT = L"*.lp";
const wchar_t* BUILTIN_LP = L"*builtin*";

} //namespace

// 内部クラス
PluginManager::Impl* PluginManager::m_impl = NULL;

//-----------------------------------------------------------------------------
// 内部処理クラス
//-----------------------------------------------------------------------------
class PluginManager::Impl
{
public:
	Impl(HWND root);
	~Impl();

	void LoadList();
	void SaveList();

	void LoadBuiltin();
	bool LoadPlugin(const wchar_t* path, bool enable);

	int GetNum() const;
	Plugin* GetPlugin(int index);

private:
	typedef std::vector<Plugin*> PluginAry;
	typedef PluginAry::iterator  PluginAryIter;

	HWND		m_root;
	PluginAry	m_plugin;
};

//-----------------------------------------------------------------------------
// 内部コンストラクタ
//-----------------------------------------------------------------------------
PluginManager::Impl::Impl(HWND root)
	: m_root(root)
{
	m_plugin.reserve(64);
}

//-----------------------------------------------------------------------------
// 内部デストラクタ
//-----------------------------------------------------------------------------
PluginManager::Impl::~Impl()
{
}

//-----------------------------------------------------------------------------
// プラグインリスと読み込み
//-----------------------------------------------------------------------------
void PluginManager::Impl::LoadList()
{
	wchar_t path[MAX_PATH];

	GetModuleFileName(NULL, path, MAX_PATH);
	PathRenameExtension(path, PLGDAT_EXT);

	typedef std::map<wx::String, bool> StringMap;
	typedef StringMap::iterator StringMapIter;

	wx::TextReader reader;
	StringMap smap;

	if (reader.Open(path)) {
		while (reader.HasMoreData()) {
			const wchar_t* line = reader.ReadLine(true);

			if (line[0] == L'#' && lstrlen(line) > 5) {
				wx::String lp_path = wx::String(&line[3]).ToLower();
				bool enable = (line[1] == L'Y');

				// ビルトインプラグイン
				if (lp_path == BUILTIN_LP) {
					if (!m_plugin.empty()) {
						m_plugin[0]->SetEnable(enable);
					}
				}
				// ２重読み込み防止
				else if (smap.find(lp_path) == smap.end()) {
					if (LoadPlugin(lp_path, enable)) {
						smap.insert(StringMap::value_type(lp_path.Get(), enable));
					}
				}
			}
		}

		reader.Close();
	}

	PathRemoveFileSpec(path);

	wx::SearchFile sf;
	sf.Execute(path, PLUGIN_EXT);

	// 検索で見つけたプラグインのうち、読み込んでないものだけを読み込む
	for (int i = 0; i < sf.GetFoundNum(); ++i) {
		wx::String lp_path = wx::String(sf.GetFoundPath(i)).ToLower();
		if (smap.find(lp_path) == smap.end()) {
			LoadPlugin(lp_path, true);
		}
	}
}

//-----------------------------------------------------------------------------
// プラグインリスト保存
//-----------------------------------------------------------------------------
void PluginManager::Impl::SaveList()
{
	wchar_t path[MAX_PATH];

	// プラグインリストファイルへのパスを設定
	GetModuleFileName(NULL, path, MAX_PATH);
	PathRenameExtension(path, PLGDAT_EXT);

	wx::TextWriter writer;

	writer.Open(path);
	writer.WriteLine(L";================================================");
	writer.WriteLine(L";Lunaプラグインリストファイル");
	writer.WriteLine(L"; ※内容を編集しないでください！");
	writer.WriteLine(L";================================================");

	for (PluginAryIter it = m_plugin.begin(); it != m_plugin.end(); ++it) {
		const Plugin* plugin = *it;
		writer.Write(plugin->IsEnable()? L"#Y," : L"#N,");

		if (plugin->IsBuiltIn()) {
			lstrcpyn(path, BUILTIN_LP, MAX_PATH);
		}
		else {
			GetModuleFileName(plugin->GetModule(), path, MAX_PATH);
		}

		writer.WriteLine(path);
	}

	writer.Close();

	for (PluginAryIter it = m_plugin.begin(); it != m_plugin.end(); ++it) {
		delete *it;
	}

	m_plugin.clear();
}

//-----------------------------------------------------------------------------
// 内部プラグイン生成
//-----------------------------------------------------------------------------
void PluginManager::Impl::LoadBuiltin()
{
	Plugin* plugin = new Plugin();
	if (!plugin) {
		return;
	}

	HINSTANCE instance = GetModuleHandle(NULL);
	if (!plugin->Load(GetBuiltinPlugin(instance))) {
		delete plugin;
		return;
	}

	plugin->SetEnable(true);
	m_plugin.push_back(plugin);
}

//-----------------------------------------------------------------------------
// プラグイン読み込み
//-----------------------------------------------------------------------------
bool PluginManager::Impl::LoadPlugin(const wchar_t* path, bool enable)
{
	Plugin* plugin = new Plugin();
	if (!plugin) {
		return false;
	}

	if (!plugin->Load(path)) {
		delete plugin;
		return false;
	}

	plugin->SetEnable(enable);
	m_plugin.push_back(plugin);
	return true;
}

//-----------------------------------------------------------------------------
// プラグイン数取得
//-----------------------------------------------------------------------------
int PluginManager::Impl::GetNum() const
{
	return static_cast<int>(m_plugin.size());
}

//-----------------------------------------------------------------------------
// プラグイン取得
//-----------------------------------------------------------------------------
Plugin* PluginManager::Impl::GetPlugin(int index)
{
	return m_plugin[index];
}

//-----------------------------------------------------------------------------
// マネージャー初期化
//-----------------------------------------------------------------------------
bool PluginManager::Initialize(HWND root)
{
	m_impl = new Impl(root);
	if (!m_impl) {
		return false;
	}

	m_impl->LoadBuiltin();
	m_impl->LoadList();
	return true;
}

//-----------------------------------------------------------------------------
// マネージャー終了
//-----------------------------------------------------------------------------
void PluginManager::Finalize()
{
	if (m_impl) {
		m_impl->SaveList();
		WX_SAFE_DELETE(m_impl);
	}
}

//-----------------------------------------------------------------------------
// プラグイン数取得
//-----------------------------------------------------------------------------
int PluginManager::GetNum()
{
	return m_impl->GetNum();
}

//-----------------------------------------------------------------------------
// プラグイン取得
//-----------------------------------------------------------------------------
Plugin* PluginManager::GetPlugin(int index)
{
	return m_impl->GetPlugin(index);
}
