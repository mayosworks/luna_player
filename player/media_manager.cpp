//=============================================================================
// メディア管理
//=============================================================================

//#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include "lib/wx_misc.h"
#include "lib/wx_string.h"
#include "lib/wx_text_rw.h"
#include "lib/wx_hash.h"
#include "luna.h"
#include "media.h"
#include "media_manager.h"
#include "phonetic.h"

// 内部クラス
MediaManager::Impl* MediaManager::m_impl = NULL;

//-----------------------------------------------------------------------------
// 内部クラス
//-----------------------------------------------------------------------------
class MediaManager::Impl
{
public:
	Impl();
	~Impl();

	void Clear();
	void RemoveNotExists();

	int GetNum() const;
	Media* Get(int no);

	void Insert(Media& media, int pos);
	void Update(Media& media, int pos);
	void Remove(int pos);

	void SetEnableRuby(bool enable);
	void UpdateRuby(Media& media);

	void Sort(SortKey key, SortType type);
	SortKey GetSortKey() const;
	SortType GetSortType() const;

	void SetFilter(const wchar_t* filter);
	bool IsFiltered() const;

	int GetRawNum() const;

	bool LoadList(const wchar_t* path);
	bool SaveList(const wchar_t* path);

	bool LoadM3uList(const wchar_t* path);
	bool LoadKbmList(const wchar_t* path);

public:
	bool IsMatch(const Media* media, const wchar_t* filter, int length);
	bool Contains(const wchar_t* text, int tlen, const wchar_t* cond, int clen);

//private:
	typedef std::list<Media*>	MediaList;
	typedef MediaList::iterator	MediaListIter;

	typedef std::vector<Media*>	MediaVec;
	typedef MediaVec::iterator	MediaVecIter;

	typedef std::map<wx::String, int>	StringMap;
	typedef StringMap::iterator			StringMapIter;

	Phonetic		m_phonetic;
	MediaList		m_all_media;
	MediaVec		m_flt_media;
	StringMap		m_artists;
	StringMap		m_albums;
	wx::String		m_filter;
	SortKey			m_sort_key;
	SortType		m_sort_type;
	bool			m_use_ruby;
};

//-----------------------------------------------------------------------------
// 内部コンストラクタ
//-----------------------------------------------------------------------------
MediaManager::Impl::Impl()
	: m_sort_key(SORT_KEY_NONE)
	, m_use_ruby(false)
{
	m_phonetic.Initialize();

	m_flt_media.reserve(1024);
	m_filter = L"";

	Clear();
}

//-----------------------------------------------------------------------------
// 内部デストラクタ
//-----------------------------------------------------------------------------
MediaManager::Impl::~Impl()
{
	Clear();
	m_phonetic.Finalize();
}

//-----------------------------------------------------------------------------
// リストクリア
//-----------------------------------------------------------------------------
void MediaManager::Impl::Clear()
{
	for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
		delete (*it);
	}

	m_all_media.clear();
	m_flt_media.clear();

	m_artists.clear();
	m_artists[wx::String("(null)")] = 0;

	m_albums.clear();
	m_albums[wx::String("(null)")] = 0;
}

//-----------------------------------------------------------------------------
// 存在しない項目を削除
//-----------------------------------------------------------------------------
void MediaManager::Impl::RemoveNotExists()
{
	MediaList temp_list;

	for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
		Media* media = *it;
		if (PathFileExists(media->GetPath())) {
			temp_list.push_back(media);
		}
		else {
			delete media;
		}
	}

	m_all_media.clear();
	m_flt_media.clear();

	m_all_media = temp_list;
	wx::String filter = m_filter;
	SetFilter(filter.Get());
}

//-----------------------------------------------------------------------------
// メディア数取得
//-----------------------------------------------------------------------------
int MediaManager::Impl::GetNum() const
{
	return static_cast<int>(m_flt_media.size());
}

