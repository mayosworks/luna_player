//=============================================================================
// 歌詞データ読み込み
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "lib/wx_misc.h"
#include "lyrics_loader.h"

// 内部定義
namespace {

// 歌詞形式
enum Type
{
	TYPE_BASE,

	TYPE_LRC = TYPE_BASE,
	TYPE_TXT,

	TYPE_MAXNUM,
};

const wchar_t* EXT[] =
{
	L"lrc",
	L"txt",
};

} //namespace

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
LyricsLoader::LyricsLoader()
	: m_search_nest(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
LyricsLoader::~LyricsLoader()
{
}

//-----------------------------------------------------------------------------
// 検索するディレクトリ設定
//-----------------------------------------------------------------------------
void LyricsLoader::SetSearchDir(const wchar_t* search_dir)
{
	WX_NULL_ASSERT(search_dir);
	m_search_dir = search_dir;
}

//-----------------------------------------------------------------------------
// 検索する階層を設定
//-----------------------------------------------------------------------------
void LyricsLoader::SetSearchNest(int search_nest)
{
	m_search_nest = search_nest;
}

//-----------------------------------------------------------------------------
// 検索する階層を取得
//-----------------------------------------------------------------------------
int LyricsLoader::SetSearchNest() const
{
	return m_search_nest;
}

//-----------------------------------------------------------------------------
// 歌詞を検索
//-----------------------------------------------------------------------------
bool LyricsLoader::SearchLyrics(const wchar_t* file_path, const wchar_t* title)
{
	wchar_t base_dir[MAX_PATH], base_name[MAX_PATH];

	m_found_path = L"";

	// ファイル名とディレクトリに分割
	lstrcpyn(base_dir, file_path, MAX_PATH);
	PathRemoveFileSpec(base_dir);
	PathAddBackslash(base_dir);

	// 拡張子なしのファイル名を作成
	lstrcpyn(base_name, PathFindFileName(file_path), MAX_PATH);
	PathRemoveExtension(base_name);

	// 設定されたディレクトリを検索
	if (!m_search_dir.IsEmpty()) {
		// 拡張子形式→場所の順で検索。
		for (int type = TYPE_BASE; type < TYPE_MAXNUM; ++type) {
			if (FindFileInDir(m_search_dir, base_name, m_search_nest, type)) {
				return true;
			}

			if (title && FindFileInDir(m_search_dir, title, m_search_nest, type)) {
				return true;
			}
		}
	}

	// 拡張子形式→場所の順で検索。
	for (int type = TYPE_BASE; type < TYPE_MAXNUM; ++type) 	{
		// カレントディレクトリ：ファイル名検索
		if (FindFileInDir(base_dir, base_name, 0, type)) {
			return true;
		}

		// タイトルをファイル名にして、再検索
		if (title && FindFileInDir(base_dir, title, 0, type)) {
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// 検索で見つかったファイルパスを取得
//-----------------------------------------------------------------------------
const wchar_t* LyricsLoader::GetFoundPath() const
{
	return m_found_path;
}

//-----------------------------------------------------------------------------
// 検索処理
//-----------------------------------------------------------------------------
bool LyricsLoader::FindFileInDir(const wchar_t* dir, const wchar_t* fname, int nest, int type)
{
	wchar_t path[MAX_PATH];

	// ここでPathCombineを使うと、"file..."のようなファイル名の...がなくなるので注意。
	wsprintf(path, TEXT("%s\\%s.%s"), dir, fname, EXT[type]);
	if (PathFileExists(path)) {
		m_found_path = path;
		return true;
	}

	// 検索階層チェック、0ならば終了
	if (!nest) {
		return false;
	}

	PathCombine(path, dir, L"*");

	// サブディレクトリ検索

	WIN32_FIND_DATA wfd;
	HANDLE find = FindFirstFileEx(path, FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, 0);
	if (find == INVALID_HANDLE_VALUE) {
		return false;
	}

	do {
		if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (wfd.cFileName[0] != L'.')) {
			PathCombine(path, dir, wfd.cFileName);
			if (FindFileInDir(path, fname, nest - 1, type)) {
				FindClose(find);
				return true;
			}
		}
	}
	while (FindNextFile(find, &wfd));
	FindClose(find);

	return false;
}
