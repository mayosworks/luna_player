//=============================================================================
// 歌詞データ
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "lib/wx_text_rw.h"
#include "lyrics.h"

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Lyrics::Lyrics()
	: m_time_ratio(1.0)
	, m_time_offset(0)
	, m_lrc_index(1)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Lyrics::~Lyrics()
{
}

//-----------------------------------------------------------------------------
// タイムレートを設定
//-----------------------------------------------------------------------------
void Lyrics::SetTimeRatio(double def_time_ratio)
{
	// 歌詞データ読み込み前のみ有効
	if (!IsLoaded()) {
		m_time_ratio = def_time_ratio;
	}
}

//-----------------------------------------------------------------------------
// タイムレートを取得
//-----------------------------------------------------------------------------
double Lyrics::GetTimeRatio()
{
	return m_time_ratio;
}

//-----------------------------------------------------------------------------
// ファイルからデータをロードする
//-----------------------------------------------------------------------------
bool Lyrics::LoadFile(const wchar_t* lrc_path, bool force_time_tag)
{
	wx::TextReader reader;

	if (!reader.Open(lrc_path)) {
		return false;
	}

	// 読込済みの場合はそれを解放
	if (IsLoaded()) {
		FreeData();
	}

	const wchar_t* all = reader.ReadAll();
	int len = static_cast<int>(lstrlen(all));

	// []がない場合は歌詞データとは判断しない
	bool found1 = false, found2 = false;
	for (int i = 0; i < len && !(found1 && found2); ++i) {
		if (all[i] == L'[') {
			found1 = true;
		}

		if (all[i] == L']') {
			found2 = true;
		}
	}

	if (!found1 || !found2) {
		FreeData();
		return false;
	}

	UINT last_time = 0;

	while (reader.HasMoreData()) {
		wx::String line = reader.ReadLine(true);
		if (HasLyricsData(line)) {
			parseLyrics(line, last_time, force_time_tag);
		}
	}

	if (m_lrc_data.empty()) {
		FreeData();
		return false;
	}

	// @TimeRatio/@Offset反映
	if (!modifyTime()) {
		FreeData();
		return false;
	}

	// 処理用カーソルを準備する
	m_lrc_cursor     = m_lrc_data.begin();
	m_lrc_cur_cursor = m_lrc_pre_data.begin();
	m_lrc_pre_cursor = m_lrc_pre_data.begin();
	return true;
}

//-----------------------------------------------------------------------------
// データを解放する
//-----------------------------------------------------------------------------
void Lyrics::FreeData()
{
	m_lrc_data.clear();
	m_lrc_cursor = m_lrc_data.end();

	m_lrc_pre_data.clear();
	m_lrc_cur_cursor = m_lrc_pre_data.end();
	m_lrc_pre_cursor = m_lrc_pre_data.end();

	m_lrc_line.clear();
	m_lrc_index = 1;

	m_time_ratio = 1.0;
	m_time_offset = 0;

	m_artist.Clear();
	m_title.Clear();
	m_album.Clear();
	m_words.Clear();
}

//-----------------------------------------------------------------------------
// データを読み込んでいるか
//-----------------------------------------------------------------------------
bool Lyrics::IsLoaded() const
{
	return !m_lrc_data.empty();
}

