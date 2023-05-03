//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "wx_misc.h"
#include "wx_string.h"

namespace wx {

//-----------------------------------------------------------------------------
// デフォルトコンストラクタ
//-----------------------------------------------------------------------------
String::String()
	: m_string(NULL)
	, m_length(0)
{
	SetString(L"", 0);
}

//-----------------------------------------------------------------------------
// Unicode文字列コンストラクタ
//-----------------------------------------------------------------------------
String::String(const wchar_t* str, int len)
	: m_string(NULL)
	, m_length(0)
{
	SetString(str, len);
}

//-----------------------------------------------------------------------------
// Shift_JIS文字列指定コンストラクタ
//-----------------------------------------------------------------------------
String::String(const char* str, int len)
	: m_string(NULL)
	, m_length(0)
{
	int wlen = MultiByteToWideChar(CP_ACP, 0, str, len, NULL, 0);
	m_string = AllocString(wlen + 1);
	if (m_string) {
		MultiByteToWideChar(CP_ACP, 0, str, len, m_string, wlen);
		m_string[wlen] = L'\0';
		m_length = wlen - 1;
	}
}

//-----------------------------------------------------------------------------
// コピーコンストラクタ
//-----------------------------------------------------------------------------
String::String(const String& obj)
	: m_string(NULL)
	, m_length(0)
{
	SetString(obj.m_string, obj.m_length);
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
String::~String()
{
	FreeString(m_string);
	m_string = NULL;
}

//-----------------------------------------------------------------------------
// 文字列オブジェクトを代入する
//-----------------------------------------------------------------------------
String&	String::operator = (const String& str)
{
	SetString(str.m_string, str.m_length);
	return *this;
}

//-----------------------------------------------------------------------------
// 文字列を代入する
//-----------------------------------------------------------------------------
String& String::operator = (const wchar_t* str)
{
	SetString(str);
	return *this;
}

//-----------------------------------------------------------------------------
// 文字列オブジェクトを追加する
//-----------------------------------------------------------------------------
String& String::operator += (const String& obj)
{
	AddString(obj.m_string, obj.m_length);
	return *this;
}

//-----------------------------------------------------------------------------
// 文字列を追加する
//-----------------------------------------------------------------------------
String& String::operator += (const wchar_t* str)
{
	AddString(str);
	return *this;
}

//-----------------------------------------------------------------------------
// 文字列オブジェクトを結合する
//-----------------------------------------------------------------------------
String String::operator + (const String& obj) const
{
	String newstr(*this);
	newstr.AddString(obj.m_string, obj.m_length);
	return newstr;
}

//-----------------------------------------------------------------------------
// 文字列を結合する
//-----------------------------------------------------------------------------
String String::operator + (const wchar_t* str) const
{
	String newstr(*this);
	newstr.AddString(str);
	return newstr;
}

//-----------------------------------------------------------------------------
// 文字列が同じかを比較する
//-----------------------------------------------------------------------------
bool String::operator == (const wchar_t* str) const
{
	return (lstrcmpW(m_string, str) == 0);
}

//-----------------------------------------------------------------------------
// 文字列が同じでないかを比較する
//-----------------------------------------------------------------------------
bool String::operator != (const wchar_t* str) const
{
	return (lstrcmpW(m_string, str) != 0);
}

//-----------------------------------------------------------------------------
// 文字列が大きいかを比較する
//-----------------------------------------------------------------------------
bool String::operator > (const wchar_t* str) const
{
	return (lstrcmpW(m_string, str) > 0);
}

//-----------------------------------------------------------------------------
// 文字列が小さいかを比較する
//-----------------------------------------------------------------------------
bool String::operator < (const wchar_t* str) const
{
	return (lstrcmpW(m_string, str) < 0);
}

//-----------------------------------------------------------------------------
// 特定位置の文字を取得する
//-----------------------------------------------------------------------------
wchar_t& String::operator [](int pos)
{
	return m_string[pos];
}

//-----------------------------------------------------------------------------
// 特定位置の文字を取得する
//-----------------------------------------------------------------------------
wchar_t String::At(int pos) const
{
	if (pos < m_length) {
		return m_string[pos];
	}

	return L'\0';
}

//-----------------------------------------------------------------------------
// 文字列が有効かどうかを取得する
//-----------------------------------------------------------------------------
bool String::IsValid() const
{
	return (m_string != NULL);
}

//-----------------------------------------------------------------------------
// 文字列が空かどうかを取得する
//-----------------------------------------------------------------------------
bool String::IsEmpty() const
{
	if (m_string[0]) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 文字列クリア
//-----------------------------------------------------------------------------
void String::Clear()
{
	SetString(L"", 0);
}

//-----------------------------------------------------------------------------
// 文字列をフォーマットしてセットする（最大1023文字）
//-----------------------------------------------------------------------------
String&	String::Print(const wchar_t* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	VPrint(fmt, ap);
	va_end(ap);

	return *this;
}

//-----------------------------------------------------------------------------
//  文字列をフォーマットしてセットする（最大1023文字）
//-----------------------------------------------------------------------------
String&	String::VPrint(const wchar_t* fmt, va_list ap)
{
	wchar_t buf[1024];

	wvsprintfW(buf, fmt, ap);
	buf[WX_NUMBEROF(buf) -  1] = L'\0';

	SetString(buf);
	return *this;
}

//-----------------------------------------------------------------------------
// 文字列の一部を消す
//-----------------------------------------------------------------------------
String& String::Erase(int start, int len)
{
	if (start < m_length) {
		// 終端が範囲外なら消す
		if (len < 0 || start + len >= m_length) {
			m_string[start] = L'\0';
			return *this;
		}

		int mvLen = m_length - start - len + 1;

		MoveMemory(&m_string[start], &m_string[start + len], mvLen * sizeof(wchar_t));
		m_length = start + mvLen;
		m_string[m_length - 1] = L'\0';
	}

	return *this;
}

//-----------------------------------------------------------------------------
// 文字を検索する
//-----------------------------------------------------------------------------
int String::Find(wchar_t cond, int start) const
{
	if (start >= m_length) {
		return NPOS;
	}

	for (int i = start; i < m_length; ++i) {
		if (m_string[i] == cond) {
			return i;
		}
	}

	return NPOS;
}

//-----------------------------------------------------------------------------
// 文字列を検索する
//-----------------------------------------------------------------------------
int String::Find(const wchar_t* cond, int start) const
{
	int len = m_length;
	int clen = lstrlenW(cond);
	if (start >= len) {
		return NPOS;
	}

	for (int i = start; i < len; ++i) {
		if (m_string[i] == cond[0]) {
			int mlen = len - i;
			if (len < clen) {
				return NPOS;
			}

			for (int j = 0; j < clen; ++j) {
				if (m_string[i + j] != cond[j]) {
					mlen = NPOS;
					break;
				}
			}

			if (mlen != NPOS) {
				return i;
			}
		}
	}

	return NPOS;
}

//-----------------------------------------------------------------------------
// 文字列を分割する
//-----------------------------------------------------------------------------
String String::Substr(int start, int len) const
{
	// コピーできない
	if (start >= m_length) {
		return String();
	}

	// すべてコピー
	if (len < 0 || (start + len) >= m_length) {
		return String(&m_string[start]);
	}

	return String(&m_string[start], len);
}

//-----------------------------------------------------------------------------
// 文字列をトークン分解する
//-----------------------------------------------------------------------------
String String::GetToken(wchar_t delim, int start, int& next)
{
	// 範囲外の場合
	if (start > m_length || next == NPOS) {
		next = NPOS;
		return String();
	}

	// 最後の文字の場合
	if (At(start) == L'\0') {
		next = NPOS;
		return String();
	}

	int pos = Find(delim, start);
	if (pos != NPOS) {
		next = pos + 1;
		return Substr(start, pos - start);
	}

	next = m_length + 1;
	return Substr(start, -1);
}

//-----------------------------------------------------------------------------
//　文字を置換する
//-----------------------------------------------------------------------------
String String::Replace(wchar_t cond, wchar_t ch) const
{
	String ret(*this);

	for (int i = 0; i < m_length; ++i) {
		if (ret.m_string[i] == cond) {
			ret.m_string[i] = ch;
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 文字列を置換する
//-----------------------------------------------------------------------------
String String::Replace(const wchar_t* cond, const wchar_t* str) const
{
	String ret;
	int start = 0, pos = 0;
	int clen = lstrlenW(cond);
	
	while (pos != NPOS) {
		pos = Find(cond, pos);
		if (pos != NPOS) {
			ret += Substr(start, pos - start);
			ret += str;

			pos += clen;
			start = pos;
		}
		else {
			ret += Substr(start);
		}
	}

	if (ret.IsEmpty()) {
		ret = *this;
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 文字列を大文字化する
//-----------------------------------------------------------------------------
String String::ToUpper() const
{
	String ret(*this);

	for (int i = 0; i < m_length; ++i) {
		wchar_t ch = ret.m_string[i];
		if (ch >= L'a' && ch <= L'z') {
			ret.m_string[i] -= (L'a' - L'A');
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 文字列を小文字化する
//-----------------------------------------------------------------------------
String String::ToLower() const
{
	String ret(*this);

	for (int i = 0; i < m_length; ++i) {
		wchar_t ch = ret.m_string[i];
		if (ch >= L'A' && ch <= L'Z') {
			ret.m_string[i] += (L'a' - L'A');
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 文字列をトリミングする
//-----------------------------------------------------------------------------
String String::Trim() const
{
	for (int i = 0; i < m_length; ++i) {
		if (m_string[i] != L' ') {
			for (int j = m_length - 1; j >= i; --j) {
				if (m_string[j] != L' ') {
					return Substr(i, j - i + 1);
				}
			}

			return Substr(i, -1);
		}
	}

	return String(*this);
}

//-----------------------------------------------------------------------------
// 文字列バッファ確保
//-----------------------------------------------------------------------------
wchar_t* String::AllocString(int len)
{
	size_t size = len * sizeof(wchar_t);
	void* buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);

	return static_cast<wchar_t*>(buf);
}

//-----------------------------------------------------------------------------
// 文字列バッファ解放
//-----------------------------------------------------------------------------
void String::FreeString(wchar_t* str)
{
	if (str) {
		HeapFree(GetProcessHeap(), 0, str);
	}
}

//-----------------------------------------------------------------------------
// Unicode文字列をセットする
//-----------------------------------------------------------------------------
void String::SetString(const wchar_t* str, int len)
{
	if (m_string) {
		FreeString(m_string);
		m_string = NULL;
	}

	int wlen = (len < 0)? lstrlenW(str) : len;

	m_string = AllocString(wlen + 1);
	m_length = 0;

	if (m_string) {
		CopyMemory(m_string, str, wlen * sizeof(wchar_t));
		m_string[wlen] = L'\0';
		m_length = wlen;
	}
}

//-----------------------------------------------------------------------------
// Unicode文字列を追加する
//-----------------------------------------------------------------------------
void String::AddString(const wchar_t* str, int len)
{
	int addlen = (len < 0)? lstrlenW(str) : len;
	int newlen = m_length + addlen;

	wchar_t* newstr = AllocString(newlen + 1);
	if (newstr) {
		CopyMemory(newstr, m_string, m_length * sizeof(wchar_t));
		CopyMemory(&newstr[m_length], str, addlen * sizeof(wchar_t));
		newstr[newlen] = L'\0';

		FreeString(m_string);

		m_string = newstr;
		m_length = newlen;
	}
}

} //namespace wx
