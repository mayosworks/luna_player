//=============================================================================
// 設定関係
//=============================================================================
#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include "lib/wx_string.h"

//-----------------------------------------------------------------------------
// 再生モード
//-----------------------------------------------------------------------------
enum PlayMode
{
	PLAY_NORMAL,	// 通常再生
	PLAY_REPEAT,	// 全曲リピート
	PLAY_SINGLE,	// 単曲リピート
	PLAY_RANDOM,	// ランダム
};

//-----------------------------------------------------------------------------
// エフェクト
//-----------------------------------------------------------------------------
enum FxMode
{
	FX_NONE,
	FX_WAVES_REVERB	= (1 << 0),
	FX_COMPRESSOR	= (1 << 1),
	FX_VOCAL_CANCEL	= (1 << 2),
	FX_VOCAL_BOOST	= (1 << 3),

	FX_SETUP_ALL = 0x7FFFFFFF,
};

//-----------------------------------------------------------------------------
// 設定データ構造体
//-----------------------------------------------------------------------------
struct Config
{
	// [Display]
	bool		always_top;		///<常に手前に表示
	bool		use_task_tray;	///<タスクトレイ使用
	bool		use_balloon;	///<ふきだし使用
	bool		use_tray_ctrl;	///<タスクトレイ制御を使う
	bool		hide_task_bar;	///<最小化時、タスクバーを消す
	int			title_width;	///<タイトル幅
	int			artist_width;	///<アーティスト幅
	int			album_width;	///<アルバム幅
	int			duration_width;	///<時間幅

	// [Output]
	wx::String	device_name;	///<サウンドデバイス名
	int			out_volume;		///<出力音量
	bool		use_mmcss;		///<M MCSSを使う
	bool		allow_ks;		///<DirectKS出力許可
	bool		fx_reverb;		///<リバーブエフェクト
	bool		fx_compress;	///<コンプレッションエフェクト
	bool		vc_cancel;		///<ボーカル消し
	bool		vc_boost;		///<ボーカル強調

	// [Lyrics]
	bool		show_lyrics;	///<歌詞表示する
	bool		scan_sub_dir;	///<サブディレクトリ検索する
	wx::String	lyrcis_dir;		///<歌詞ファイル検索ディレクトリ

	// [Control]
	int			play_mode;		///<再生モード
	int			sort_key;		///<ソートキー
	int			sort_type;		///<ソートタイプ
	bool		use_ruby;		///<よみがなを使う
};

//-----------------------------------------------------------------------------
// 設定読み込み、保存
//-----------------------------------------------------------------------------
void LoadConfig(Config& conf);
void SaveConfig(Config& conf);
