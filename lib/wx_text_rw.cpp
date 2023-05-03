//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2006 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "wx_misc.h"
#include "wx_text_rw.h"

namespace wx {

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
LineParser::LineParser()
	: m_text(NULL)
	, m_next(NULL)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
LineParser::~LineParser()
{
	Clear();
}

//-----------------------------------------------------------------------------
// 文字列データをセット
//-----------------------------------------------------------------------------
bool LineParser::SetData(const wchar_t* data, UINT bytes)
{
	WX_NULL_ASSERT(data);

	// オブジェクトの再利用は禁止
	if (m_text) {
		return false;
	}

	// サイズの指定がないときは、自動計算する
	if (!bytes) {
		bytes = lstrlenW(data) * sizeof(wchar_t) + 4;
	}

	m_text = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes));
	if (!m_text) {
		return false;
	}

	int end_pos = lstrlenW(data);

	// \r\nを\nのみにする
	for (int i = 0, j = 0; i < end_pos; ++i, ++j) {
		if (data[i] == L'\r') {
			m_text[j] = ((data[i + 1] == L'\n')? data[++i] : L'\n');
		}
		else {
			m_text[j] = data[i];
		}
	}

	m_next = m_text;
	return true;
}

//-----------------------------------------------------------------------------
// 保持している文字列データをクリア
//-----------------------------------------------------------------------------
void LineParser::Clear()
{
	if (m_text) {
		HeapFree(GetProcessHeap(), 0, m_text);
		m_text = NULL;
		m_next = NULL;
	}
}

//-----------------------------------------------------------------------------
// まだ行があるか
//-----------------------------------------------------------------------------
bool LineParser::HasMoreLine() const
{
	return (m_next != NULL);
}

//-----------------------------------------------------------------------------
// １行分の文字列を取得
//-----------------------------------------------------------------------------
const wchar_t* LineParser::GetLine(bool trimming)
{
	if (!HasMoreLine()) {
		return NULL;
	}

	wchar_t* ret = m_next, *p;
	for (p = m_next; p && *p; p++) {
		if (*p != L'\n') {
			continue;
		}

		*p++ = L'\0';

		// トリミング処理
		if (trimming) {
			// 先頭のトリミング
			for (; *p == L' '; ++p) {}

			// 終端のトリミング
			for (wchar_t* t = p - 1; t != m_next; t--) {
				if (*t != L' ') {
					break;
				}

				*t = L'\0';
			}
		}
		break;
	}

	// 次のポイントを設定
	m_next = (p == m_next)? NULL : p;
	return ret;
}


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
TextReader::TextReader()
	: m_text(NULL)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
TextReader::~TextReader()
{
	Close();
}

