//=============================================================================
// 歌詞データ読み込み
//=============================================================================
#pragma once

#include "lib/wx_string.h"
#include "Lyrics.h"

//-----------------------------------------------------------------------------
// 歌詞データ検索などの制御クラス
//-----------------------------------------------------------------------------
class LyricsLoader
{
public:
	LyricsLoader();
	virtual ~LyricsLoader();

	// 検索パス設定
	void SetSearchDir(const wchar_t* search_dir);

	void SetSearchNest(int search_nest);
	int SetSearchNest() const;

	// 歌詞ファイル検索
	bool SearchLyrics(const wchar_t* file_path, const wchar_t* title);

	// 見つかったファイルのパスを取得
	const wchar_t* GetFoundPath() const;

private:
	bool FindFileInDir(const wchar_t* dir, const wchar_t* fname, int nest, int type);

private:
	wx::String	m_found_path;	// 見つけたパス
	wx::String	m_search_dir;	// 検索対象ディレクトリ
	int			m_search_nest;	// 検索階層：-1は無限、0はなし, def:0
};
