﻿//=============================================================================
// よみがな取得
//=============================================================================
#pragma once

#include <windows.h>

//-----------------------------------------------------------------------------
// Declarations
//-----------------------------------------------------------------------------
interface IFELanguage;

//-----------------------------------------------------------------------------
// よみがな取得
// ・IMM-API(IME)の逆変換を利用して、漢字からよみがなを取得します。
// ・100%正しい答えがでるとは限りません。
//-----------------------------------------------------------------------------
class Phonetic
{
public:
	Phonetic();
	~Phonetic();

	bool Initialize();
	void Finalize();

	// 漢字からよみなへ変換。
	bool Convert(const wchar_t* text, wchar_t* phonetic, UINT max_len);

private:
	IFELanguage* m_fe_lang;
};