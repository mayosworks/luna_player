﻿//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================
#pragma once

namespace wx {

//-----------------------------------------------------------------------------
//! @class	Application
//! @brief	アプリケーションインターフェイス
//-----------------------------------------------------------------------------
class Application
{
public:
	//-------------------------------------------------------------------------
	//! @brief	コンストラクタ
	//-------------------------------------------------------------------------
	Application();

	//-------------------------------------------------------------------------
	//! @brief	デストラクタ
	//-------------------------------------------------------------------------
	virtual ~Application();

	//-------------------------------------------------------------------------
	//! @brief	メイン実行関数
	//!
	//! @param	app_inst	アプリケーションインスタンスハンドル
	//!
	//! @return	終了コード
	//-------------------------------------------------------------------------
	int Main(HINSTANCE app_inst);

protected:
	//-------------------------------------------------------------------------
	//! @brief	初期化関数
	//!
	//! @param	app_inst	アプリケーションインスタンスハンドル
	//!
	//! @retval	true		初期化成功
	//! @retval	false		初期化失敗
	//-------------------------------------------------------------------------
	virtual bool Initialize(HINSTANCE app_inst) = 0;

	//-------------------------------------------------------------------------
	//! @brief	終了関数
	//-------------------------------------------------------------------------
	virtual void Finalize() = 0;

	//-------------------------------------------------------------------------
	//! @brief	実行関数
	//!
	//! @param	msg			メッセージ
	//!
	//! @retval	true		アプリ継続
	//! @retval	false		アプリ終了
	//-------------------------------------------------------------------------
	virtual bool Execute(MSG* msg) = 0;

protected:
	static const int EXIT_NORMAL = 0;		///<正常終了時の終了コード
	static const int EXIT_FAILED = 1;		///<異常終了時の終了コード
};

} //namespace wx