//-----------------------------------------------------------------------------
// メディア取得
//-----------------------------------------------------------------------------
Media* MediaManager::Impl::Get(int pos)
{
	if (pos >= 0 && pos < GetNum()) {
		return m_flt_media[pos];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// メディア追加
//-----------------------------------------------------------------------------
void MediaManager::Impl::Insert(Media& media, int /*pos*/)
{
	// コピーを作って登録する
	Media* item = new Media(media);
	if (item) {
		m_all_media.push_back(item);

		if (m_filter.IsEmpty()) {
			m_flt_media.push_back(item);
		}
		else if (IsMatch(item, m_filter.Get(), m_filter.Length())) {
			m_flt_media.push_back(item);
		}

		wx::String artist = item->GetArtist();
		if (artist.IsEmpty()) {
			artist = L"(null)";
		}

		if (m_artists.find(artist) == m_artists.end()) {
			int index = static_cast<int>(m_artists.size()) + 1;
			m_artists.insert(StringMap::value_type(artist, index));
		}

		wx::String album = item->GetAlbum();
		if (album.IsEmpty()) {
			album = L"(null)";
		}

		if (m_albums.find(album) == m_albums.end()) {
			int index = static_cast<int>(m_albums.size()) + 1;
			m_albums.insert(StringMap::value_type(album, index));
		}
	}

	WX_TRACE("MediaManager/Insert: %s\n", media.GetPath());
}

//-----------------------------------------------------------------------------
// メディア更新
//-----------------------------------------------------------------------------
void MediaManager::Impl::Update(Media& media, int pos)
{
	Media* item = Get(pos);
	if (item) {
		Media::Copy(*item, media);
	}

	WX_TRACE("MediaManager/Update: %s,%d\n", media.GetPath(), pos);
}

//-----------------------------------------------------------------------------
// メディア削除
//-----------------------------------------------------------------------------
void MediaManager::Impl::Remove(int pos)
{
	Media* item = Get(pos);
	if (item) {
		MediaVecIter it = (m_flt_media.begin() + pos);
		if (it != m_flt_media.end()) {
			m_flt_media.erase(it);
		}

		m_all_media.remove(item);
		delete item;
	}

	WX_TRACE("MediaManager/Remove: %d\n", pos);
}

//-----------------------------------------------------------------------------
// ソート
//-----------------------------------------------------------------------------
void MediaManager::Impl::Sort(SortKey key, SortType type)
{
	class CompareMetadata
	{
	public:
		CompareMetadata(SortKey key, SortType type)
			: m_key(key)
			, m_type(type)
		{
		}

		bool operator()(const Media* lhs, const Media* rhs)
		{
			static DWORD COMP_FLAGS = NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH;

			const wchar_t* str1 = NULL;
			const wchar_t* str2 = NULL;
			int num1 = 0;
			int num2 = 0;

			if (m_key == SORT_KEY_TITLE) {
				str1 = lhs->GetTitle();
				num1 = lhs->GetTitleLength();

				str2 = rhs->GetTitle();
				num2 = rhs->GetTitleLength();
			}
			else if (m_key == SORT_KEY_ARTIST) {
				str1 = lhs->GetArtist();
				num1 = lhs->GetArtistLength();

				str2 = rhs->GetArtist();
				num2 = rhs->GetArtistLength();
			}
			else if (m_key == SORT_KEY_ALBUM) {
				str1 = lhs->GetAlbum();
				num1 = lhs->GetAlbumLength();

				str2 = rhs->GetAlbum();
				num2 = rhs->GetAlbumLength();
			}
			else if (m_key == SORT_KEY_DURATION) {
				num1 = lhs->GetDuration();
				num2 = rhs->GetDuration();
			}

			bool result = false;

			if (str1 != NULL && str2 != NULL) {
				int comp_ret = CompareString(LOCALE_USER_DEFAULT, COMP_FLAGS, str1, num1, str2, num2);
				if (m_type == SORT_TYPE_GREATER) {
					result = (comp_ret == CSTR_GREATER_THAN);
				}
				else {
					result = (comp_ret == CSTR_LESS_THAN);
				}
			}
			else {
				result = (m_type == SORT_TYPE_GREATER) ? (num1 > num2) : (num1 < num2);
			}

			return result;
		}

	private:
		SortKey	m_key;
		SortType m_type;
	};

	// ソートモード切り替え
	if (type == SORT_TYPE_AUTO) {
		if (m_sort_key == key) {
			switch (m_sort_type) {
			case SORT_TYPE_AUTO:
			case SORT_TYPE_NONE:
				type = SORT_TYPE_LESS;
				break;

			case SORT_TYPE_LESS:
				type = SORT_TYPE_GREATER;
				break;

			case SORT_TYPE_GREATER:
				type = SORT_TYPE_NONE;
				break;
			}
		}
		else {
			type = SORT_TYPE_LESS;
		}
	}

	if (type == SORT_TYPE_NONE) {
		wx::String filter = m_filter;
		SetFilter(filter);
	}
	else {
		CompareMetadata cmp_meta_data(key, type);
		std::stable_sort(m_flt_media.begin(), m_flt_media.end(), cmp_meta_data);
	}

	m_sort_key = key;
	m_sort_type = type;
}

//-----------------------------------------------------------------------------
// ソートキー取得
//-----------------------------------------------------------------------------
MediaManager::SortKey MediaManager::Impl::GetSortKey() const
{
	return m_sort_key;
}

//-----------------------------------------------------------------------------
// ソート種類取得
//-----------------------------------------------------------------------------
MediaManager::SortType MediaManager::Impl::GetSortType() const
{
	return m_sort_type;
}

//-----------------------------------------------------------------------------
// よみがなフィルタを有効にする
//-----------------------------------------------------------------------------
void MediaManager::Impl::SetEnableRuby(bool enable)
{
	m_use_ruby = enable;
}

//-----------------------------------------------------------------------------
// よみがな更新
//-----------------------------------------------------------------------------
void MediaManager::Impl::UpdateRuby(Media& media)
{
	if (!m_use_ruby) {
		return;
	}

	wchar_t buf[128];

	if (m_phonetic.Convert(media.GetTitle(), buf, WX_LENGTHOF(buf))) {
		media.SetTitleRuby(buf);
	}

	if (m_phonetic.Convert(media.GetArtist(), buf, WX_LENGTHOF(buf))) {
		media.SetArtistRuby(buf);
	}

	if (m_phonetic.Convert(media.GetAlbum(), buf, WX_LENGTHOF(buf))) {
		media.SetAlbumRuby(buf);
	}
}

//-----------------------------------------------------------------------------
// フィルタリング
//-----------------------------------------------------------------------------
void MediaManager::Impl::SetFilter(const wchar_t* filter)
{
	WX_NULL_ASSERT(filter);

	m_flt_media.clear();
	m_flt_media.reserve(m_all_media.size());

	m_filter = filter;
	if (m_filter.IsEmpty()) {
		for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
			m_flt_media.push_back(*it);
		}
	}
	else {
		for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
			if (IsMatch(*it, m_filter.Get(), m_filter.Length())) {
				m_flt_media.push_back(*it);
			}
		}
	}

	WX_TRACE("MediaManager/Filter: %s\n", filter);
}

