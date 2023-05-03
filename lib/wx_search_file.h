﻿//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2008 MAYO.
//=============================================================================
#pragma once

namespace wx {

//-----------------------------------------------------------------------------
//! @class	IFileFoundCallback
//! @brief	ファイル検索時のコールバック
//-----------------------------------------------------------------------------
class IFileFoundCallback
{
public:
	//-------------------------------------------------------------------------
	//! @brief	デストラクタ
	//-------------------------------------------------------------------------
	virtual ~IFileFoundCallback(){}

	//-------------------------------------------------------------------------
	//! @brief	ディレクトリが見つかった時のコールバック
	//!
	//! @param	path	ディレクトリのパス
	//! @param	depth	検索開始のディレクトリからの相対階層
	//!
	//! @retval	true	ディレクトリを検索する
	//! @retval	false	ディレクトリは検索しない
	//-------------------------------------------------------------------------
	virtual bool FoundDir(PCTSTR /*path*/, int /*depth*/)
	{
		// デフォルトは検索する
		return true;
	}

	//-------------------------------------------------------------------------
	//! @brief	ファイルが見つかった時のコールバック
	//!
	//! @param	path	ファイルのパス
	//! @param	ext		ファイルの拡張子
	//! @param	num		検索開始から見つけたファイル数
	//!
	//! @retval	true	検索を継続する
	//! @retval	false	検索を中止する
	//-------------------------------------------------------------------------
	virtual bool FoundFile(PCTSTR path, PCTSTR ext, int index) = 0;
};

//-----------------------------------------------------------------------------
//! @class	SearchFile
//! @brief	ファイル検索
//-----------------------------------------------------------------------------
class SearchFile : protected IFileFoundCallback
{
public:
	//-------------------------------------------------------------------------
	//! @brief	コンストラクタ
	//!
	//! @param	cb			コールバッククラス
	//-------------------------------------------------------------------------
	SearchFile(IFileFoundCallback* cb = NULL);

	//-------------------------------------------------------------------------
	//! @brief	デストラクタ
	//-------------------------------------------------------------------------
	virtual ~SearchFile();

	//-------------------------------------------------------------------------
	//! @brief	コールバック設定
	//!
	//! @param	cb			コールバッククラス、NULLでクリアする
	//-------------------------------------------------------------------------
	void SetCallback(IFileFoundCallback* cb);

	//-------------------------------------------------------------------------
	//! @brief	ファイル検索実行
	//!
	//! @param	dir			検索するルートディレクトリ
	//! @param	cond		検索条件（例："*.*"）
	//! @param	depth		検索階層（0は無制限、1以上は階層制限あり）
	//!
	//! @retval	true		ファイルを見つけた
	//! @retval	false		ファイルは見つからなかった
	//-------------------------------------------------------------------------
	bool Execute(PCTSTR dir, PCTSTR cond, int depth = 0);

	//-------------------------------------------------------------------------
	//! @brief	見つけたファイル数を取得する
	//!
	//! @return 見つけたファイル数
	//-------------------------------------------------------------------------
	int GetFoundNum() const;

	//-------------------------------------------------------------------------
	//! @brief	見つけたファイルのパスを取得する（コールバック未設定時のみ取得可能）
	//!
	//! @param	index		ファイルのインデックス (0 ～ GetFoundNum()-1)
	//!
	//! @return ファイルパス
	//-------------------------------------------------------------------------
	PCTSTR GetFoundPath(int index) const;

public:
	static const int MAX_FILE_NUM = 512;	//< 最大検索可能ファイル数

private:
	void Clear();

	// 検索処理
	bool Find(PCTSTR dir, PCTSTR cond, int depth, IFileFoundCallback* cb);
	bool FindDir(PCTSTR dir, PCTSTR cond, int depth, IFileFoundCallback* cb);

	// IFoundCallback実装
	bool FoundDir (PCTSTR path, int depth);
	bool FoundFile(PCTSTR path, PCTSTR ext, int index);

private:
	TCHAR*				m_path;
	int					m_num;
	int					m_depth;
	IFileFoundCallback*	m_cb;
};

} //namespace wx
