//=============================================================================
// メディア管理
//=============================================================================
#pragma once

#include <vector>
#include "lib/wx_string.h"

class Media;

//-----------------------------------------------------------------------------
// メディア管理マネージャー
//-----------------------------------------------------------------------------
class MediaManager
{
public:
	enum SortKey
	{
		SORT_KEY_NONE,		// ソートしない
		SORT_KEY_TITLE,		// タイトル
		SORT_KEY_ARTIST,	// アーティスト
		SORT_KEY_ALBUM,		// アルバム
		SORT_KEY_DURATION,	// 時間

		SORT_KEY_NUM,
	};

	enum SortType
	{
		SORT_TYPE_AUTO,
		SORT_TYPE_NONE,
		SORT_TYPE_LESS,
		SORT_TYPE_GREATER,

		SORT_TYPE_NUM,
	};

	typedef std::vector<wx::String>	StringVector;

public:
	static bool Initialize();
	static void Finalize();

	static void Clear();
	static void RemoveNotExists();

	static int GetNum();
	static Media* Get(int pos);

	static void GetArtistList(StringVector& vec);
	static int GetArtistId(const wchar_t* artist);

	static void GetAlbumList(StringVector& vec);
	static int GetAlbumId(const wchar_t* album);

	static void Insert(Media& media, int pos = -1);
	static void Update(Media& media, int pos);
	static void Remove(int pos);

	// よみがなフィルタを有効にする
	static void SetEnableRuby(bool enable);

	// よみがな更新
	static void UpdateRuby(Media& media);

	// ソート
	static void Sort(SortKey key, SortType type);
	static SortKey GetSortKey();
	static SortType GetSortType();

	// フィルタ
	static void SetFilter(const wchar_t* filter);
	static bool IsFiltered();

	// フィルタをかける前の値
	static int GetRawNum();

	// デフォルトプレイリストの読み込みと保存
	static bool LoadDefaultList();
	static bool SaveDefaultList();

	// プレイリストの読み込みと保存
	static bool LoadList(const wchar_t* path);
	static bool SaveList(const wchar_t* path);


private:
	MediaManager();
	~MediaManager();

private:
	class Impl;
	static Impl* m_impl;
};