//-----------------------------------------------------------------------------
// フィルタリングされてるか？
//-----------------------------------------------------------------------------
bool MediaManager::Impl::IsFiltered() const
{
	return !m_filter.IsEmpty();
}

//-----------------------------------------------------------------------------
// 本当のメディア数取得
//-----------------------------------------------------------------------------
int MediaManager::Impl::GetRawNum() const
{
	return static_cast<int>(m_all_media.size());
}

//-----------------------------------------------------------------------------
// フィルタ文字列にマッチするか？
//-----------------------------------------------------------------------------
bool MediaManager::Impl::IsMatch(const Media* media, const wchar_t* filter, int length)
{
	WX_NULL_ASSERT(media);

	// タイトル検索
	if (Contains(media->GetTitle(), media->GetTitleLength(), filter, length)) {
		return true;
	}

	// アーティスト検索
	if (Contains(media->GetArtist(), media->GetArtistLength(), filter, length)) {
		return true;
	}

	// アルバム検索
	if (Contains(media->GetAlbum(), media->GetAlbumLength(), filter, length)) {
		return true;
	}

	if (m_use_ruby) {
		// タイトルよみ検索
		if (Contains(media->GetTitleRuby().Get(), media->GetTitleRuby().Length(), filter, length)) {
			return true;
		}

		// アーティストよみ検索
		if (Contains(media->GetArtistRuby().Get(), media->GetArtistRuby().Length(), filter, length)) {
			return true;
		}

		// アルバムよみ検索
		if (Contains(media->GetAlbumRuby().Get(), media->GetAlbumRuby().Length(), filter, length)) {
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// 文字列が含まれるか？
//-----------------------------------------------------------------------------
bool MediaManager::Impl::Contains(const wchar_t* text_ptr, int text_len, const wchar_t* cond_ptr, int cond_len)
{
	if (text_len < cond_len) {
		return false;
	}

	const DWORD COMP_FLAGS = NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH;

	int text_idx = 0;
	while (cond_len <= text_len) {
		int check_len = std::min<int>(text_len, cond_len);
		if (CompareString(LOCALE_USER_DEFAULT, COMP_FLAGS, &text_ptr[text_idx], check_len, cond_ptr, cond_len) == CSTR_EQUAL) {
			return true;
		}

		++text_idx;
		--text_len;
	}

	return false;
}

//-----------------------------------------------------------------------------
// プレイリストの読み込み
//-----------------------------------------------------------------------------
bool MediaManager::Impl::LoadList(const wchar_t* path)
{
	WX_NULL_ASSERT(path);
	WX_TRACE("MediaManager/LoadList: %s\n", path);

	// m3u/m3u8
	if (PathMatchSpec(path, TEXT("*.m3u;*.m3u8"))) {
		return LoadM3uList(path);
	}

	// kbm/kbmu
	if (PathMatchSpec(path, TEXT("*.kbm;*.kbmu"))) {
		return LoadKbmList(path);
	}

	wx::TextReader reader;

	if (!reader.Open(path)) {
		return false;
	}

	wx::String title, artist, album;
	UINT duration = 0;
	bool seekable = false;

	while (reader.HasMoreData()) {
		wx::String line = reader.ReadLine(true);
		if (!line.IsEmpty()) {
			// 拡張情報
			if (line[0] == L'#') {
				if (line.Substr(0, 4) == L"#TI:") {
					title = line.Substr(4);
				}

				if (line.Substr(0, 4) == L"#AR:") {
					artist = line.Substr(4);
				}

				if (line.Substr(0, 4) == L"#AL:") {
					album = line.Substr(4);
				}

				if (line.Substr(0, 4) == L"#SD:") {
					seekable = (line[4] != L'0');

					int value = 0;
					if (StrToIntEx(line.Substr(6), STIF_DEFAULT, &value)) {
						duration = value;
					}
				}
			}
			else if (line[0] != L';') {
				Media media;

				media.SetPath(line);

				media.SetDuration(duration);
				media.SetSeekable(seekable);
				media.SetTitle(title);
				media.SetArtist(artist);
				media.SetAlbum(album);

				UpdateRuby(media);

				Insert(media, -1);

				duration = 0;
				seekable = false;

				title.Clear();
				artist.Clear();
				album.Clear();
			}
		}
	}

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// プレイリストの保存
//-----------------------------------------------------------------------------
bool MediaManager::Impl::SaveList(const wchar_t* path)
{
	WX_NULL_ASSERT(path);
	WX_TRACE("MediaManager/SaveList: %s\n", path);

	bool is_m3ua = (PathMatchSpec(path, TEXT("*.m3u")) != 0);
	bool is_m3u8 = (PathMatchSpec(path, TEXT("**.m3u8")) != 0);

	wx::TextWriter writer;

	if (!writer.Open(path, is_m3ua || is_m3u8)) {
		return false;
	}

	wchar_t buf[32];

	// Luna専用
	if (!is_m3ua && !is_m3u8) {
		// ヘッダの注釈を記入
		writer.WriteLine(L";================================================");
		writer.WriteLine(L"; Luna プレイリストファイル");
		writer.WriteLine(L"; ※内容を編集しないでください！");
		writer.WriteLine(L";================================================");

		for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
			Media* media = *it;

			writer.Write(L"#TI:");
			writer.WriteLine(media->GetTitle());

			writer.Write(L"#AR:");
			writer.WriteLine(media->GetArtist());

			writer.Write(L"#AL:");
			writer.WriteLine(media->GetAlbum());

			wsprintf(buf, L"#SD:%d,%d", media->IsSeekable()? 1 : 0, media->GetDuration());
			writer.WriteLine(buf);

			writer.WriteLine(media->GetPath());
		}
	}
	else {
		UINT cp = is_m3u8 ? CP_UTF8 : CP_ACP;
		char buf[1024];

		for (MediaListIter it = m_all_media.begin(); it != m_all_media.end(); ++it) {
			Media* media = *it;
			WideCharToMultiByte(cp, 0, media->GetPath(), -1, buf, 1023, NULL, NULL);
			writer.WriteBin(buf, lstrlenA(buf));
			writer.WriteBin("\r\n", 2);
		}
	}

	writer.Close();
	return true;
}

//-----------------------------------------------------------------------------
// M3Uプレイリストの読み込み
//-----------------------------------------------------------------------------
bool MediaManager::Impl::LoadM3uList(const wchar_t* path)
{
	wx::TextReader reader;

	if (!reader.Open(path)) {
		return false;
	}

	wchar_t base[MAX_PATH];
	lstrcpy(base, path);
	PathRemoveFileSpec(base);

	while (reader.HasMoreData()) {
		wx::String line = reader.ReadLine(true);
		if (!line.IsEmpty()) {
			// パス情報
			if (line[0] != L'#') {
				if (PathIsRelative(line.Get())) {
					wchar_t dest[MAX_PATH];
					PathCombine(dest, base, line.Get());
					line = dest;
				}

				if (PathFileExists(line.Get())) {
					Luna::Add(line.Get());
				}
			}
		}
	}

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// KbMediaPlayerプレイリストの読み込み
//-----------------------------------------------------------------------------
bool MediaManager::Impl::LoadKbmList(const wchar_t* path)
{
	wx::TextReader reader;

	if (!reader.Open(path)) {
		return false;
	}

	if (wx::String(reader.ReadLine()) != L"KbMIDI Player Play List") {
		reader.Close();
		return false;
	}

	while (reader.HasMoreData()) {
		wx::String line = reader.ReadLine(true);
		if (!line.IsEmpty()) {
			// パス情報
			if (line[0] == L'0' && line[1] == L':') {
				wx::String path = line.Substr(2, -1);
				if (PathFileExists(path.Get())) {
					Luna::Add(path.Get());
				}
			}
		}
	}

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// マネージャー初期化
//-----------------------------------------------------------------------------
bool MediaManager::Initialize()
{
	m_impl = new Impl();
	if (!m_impl) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// マネージャー終了
//-----------------------------------------------------------------------------
void MediaManager::Finalize()
{
	WX_SAFE_DELETE(m_impl);
}

//-----------------------------------------------------------------------------
// リストクリア
//-----------------------------------------------------------------------------
void MediaManager::Clear()
{
	m_impl->Clear();
}

//-----------------------------------------------------------------------------
// 存在しない項目を削除
//-----------------------------------------------------------------------------
void MediaManager::RemoveNotExists()
{
	m_impl->RemoveNotExists();
}

//-----------------------------------------------------------------------------
// メディア数取得
//-----------------------------------------------------------------------------
int MediaManager::GetNum()
{
	return m_impl->GetNum();
}

//-----------------------------------------------------------------------------
// メディア取得
//-----------------------------------------------------------------------------
Media* MediaManager::Get(int pos)
{
	return m_impl->Get(pos);
}

void MediaManager::GetArtistList(StringVector& vec)
{
	Impl::StringMap& map = m_impl->m_artists;
	vec.clear();
	vec.reserve(map.size() + 1);

	vec.push_back(L"");
	for (Impl::StringMapIter it = map.begin(); it != map.end(); ++it) {
		vec.push_back(it->first);
	}
}

int MediaManager::GetArtistId(const wchar_t* artist)
{
	int size = lstrlen(artist) * 2;
	return static_cast<int>(wx::CalcFNV32a(artist, size));
}

void MediaManager::GetAlbumList(StringVector& vec)
{
	Impl::StringMap& map = m_impl->m_albums;
	vec.clear();
	vec.reserve(map.size() + 1);

	vec.push_back(L"");
	for (Impl::StringMapIter it = map.begin(); it != map.end(); ++it) {
		vec.push_back(it->first);
	}
}

int MediaManager::GetAlbumId(const wchar_t* album)
{
	int size = lstrlen(album) * 2;
	return static_cast<int>(wx::CalcFNV32a(album, size));
}

//-----------------------------------------------------------------------------
// メディア追加
//-----------------------------------------------------------------------------
void MediaManager::Insert(Media& media, int pos)
{
	m_impl->Insert(media, pos);
}

//-----------------------------------------------------------------------------
// メディア更新
//-----------------------------------------------------------------------------
void MediaManager::Update(Media& media, int pos)
{
	m_impl->Update(media, pos);
}

//-----------------------------------------------------------------------------
// メディア削除
//-----------------------------------------------------------------------------
void MediaManager::Remove(int pos)
{
	m_impl->Remove(pos);
}

//-----------------------------------------------------------------------------
// よみがなフィルタを有効にする
//-----------------------------------------------------------------------------
void MediaManager::SetEnableRuby(bool enable)
{
	m_impl->SetEnableRuby(enable);
}

//-----------------------------------------------------------------------------
// よみがな更新
//-----------------------------------------------------------------------------
void MediaManager::UpdateRuby(Media& media)
{
	m_impl->UpdateRuby(media);
}

//-----------------------------------------------------------------------------
// ソート
//-----------------------------------------------------------------------------
void MediaManager::Sort(SortKey key, SortType type)
{
	WX_ASSERT(key < SORT_KEY_NUM, "キーが範囲外です！");
	WX_ASSERT(type < SORT_TYPE_NUM, "タイプが範囲外です！");
	m_impl->Sort(key, type);
}

//-----------------------------------------------------------------------------
// ソートキー取得
//-----------------------------------------------------------------------------
MediaManager::SortKey MediaManager::GetSortKey()
{
	return m_impl->GetSortKey();
}

//-----------------------------------------------------------------------------
// ソート種類取得
//-----------------------------------------------------------------------------
MediaManager::SortType MediaManager::GetSortType()
{
	return m_impl->GetSortType();
}

//-----------------------------------------------------------------------------
// フィルタ
//-----------------------------------------------------------------------------
void MediaManager::SetFilter(const wchar_t* filter)
{
	m_impl->SetFilter(filter);
}

//-----------------------------------------------------------------------------
// フィルタされてるか？
//-----------------------------------------------------------------------------
bool MediaManager::IsFiltered()
{
	return m_impl->IsFiltered();
}

//-----------------------------------------------------------------------------
// フィルタをかける前の値
//-----------------------------------------------------------------------------
int MediaManager::GetRawNum()
{
	return m_impl->GetRawNum();
}

//-----------------------------------------------------------------------------
// デフォルトプレイリストの読み込み
//-----------------------------------------------------------------------------
bool MediaManager::LoadDefaultList()
{
	wchar_t path[MAX_PATH];

	GetModuleFileName(NULL, path, MAX_PATH);
	PathRenameExtension(path, TEXT(".lst"));

	return m_impl->LoadList(path);
}

//-----------------------------------------------------------------------------
// デフォルトプレイリストの保存
//-----------------------------------------------------------------------------
bool MediaManager::SaveDefaultList()
{
	wchar_t path[MAX_PATH];

	GetModuleFileName(NULL, path, MAX_PATH);
	PathRenameExtension(path, TEXT(".lst"));

	return m_impl->SaveList(path);
}

//-----------------------------------------------------------------------------
// プレイリストの読み込み
//-----------------------------------------------------------------------------
bool MediaManager::LoadList(const wchar_t* path)
{
	return m_impl->LoadList(path);
}

//-----------------------------------------------------------------------------
// プレイリストの保存
//-----------------------------------------------------------------------------
bool MediaManager::SaveList(const wchar_t* path)
{
	return m_impl->SaveList(path);
}