//-----------------------------------------------------------------------------
// 指定された時間で歌詞があるか
//-----------------------------------------------------------------------------
bool Lyrics::HasLyrics(UINT time_ms) const
{
	if (m_lrc_cursor == m_lrc_data.end()) {
		return false;
	}

	if (time_ms < m_lrc_cursor->first) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 指定された時間の歌詞を取得
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetLyrics(UINT time_ms)
{
	if (!HasLyrics(time_ms)) {
		return NULL;
	}

	return (m_lrc_cursor++)->second;
}

//-----------------------------------------------------------------------------
// 指定された時間の１行分の歌詞を取得
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetCurLine(UINT time_ms)
{
	if (m_lrc_cur_cursor == m_lrc_pre_data.end()) {
		return NULL;
	}

	if (time_ms < m_lrc_cur_cursor->first) {
		return NULL;
	}

	return (m_lrc_cur_cursor++)->second;
}

//-----------------------------------------------------------------------------
// 指定された時間の先行分の歌詞を取得
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetPreLine(UINT time_ms)
{
	if (m_lrc_pre_cursor == m_lrc_pre_data.end()) {
		return L"";
	}

	if (time_ms < m_lrc_pre_cursor->first) {
		return NULL;
	}

	TSMapIter retIter = ++m_lrc_pre_cursor;
	if (retIter == m_lrc_pre_data.end()) {
		return NULL;
	}

	return retIter->second;
}

//-----------------------------------------------------------------------------
// 歌詞をシークする
//-----------------------------------------------------------------------------
bool Lyrics::SeekTime(UINT seek_time)
{
	if (!IsLoaded()) {
		return false;
	}

	m_lrc_cursor    = m_lrc_data.begin();
	m_lrc_cur_cursor = m_lrc_pre_data.begin();
	m_lrc_pre_cursor = m_lrc_cur_cursor;

	// カーソルリセット
	if (seek_time == 0) {
		return true;
	}

	while (seek_time >= m_lrc_cursor->first) {
		if (++m_lrc_cursor == m_lrc_data.end()) {
			break;
		}
	}

	if (m_lrc_cursor != m_lrc_data.begin()) {
		m_lrc_cursor--;
	}

	while (seek_time >= m_lrc_cur_cursor->first) {
		if (++m_lrc_cur_cursor == m_lrc_pre_data.end()) {
			break;
		}
	}

	if (m_lrc_cur_cursor != m_lrc_pre_data.begin()) {
		m_lrc_cur_cursor--;
	}

	m_lrc_pre_cursor = m_lrc_cur_cursor;
	return true;
}

//-----------------------------------------------------------------------------
// タイトル名を取得する
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetTitle() const
{
	return m_title;
}

//-----------------------------------------------------------------------------
// アーティスト名を取得する
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetArtist() const
{
	return m_artist;
}

//-----------------------------------------------------------------------------
// アルバム名を取得する
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetAlbum() const
{
	return m_album;
}

//-----------------------------------------------------------------------------
// 歌詞データ全体を取得する
//-----------------------------------------------------------------------------
const wchar_t* Lyrics::GetWords() const
{
	return m_words;
}

//-----------------------------------------------------------------------------
// 行単位のチェックを行い、歌詞データがあるかどうかを判断する
//-----------------------------------------------------------------------------
bool Lyrics::HasLyricsData(const wx::String& line)
{
	// ブランクの行は処理しない
	if (line.IsEmpty()) {
		m_words += L"\r\n";
		return false;
	}

	// 改行、コメントは処理しない
	if (line.At(0) == L'\n' || line.At(0) == L';' || line.At(0) == L'/') {
		m_words += line;
		m_words += L"\r\n";
		return false;
	}

	// ＠タグの解析
	if (parseAtTag(line.Get())) {
		return false;
	}

	// タイムタグのない行をスキップする
	if (line.Find(L'[') == wx::String::NPOS || line.Find(L']') == wx::String::NPOS) {
		m_words += line;
		m_words += L"\r\n";
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// １行分のデータを歌詞バッファに格納する
//-----------------------------------------------------------------------------
bool Lyrics::parseLyrics(wx::String& line, UINT& last_time, bool force_time_tag)
{
	m_lrc_line[m_lrc_index++] = line;

	// 行頭タイムタグに変換
	if (force_time_tag) {
		convKaraokeToTime(line);
	}

	UINT first_time = 0;
	int end_pos = 0;
	bool is_first = true, has_more_data = true, has_first_time = false, has_valid = false;
	wx::String words;

	// タイムタグを解析し、タイムマップに格納
	while (has_more_data) {
		int std_pos = line.Find(L'[', end_pos);
		end_pos = line.Find(L']', end_pos);
		if (std_pos == wx::String::NPOS || end_pos == wx::String::NPOS) {
			break;
		}

		UINT cur_time = 0;
		bool is_valid_time = parseTime(line.Substr(std_pos + 1, end_pos - std_pos - 1), cur_time);

		// 次のタイムタグの開始位置を取得
		int next_pos = line.Find(L'[', end_pos);

		// これ以上タグはない
		if (next_pos == wx::String::NPOS) {
			if (is_valid_time) {
				words += line.Substr(end_pos + 1);
			}

			has_more_data = false;
		}
		else {
			if (is_valid_time) {
				words += line.Substr(end_pos + 1, next_pos - end_pos - 1);
			}

			end_pos = next_pos;
		}

		// タイムタグが解析できない場合は、その部分は無視する
		if (!is_valid_time) {
			continue;
		}

		// 行の最初のタグの前にある、歌詞を取得
		if (is_first) {
			wx::String prefix = line.Substr(0, std_pos);
			prefix += words;
			words = prefix;
			is_first = false;
		}

		// 歌詞マップに格納
		if (cur_time != last_time || words.Length() > 0) {
			m_lrc_data[cur_time] = words;
			has_valid = true;

			// 各行の最初の時間を保存しておく
			if (!has_first_time) {
				first_time = cur_time;
				has_first_time = true;
			}
		}

		last_time = cur_time;
	}

	if (has_valid) {
		// 先行歌詞マップに入れる
		m_lrc_pre_data[first_time] = words;
	}

	m_words += words;
	m_words += L"\r\n";
	return true;
}

//-----------------------------------------------------------------------------
// ＠タグの解析（＠タグの場合は、trueを返す）
//-----------------------------------------------------------------------------
bool Lyrics::parseAtTag(const wx::String& line)
{
	// １文字目が＠でない場合は非対称
	if (line.At(0) != L'@') {
		return false;
	}

	// = で区切られていない場合は、値の取得ができない
	int pos = line.Find(L'=');
	if (pos == wx::String::NPOS) {
		return false;
	}

	// @TimeRatio
	if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		line.Substr(0, 10).Get(), -1, L"@TimeRatio", -1) == CSTR_EQUAL) {
		double dbl_val = 0.0;
		if (toDouble(line.Substr(pos + 1), dbl_val)) {
			// 0.0 であってはいけない
			if (dbl_val > 0.0) {
				m_time_ratio = dbl_val;
			}
		}

		return true;
	}

	// @Offset
	if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		line.Substr(0, 7).Get(), -1, L"@Offset", -1) == CSTR_EQUAL) {
		return toInteger(line.Substr(pos + 1), m_time_offset);
	}

	// @Artist
	if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		line.Substr(0, 7).Get(), -1, L"@Artist", -1) == CSTR_EQUAL) {
		m_artist = line.Substr(pos + 1);
		return true;
	}

	// @Title
	if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		line.Substr(0, 6).Get(), -1, L"@Title", -1) == CSTR_EQUAL) {
		m_title = line.Substr(pos + 1);
		return true;
	}

	// @Album
	if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		line.Substr(0, 6).Get(), -1, L"@Album", -1) == CSTR_EQUAL) {
		m_album = line.Substr(pos + 1);
		return true;
	}

	// 不明タグは無視する
	return true;
}