//-----------------------------------------------------------------------------
// ファイルを読み取り用に開く
//-----------------------------------------------------------------------------
bool TextReader::Open(PCTSTR path)
{
	WX_NULL_ASSERT(path);

	// オブジェクトの再利用は禁止
	if (!path || m_text) {
		return false;
	}

	HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD file_size = GetFileSize(file, NULL);

	// 全体サイズ＋＠メモリを確保する
	BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, file_size + 64));
	if (!buffer) {
		CloseHandle(file);
		return false;
	}

	DWORD readed_size;

	// ファイル全体を一度読み込む
	if (!ReadFile(file, buffer, file_size, &readed_size, NULL)) {
		readed_size = 0;
	}

	CloseHandle(file);

	// きちんと読み込めたか？
	if (file_size != readed_size) {
		HeapFree(GetProcessHeap(), 0, buffer);
		return false;
	}

	buffer[readed_size] =L'\0';

	DWORD conv_size = file_size * sizeof(wchar_t) + 64;

	// Unicode変換バッファを確保する
	m_text = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, conv_size));
	if (!m_text) {
		HeapFree(GetProcessHeap(), 0, buffer);
		return false;
	}

	// UTF-16 LE(BOM)は判定せず、コピー。
	if (buffer[0] == 0xFF && buffer[1] == 0xFE) {
		CopyMemory(m_text, buffer + 2, file_size - 2);
		m_text[file_size / 2 - 1] = L'\0';
	}
	// UTF-16 BOM BEはLEに変換
	else if (buffer[0] == 0xFE && buffer[1] == 0xFF) {
		union WcharByte
		{
			wchar_t	w;
			BYTE	b[2];
		};

		WcharByte wb;
		for (DWORD i = 2; i < file_size; i += 2) {
			wb.b[1] = buffer[i];
			wb.b[0] = buffer[i + 1];
			m_text[i / 2] = wb.w;
		}

		m_text[file_size / 2 - 1] = L'\0';
	}
	// UTF-8 BOM
	else if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
		if (!MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<PCSTR>(buffer + 3), file_size - 3, m_text, conv_size)) {
			HeapFree(GetProcessHeap(), 0, buffer);
			Close();
			return false;
		}
	}
	// UNICODEの場合はコピー、SJISはUNICODE変換する
	else if (IsTextUnicode(buffer, file_size, NULL)) {
		CopyMemory(m_text, buffer, file_size);
		m_text[file_size / 2] = L'\0';
	}
	// それ以外はデフォルトCPとみなす
	else {
		if (!MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<PCSTR>(buffer), file_size, m_text, conv_size)) {
			HeapFree(GetProcessHeap(), 0, buffer);
			Close();
			return false;
		}
	}

	HeapFree(GetProcessHeap(), 0, buffer);

	// LineParserに渡す
	if (!m_line.SetData(m_text, conv_size)) {
		Close();
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// ファイルを閉じる
//-----------------------------------------------------------------------------
void TextReader::Close()
{
	m_line.Clear();

	if (m_text) {
		HeapFree(GetProcessHeap(), 0, m_text);
		m_text = NULL;
	}
}

//-----------------------------------------------------------------------------
// まだデータがあるか
//-----------------------------------------------------------------------------
bool TextReader::HasMoreData() const
{
	return m_line.HasMoreLine();
}

//-----------------------------------------------------------------------------
// すべてのデータを取得
//-----------------------------------------------------------------------------
const wchar_t* TextReader::ReadAll() const
{
	return m_text;
}

//-----------------------------------------------------------------------------
// １行分のデータを取得
//-----------------------------------------------------------------------------
const wchar_t* TextReader::ReadLine(bool trimming)
{
	return m_line.GetLine(trimming);
}


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
TextWriter::TextWriter()
	: m_file(INVALID_HANDLE_VALUE)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
TextWriter::~TextWriter()
{
	Close();
}

//-----------------------------------------------------------------------------
// ファイルを書き込み用に開く
//-----------------------------------------------------------------------------
bool TextWriter::Open(PCTSTR path, bool no_bom)
{
	WX_NULL_ASSERT(path);

	// オブジェクトの再利用は禁止
	if (!path || m_file != INVALID_HANDLE_VALUE) {
		return false;
	}

	// テキストファイルを開く
	m_file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (m_file == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!no_bom) {
		// BOMを書き込む
		WORD bom = 0xFEFF;

		WriteBin(&bom, sizeof(bom));
	}

	return true;
}

//-----------------------------------------------------------------------------
// ファイルを閉じる
//-----------------------------------------------------------------------------
void TextWriter::Close()
{
	if (m_file != INVALID_HANDLE_VALUE) {
		CloseHandle(m_file);
	}

	m_file = INVALID_HANDLE_VALUE;
}

//-----------------------------------------------------------------------------
// 指定文字列を書き込む
//-----------------------------------------------------------------------------
bool TextWriter::Write(const wchar_t* data) const
{
	return WriteBin(data, lstrlenW(data) * 2);
}

//-----------------------------------------------------------------------------
// 指定文字列に改行をつけて書き込む
//-----------------------------------------------------------------------------
bool TextWriter::WriteLine(const wchar_t* data) const
{
	if (!Write(data)) {
		return false;
	}

	// 改行文字を書き込む
	WCHAR crlf[2] = { L'\r', L'\n' };
	return WriteBin(crlf, sizeof(crlf));
}

//-----------------------------------------------------------------------------
// バイナリデータを書き込む
//-----------------------------------------------------------------------------
bool TextWriter::WriteBin(const void *data, DWORD size) const
{
	DWORD written_size = 0;

	if (m_file == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!WriteFile(m_file, data, size, &written_size, NULL)) {
		return false;
	}

	return(written_size == size);
}

//-----------------------------------------------------------------------------
// 指定文字列を書き込む（最大1023文字）
//-----------------------------------------------------------------------------
bool TextWriter::Print(const wchar_t* fmt, ...) const
{
	va_list ap;

	va_start(ap, fmt);
	bool ret = VPrint(fmt, ap);
	va_end(ap);

	return ret;
}

//-----------------------------------------------------------------------------
// 指定文字列を書き込む（最大1023文字）
//-----------------------------------------------------------------------------
bool TextWriter::VPrint(const wchar_t* fmt, va_list ap) const
{
	wchar_t buf[1024];

	wvsprintf(buf, fmt, ap);
	buf[WX_NUMBEROF(buf) - 1] = L'\0';

	return Write(buf);
}

} //namespace wx
