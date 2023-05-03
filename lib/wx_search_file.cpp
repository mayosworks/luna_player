//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2008 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "wx_search_file.h"

namespace wx {

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
SearchFile::SearchFile(IFileFoundCallback* cb)
	: m_path(NULL)
	, m_num(0)
	, m_depth(0)
	, m_cb(cb)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
SearchFile::~SearchFile()
{
	Clear();
}

//-----------------------------------------------------------------------------
// コールバック設定
//-----------------------------------------------------------------------------
void SearchFile::SetCallback(IFileFoundCallback* cb)
{
	m_cb = cb;
}

//-----------------------------------------------------------------------------
// ファイル検索実行
//-----------------------------------------------------------------------------
bool SearchFile::Execute(PCTSTR dir, PCTSTR cond, int depth)
{
	Clear();

	IFileFoundCallback* cb = m_cb;
	m_depth = depth;

	// このクラス自身をコールバックとして使用する場合は、内部バッファを使う
	if (!cb) {
		size_t size = MAX_FILE_NUM * MAX_PATH * sizeof(TCHAR);
		void* buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
		if (!buf) {
			return false;
		}

		m_path = static_cast<TCHAR*>(buf);
		cb = this;
	}

	Find(dir, cond, 1, cb);
	return (m_num > 0);
}

//-----------------------------------------------------------------------------
// 見つけたファイル数を取得する
//-----------------------------------------------------------------------------
int SearchFile::GetFoundNum() const
{
	return m_num;
}

//-----------------------------------------------------------------------------
// 見つけたファイルのパスを取得する
//-----------------------------------------------------------------------------
PCTSTR SearchFile::GetFoundPath(int index) const
{
	return m_path? &m_path[index * MAX_PATH] : NULL;
}

//-----------------------------------------------------------------------------
// 検索したパスをクリアする
//-----------------------------------------------------------------------------
void SearchFile::Clear()
{
	if (m_path) {
		HeapFree(GetProcessHeap(), 0, m_path);
		m_path = NULL;
	}

	m_num = 0;
}

//-----------------------------------------------------------------------------
// ファイル検索を行う
//-----------------------------------------------------------------------------
bool SearchFile::Find(PCTSTR dir, PCTSTR cond, int depth, IFileFoundCallback* cb)
{
	// 先にサブディレクトリ検索
	if (!FindDir(dir, cond, depth, cb)) {
		return false;
	}

	TCHAR path[MAX_PATH], ext[MAX_PATH];

	PathCombine(path, dir, cond);

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(path, &fd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return true;
	}

	do {
		// ディレクトリでなければ調べる
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			PathCombine(path, dir, fd.cFileName);
			lstrcpy(ext, PathFindExtension(fd.cFileName));

			// ユーザーがキャンセルしたら、falseが返ってくる
			if (!cb->FoundFile(path, ext, m_num)) {
				FindClose(hFind);
				return false;
			}

			++m_num;
		}
	}
	while (FindNextFile(hFind, &fd));

	FindClose(hFind);
	return true;
}

//-----------------------------------------------------------------------------
// ディレクトリ検索を行う
//-----------------------------------------------------------------------------
bool SearchFile::FindDir(PCTSTR dir, PCTSTR cond, int depth, IFileFoundCallback* cb)
{
	TCHAR path[MAX_PATH];

	PathCombine(path, dir, L"*");

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(path, &fd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return true;
	}

	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (fd.cFileName[0] != TEXT('.')) {
				if (m_depth == 0 || depth < m_depth) {
					PathCombine(path, dir, fd.cFileName);

					// ディレクトリが見つかったらコールバック
					if (cb->FoundDir(path, depth)) {
						// ユーザーがキャンセルしたら、falseが返ってくる
						if (!Find(path, cond, depth + 1, cb)) {
							FindClose(hFind);
							return false;
						}
					}
				}
			}
		}
	}
	while (FindNextFile(hFind, &fd));

	FindClose(hFind);
	return true;
}

//-----------------------------------------------------------------------------
// ディレクトリが見つかった時のコールバック
//-----------------------------------------------------------------------------
bool SearchFile::FoundDir(PCTSTR /*path*/, int /*depth*/)
{
	return true;
}

//-----------------------------------------------------------------------------
// ファイルが見つかった時のコールバック
//-----------------------------------------------------------------------------
bool SearchFile::FoundFile(PCTSTR path, PCTSTR /*ext*/, int index)
{
	if (index < MAX_FILE_NUM) {
		lstrcpyn(&m_path[index * MAX_PATH], path, MAX_PATH);
		return true;
	}

	return false;
}

} //namespace wx