//-----------------------------------------------------------------------------
// [00:00] / [00:00:00] 形式のタイムタグを解析し、ミリ秒として返す
//-----------------------------------------------------------------------------
bool Lyrics::parseTime(const wx::String& str_data, UINT &time_val)
{
	int len = str_data.Length();
	// xx:xx:xx or xx:xx の桁長チェック
	if (len != 5 && len != 8) {
		return false;
	}

	// mm:ss 形式チェック
	if (str_data.At(2) != L':') {
		return false;
	}

	int mm = 0, ss = 0, nn = 0;

	// 分、秒部分を変換
	if (!toInteger(str_data.Substr(0, 2), mm)) {
		return false;
	}

	if (!toInteger(str_data.Substr(3, 2), ss)) 	{
		return false;
	}

	// ミリ秒部分を変換
	if (len == 8) {
		// mm:ss:nn 形式チェック
		if (str_data.At(5) != L':') {
			return false;
		}

		if (!toInteger(str_data.Substr(6, 2), nn)) {
			return false;
		}
	}

	// 最終時間を算出する
	time_val = (mm * 60 + ss) * 1000 + nn * 10;
	return true;
}

//-----------------------------------------------------------------------------
// TimeRatioとOffsetを反映させる
//-----------------------------------------------------------------------------
bool Lyrics::modifyTime()
{
	// どちらも修正分が０であれば、即OKを返す
	if (m_time_ratio == 1.0 && m_time_offset == 0) {
		return true;
	}

	TSMap lrc_buf;
	TSMapIter wk_cur;
	double calc_time = 0.0;
	UINT new_time = 0;

	// 時間計算をした結果を格納
	for (wk_cur = m_lrc_data.begin(); wk_cur != m_lrc_data.end(); ++wk_cur) {
		calc_time = wk_cur->first / m_time_ratio + m_time_offset;
		if (calc_time < 0.0) {
			calc_time = 0.0;
		}

		new_time = static_cast<UINT>(calc_time);
		lrc_buf[new_time] = wk_cur->second;
	}

	// 元に戻す
	m_lrc_data.clear();
	for (wk_cur = lrc_buf.begin(); wk_cur != lrc_buf.end(); ++wk_cur) {
		m_lrc_data[wk_cur->first] = wk_cur->second;
	}

	lrc_buf.clear();

	// 時間計算をした結果を格納
	for (wk_cur = m_lrc_pre_data.begin(); wk_cur != m_lrc_pre_data.end(); ++wk_cur) {
		calc_time = wk_cur->first / m_time_ratio + m_time_offset;
		if (calc_time < 0.0) {
			calc_time = 0.0;
		}

		new_time = static_cast<UINT>(calc_time);
		lrc_buf[new_time] = wk_cur->second;
	}

	// 元に戻す
	m_lrc_pre_data.clear();
	for (wk_cur = lrc_buf.begin(); wk_cur != lrc_buf.end(); ++wk_cur) {
		m_lrc_pre_data[wk_cur->first] = wk_cur->second;
	}

	lrc_buf.clear();
	return true;
}

