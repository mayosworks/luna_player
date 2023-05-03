//=============================================================================
// 全体コントロール、表のプレイヤーと裏のレンダラーをつなぐ
//=============================================================================
#pragma once

#include <windows.h>
#include "lib/wx_misc.h"
#include "lib/wx_application.h"
#include "config.h"
#include "player_window.h"
#include "engine.h"
#include "effect.h"
#include "lyrics.h"
#include "driver.h"

#define APP_NAME		TEXT("Luna")

// メッセージ用ID
const UINT MSG_TASKTRAY = WM_APP + 1;
const UINT MSG_PLAYEND  = WM_APP + 2;
const UINT MSG_PLAYERR  = WM_APP + 3;

// メインクラス
class Luna : public wx::Application
{
public:
	Luna();
	virtual ~Luna();

	bool Initialize(HINSTANCE appInst);
	void Finalize();
	bool Execute(MSG* msg);

public:
	// ユーザー操作受付
	static bool Play();
	static void Stop();
	static void Prev();
	static void Next();
	static bool Seek(UINT time);
	static void SetVolume(int volume);

	// ファイル・フォルダを判定してメディアを追加する
	static bool Add(const wchar_t* path);

	static void PlayEnd();
	static void PlayErr();

	// メッセージを処理する（ループ内での呼び出し用）
	static void ProcessMessage();

	static Config& GetConfig() { return GetInstance().m_config; }
	static Engine& GetEngine() { return GetInstance().m_engine; }
	static Lyrics& GetLyrics() { return GetInstance().m_lyrics; }

	static void SetNexIndex(int index);

private:
	static Luna*	m_instance;

	// インスタンス取得
	static Luna& GetInstance() { return *m_instance; }

	static bool ParseDir(const wchar_t* dir);
	static bool AddMedia(const wchar_t* path);

	static void NameSafeCopy(wchar_t* dest, const wchar_t* src, int length);

private:
	HWND		m_root;
	int			m_index;
	Config		m_config;
	Player		m_player;
	Engine		m_engine;
	Plugin*		m_plugin;
	Lyrics		m_lyrics;
};
