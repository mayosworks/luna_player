﻿//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================
#pragma once

namespace wx {

//-----------------------------------------------------------------------------
//! @class	IniFile
//! @brief	構成設定ファイル 取得／設定
//-----------------------------------------------------------------------------
class IniFile
{
public:
	static const int MAX_SIZE = 1024;	///<INIファイル上の項目最大サイズ

public:
	//-------------------------------------------------------------------------
	//! @brief	インスタンス指定コンストラクタ
	//!
	//! @param	inst		インスタンスハンドル、NULLなら自分自身を指す
	//-------------------------------------------------------------------------
	IniFile(HINSTANCE inst = NULL);

	//-------------------------------------------------------------------------
	//! @brief	パス指定コンストラクタ
	//-------------------------------------------------------------------------
	explicit IniFile(PCTSTR ini_path);

	//-------------------------------------------------------------------------
	//! @brief	デストラクタ
	//-------------------------------------------------------------------------
	~IniFile();

	//-------------------------------------------------------------------------
	//! @brief	セクション名を設定
	//!
	//! @param	section		セクション名
	//-------------------------------------------------------------------------
	void SetSection(PCTSTR section);

	//-------------------------------------------------------------------------
	//! @brief	文字列を設定
	//!
	//! @param	key			キー名
	//! @param	value		文字列
	//!
	//! @retval	true	設定成功
	//! @retval	false	設定失敗
	//-------------------------------------------------------------------------
	bool SetText(PCTSTR key, PCTSTR value);

	//-------------------------------------------------------------------------
	//! @brief	数値を設定
	//!
	//! @param	key			キー名
	//! @param	value		数値
	//!
	//! @retval	true	設定成功
	//! @retval	false	設定失敗
	//-------------------------------------------------------------------------
	bool SetUint(PCTSTR key, UINT value);

	//-------------------------------------------------------------------------
	//! @brief	真偽値を設定
	//!
	//! @param	key			キー名
	//! @param	value		真偽値
	//!
	//! @retval	true	設定成功
	//! @retval	false	設定失敗
	//-------------------------------------------------------------------------
	bool SetBool(PCTSTR key, bool value);

	//-------------------------------------------------------------------------
	//! @brief	文字列を取得する
	//!
	//! @param	key			キー名
	//! @param	defval		デフォルト値
	//!
	//! @return	キーに関連付けられた文字列
	//-------------------------------------------------------------------------
	PCTSTR GetText(PCTSTR key, PCTSTR defval);

	//-------------------------------------------------------------------------
	//! @brief	数値を取得する
	//!
	//! @param	key			キー名
	//! @param	defval		デフォルト値
	//!
	//! @return	キーに関連付けられた数値
	//-------------------------------------------------------------------------
	UINT GetUint(PCTSTR key, UINT defval) const;

	//-------------------------------------------------------------------------
	//! @brief	真偽値を取得する
	//!
	//! @param	key			キー名
	//! @param	defval		デフォルト値
	//!
	//! @return	キーに関連付けられた真偽値
	//-------------------------------------------------------------------------
	bool GetBool(PCTSTR key, bool defval) const;

private:
	TCHAR	m_ini_path[MAX_PATH];
	TCHAR	m_section[MAX_PATH];
	TCHAR	m_rbuffer[MAX_SIZE];
};

} //namespace wx
