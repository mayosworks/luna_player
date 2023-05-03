//=============================================================================
// メディアオブジェクト
//=============================================================================
#pragma once

#include "lib/wx_string.h"
#include "luna_pi.h"

//-----------------------------------------------------------------------------
// メディア情報保持
//-----------------------------------------------------------------------------
class Media
{
public:
	Media();
	Media(const wchar_t* path, const Metadata& meta);
	~Media();

	const wchar_t* GetPath() const { return m_path; }
	void SetPath(const wchar_t* path) { m_path = path; }

	UINT GetDuration() const { return m_duration;   }
	bool IsSeekable() const { return m_seekable; }

	void SetDuration(UINT duration) { m_duration = duration; }
	void SetSeekable(bool seekable) { m_seekable = seekable; }

	const wchar_t* GetTitle () const { return m_title;  }
	const wchar_t* GetArtist() const { return m_artist; }
	const wchar_t* GetAlbum () const { return m_album;  }
	const wchar_t* GetExtra () const { return m_extra;  }

	// タイトル等設定
	void SetTitle (const wchar_t* title);
	void SetArtist(const wchar_t* artist);
	void SetAlbum (const wchar_t* album);
	void SetExtra (const wchar_t* extra);

	// 文字列の長さ取得
	int GetTitleLength () const { return m_title.Length();  }
	int GetArtistLength() const { return m_artist.Length(); }
	int GetAlbumLength () const { return m_album.Length();  }

	// ルビ取得
	const wx::String& GetTitleRuby () const { return m_title_rb; }
	const wx::String& GetArtistRuby() const { return m_artist_rb; }
	const wx::String& GetAlbumRuby () const { return m_album_rb; }

	// ルビ設定
	void SetTitleRuby (const wchar_t* title) { m_title_rb = title; }
	void SetArtistRuby(const wchar_t* artist) { m_artist_rb = artist; }
	void SetAlbumRuby (const wchar_t* album) { m_album_rb = album; }

public:
	// 時間を文字列化
	static void FormatTime(wchar_t* buf, UINT duration);

	// メディア間でコピーする
	static void Copy(Media& to, const Media& from);

private:
	wx::String	m_path;
	wx::String	m_title;
	wx::String	m_artist;
	wx::String	m_album;
	wx::String	m_extra;
	wx::String	m_title_rb;
	wx::String	m_artist_rb;
	wx::String	m_album_rb;
	UINT		m_duration;
	bool		m_seekable;
};
