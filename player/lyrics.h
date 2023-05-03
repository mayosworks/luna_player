//=============================================================================
// 歌詞データ
//=============================================================================
#pragma once

#include <map>
#include "lib/wx_string.h"


//-----------------------------------------------------------------------------
// 歌詞データ
//-----------------------------------------------------------------------------
class Lyrics
{
public:
	Lyrics();
	virtual ~Lyrics();

	// TimeRatioの変更は、歌詞読込前のみ有効
	void   SetTimeRatio(double def_time_ratio);
	double GetTimeRatio();

	// 歌詞データ読込（force_time_tagはカラオケタグをタイムタグに変換）
	bool LoadFile(const wchar_t* lrc_path, bool force_time_tag);

	// 読込済みデータの解放
	void FreeData();

	// 歌詞データを読み込んでいるか？
	bool IsLoaded() const;

	bool HasLyrics(UINT time_ms) const;

	// 指定時間の歌詞データを取得
	const wchar_t* GetLyrics (UINT time_ms);
	const wchar_t* GetCurLine(UINT time_ms);
	const wchar_t* GetPreLine(UINT time_ms);

	// 指定時間まで進める（ミリ秒単位）
	bool SeekTime(UINT seek_time);

	// 歌詞情報を取得
	const wchar_t* GetTitle() const;
	const wchar_t* GetArtist() const;
	const wchar_t* GetAlbum() const;
	const wchar_t* GetWords () const;

private:
	// 行単位のチェックを行い、歌詞解析にデータを渡す
	bool HasLyricsData(const wx::String& line);

	// 行から歌詞データを解析し、バッファに格納
	bool parseLyrics(wx::String& line, UINT& last_time, bool force_time_tag);

	// ＠タグの解析
	bool parseAtTag(const wx::String& line);

	// 時間タグの解析
	bool parseTime(const wx::String& str_data, UINT& time_val);

	// TimeRatioとOffsetを反映させる
	bool modifyTime();

	// 文字を数値に変換 with Multiple num
	UINT toNumMul(wchar_t ch, UINT multiple = 1);

	// 浮動小数点型 or 整数型のデータを解析
	bool toInteger(const wx::String& str_data, int&    int_val);
	bool toDouble (const wx::String& str_data, double& dbl_val);

	// カラオケタグからタイムタグへ変換
	void convKaraokeToTime(wx::String& line);

private:
	typedef std::map<UINT,wx::String>	TSMap;
	typedef TSMap::iterator				TSMapIter;

	// 歌詞データ
	TSMap		m_lrc_data;		// １文字 or １行単位の歌詞
	TSMap		m_lrc_pre_data;	// １行単位の歌詞（先行しての描画用）
	TSMap		m_lrc_line;		// １行単位の歌詞（計算用）
	UINT		m_lrc_index;		// デバッグ用

	// データカーソル（レンダリング時使用）
	TSMapIter	m_lrc_cursor;
	TSMapIter	m_lrc_cur_cursor;
	TSMapIter	m_lrc_pre_cursor;

	// 時間補正＠タグ
	int			m_time_offset;	// 時間オフセット（マイナスあり）
	double		m_time_ratio;	// 時間補正レート

	// 情報
	wx::String	m_title;
	wx::String	m_artist;
	wx::String	m_album;
	wx::String	m_words;	// 歌詞部分のみ
};
