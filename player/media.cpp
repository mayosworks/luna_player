//=============================================================================
// メディアオブジェクト
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "media.h"

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Media::Media()
	: m_duration(0)
	, m_seekable(false)
{
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Media::Media(const wchar_t* path, const Metadata& meta)
	: m_path(path)
	, m_duration(0)
	, m_seekable(false)
{
	SetDuration(meta.duration);
	SetSeekable(meta.seekable != FALSE);

	SetTitle (meta.title);
	SetArtist(meta.artist);
	SetAlbum (meta.album);
	SetExtra (meta.extra);
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Media::~Media()
{
}

//-----------------------------------------------------------------------------
// タイトルセット
//-----------------------------------------------------------------------------
void Media::SetTitle(const wchar_t* title)
{
	if (lstrlen(title) > 0) {
		m_title = title;
	}
	else {
		wchar_t buf[MAX_PATH];

		// タイトルがない場合は、ファイル名から補正する
		lstrcpy(buf, PathFindFileName(m_path));
		PathRemoveExtension(buf);

		m_title = buf;
	}

	// iTMSで買った曲とかだと、半角になってて見づらいので、全角補正
	m_title = m_title.Replace(L'~', L'～');
}

//-----------------------------------------------------------------------------
// アーティストセット
//-----------------------------------------------------------------------------
void Media::SetArtist(const wchar_t* artist)
{
	m_artist = artist;
	// iTMSで買った曲とかだと、半角になってて見づらいので、全角補正
	m_artist = m_artist.Replace(L'~', L'～');
}

//-----------------------------------------------------------------------------
// アルバムセット
//-----------------------------------------------------------------------------
void Media::SetAlbum(const wchar_t* album)
{
	m_album = album;
	// iTMSで買った曲とかだと、半角になってて見づらいので、全角補正
	m_album = m_album.Replace(L'~', L'～');
}

//-----------------------------------------------------------------------------
// その他情報セット
//-----------------------------------------------------------------------------
void Media::SetExtra(const wchar_t* extra)
{
	m_extra  = extra;
}

//-----------------------------------------------------------------------------
// 時間を文字列化
//-----------------------------------------------------------------------------
void Media::FormatTime(wchar_t* buf, UINT duration)
{
	if (duration > 0) {
		UINT sec = duration / 1000;

		wsprintf(buf, L"%d:%02d", sec / 60, sec % 60);
	}
	else {
		lstrcpy(buf, L"--:--");
	}
}

//-----------------------------------------------------------------------------
// メディア間でコピーする
//-----------------------------------------------------------------------------
void Media::Copy(Media& to, const Media& from)
{
	to.m_path		= from.m_path;
	to.m_title		= from.m_title;
	to.m_artist		= from.m_artist;
	to.m_album		= from.m_album;
	to.m_extra		= from.m_extra;
	to.m_title_rb	= from.m_title_rb;
	to.m_artist_rb	= from.m_artist_rb;
	to.m_album_rb	= from.m_album_rb;
	to.m_duration	= from.m_duration;
	to.m_seekable	= from.m_seekable;
}