//-----------------------------------------------------------------------------
// 文字を数値に変換 with Multiple num
//-----------------------------------------------------------------------------
UINT Lyrics::toNumMul(wchar_t ch, UINT multiple)
{
	return (ch - L'0') * multiple;
}
//-----------------------------------------------------------------------------
// 整数型のデータを解析
//-----------------------------------------------------------------------------
bool Lyrics::toInteger(const wx::String& str_data, int &int_val)
{
	int_val = 0;

	if (!StrToIntExW(str_data, STIF_DEFAULT, &int_val)) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 浮動小数点型のデータを解析
//-----------------------------------------------------------------------------
bool Lyrics::toDouble(const wx::String& str_data, double &dbl_val)
{
	double dvml = 1.0;
	int int_val = 0;

	int pos = str_data.Find(L'.');
	dbl_val = 0.0;

	// 整数部を数値化（小数部なしの場合）
	if (pos == wx::String::NPOS) {
		if (!toInteger(str_data, int_val)) {
			return false;
		}
	}

	// 整数部を数値化（小数部ありの場合）
	if (pos != wx::String::NPOS && pos > 0) {
		if (!toInteger(str_data.Substr(0, pos), int_val)) {
			return false;
		}
	}

	// 整数値を小数値の整数部にセット
	dbl_val = static_cast<double>(int_val);

	// 小数部を数値化
	if (pos != wx::String::NPOS) {
		// 小数部を数値化
		if (!toInteger(str_data.Substr(pos + 1), int_val)) {
			return false;
		}

		// べき乗計算
		for (int i = 1; i < str_data.Substr(pos).Length(); ++i) {
			dvml *= 10.0;
		}

		// 小数部の加算
		dbl_val += static_cast<double>(int_val) / dvml;
	}

	return true;
}

//-----------------------------------------------------------------------------
// カラオケタグからタイムタグに変換（SAKURA Scriptも無効化）
//-----------------------------------------------------------------------------
void Lyrics::convKaraokeToTime(wx::String& line)
{
	// [xx:xx:xx]AAA[xx:xx:xx]BBB -> [xx:xx:xx]AAABBBの行頭タイムタグに変換
	for (int pos0 = line.Find(L']'); pos0 != wx::String::NPOS;) {
		int pos1 = line.Find(L'[', pos0);
		if (pos1 == wx::String::NPOS) {
			return;
		}

		int pos2 = line.Find(L']', pos1);
		if (pos2 == wx::String::NPOS) {
			return;
		}

		line.Erase(pos1, pos2 - pos1 + 1);
		pos0 = line.Find(L']');
	}
}
