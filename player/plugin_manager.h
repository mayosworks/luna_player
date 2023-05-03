//=============================================================================
// プラグイン管理
//=============================================================================
#pragma once

class Plugin;

//-----------------------------------------------------------------------------
// プラグイン管理マネージャー
//-----------------------------------------------------------------------------
class PluginManager
{
public:
	static bool Initialize(HWND root);
	static void Finalize();

	static int GetNum();
	static Plugin* GetPlugin(int index);

private:
	PluginManager();
	~PluginManager();

private:
	class Impl;
	static Impl* m_impl;
};
