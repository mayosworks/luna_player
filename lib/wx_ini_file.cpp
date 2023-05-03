//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include "wx_ini_file.h"

namespace wx {

//-----------------------------------------------------------------------------
// インスタンス指定コンストラクタ
//-----------------------------------------------------------------------------
IniFile::IniFile(HINSTANCE inst)
{
	m_ini_path[0] = TEXT('\0');
	m_section[0] = TEXT('\0');
	m_rbuffer[0] = TEXT('\0');

	GetModuleFileName(inst, m_ini_path, MAX_PATH);
	PathRenameExtension(m_ini_path, TEXT(".ini"));
}

//-----------------------------------------------------------------------------
// パス指定コンストラクタ
//-----------------------------------------------------------------------------
IniFile::IniFile(PCTSTR ini_path)
{
	m_ini_path[0] = TEXT('\0');
	m_section[0] = TEXT('\0');
	m_rbuffer[0] = TEXT('\0');

	lstrcpyn(m_ini_path, ini_path, MAX_PATH);
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
IniFile::~IniFile()
{
}

//-----------------------------------------------------------------------------
// セクション名を設定
//-----------------------------------------------------------------------------
void IniFile::SetSection(PCTSTR section)
{
	lstrcpyn(m_section, section, MAX_PATH);
}

//-----------------------------------------------------------------------------
// 文字列を設定
//-----------------------------------------------------------------------------
bool IniFile::SetText(PCTSTR key, PCTSTR value)
{
	if (!WritePrivateProfileString(m_section, key, value, m_ini_path)) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 数値を設定
//-----------------------------------------------------------------------------
bool IniFile::SetUint(PCTSTR key, UINT value)
{
	TCHAR buf[32];

	wsprintf(buf, TEXT("%d"), value);
	return SetText(key, buf);
}

//-----------------------------------------------------------------------------
// 真偽値を設定
//-----------------------------------------------------------------------------
bool IniFile::SetBool(PCTSTR key, bool value)
{
	return SetText(key, value? TEXT("1") : TEXT("0"));
}

//-----------------------------------------------------------------------------
// 文字列を取得する
//-----------------------------------------------------------------------------
PCTSTR IniFile::GetText(PCTSTR key, PCTSTR defval)
{
	UINT ret = GetPrivateProfileString(m_section, key, defval, m_rbuffer, MAX_SIZE, m_ini_path);
	if (ret == (MAX_SIZE - 1)) {
		lstrcpyn(m_rbuffer, defval, MAX_SIZE);
	}

	return m_rbuffer;
}

//-----------------------------------------------------------------------------
// 数値を取得する
//-----------------------------------------------------------------------------
UINT IniFile::GetUint(PCTSTR key, UINT defval) const
{
	return GetPrivateProfileInt(m_section, key, defval, m_ini_path);
}

//-----------------------------------------------------------------------------
// 真偽値を取得する
//-----------------------------------------------------------------------------
bool IniFile::GetBool(PCTSTR key, bool defval) const
{
	return (GetUint(key, defval) == 1);
}

} //namespace wx
