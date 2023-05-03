//=============================================================================
// サウンドドライバ共通ヘッダー
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <windows.h>
#include <mmreg.h>
#include <map>
#include <string>
#include "lib/wx_misc.h"
#include "lib/wx_string.h"

class Driver;

//-----------------------------------------------------------------------------
// サウンドドライバ基底クラス
//-----------------------------------------------------------------------------
class Driver
{
public:
	Driver();
	virtual ~Driver() {}

	// デバイス列挙
	virtual bool EnumDevices() = 0;

	// デバイスを開く
	virtual bool Open(const wchar_t* guid, const WAVEFORMATEX& wfx) = 0;

	// デバイスを閉じる
	virtual void Close() = 0;

	// データや再生時間をリセット
	virtual void Reset() = 0;

	// 出力開始
	virtual bool Start() = 0;

	// 出力停止
	virtual bool Stop() = 0;

	// 書き込み（戻り値は書き込めたバイト数）
	virtual UINT Write(const void* data, UINT size) = 0;

	// フラッシュ（trueになったら完了）
	virtual bool Flush() = 0;

	// 再生位置を時間単位で取得（Resetでリセットされます）
	virtual UINT GetPlayTime() const = 0;

	// ボリューム設定、0-100%で設定
	virtual void SetVolume(int volume);

	// 出力形式を取得
	virtual bool GetFormat(WAVEFORMATEX& wfx) const = 0;

protected:
	// デバイス追加
	void AddDevice(const wchar_t* name, const wchar_t* guid);

	// ボリューム設定初期化
	void SetupVolume(const WAVEFORMATEX& format);

	// ボリューム制御
	void ProcessVolume(void* dest, const void* data, UINT size) const;

	// ソースサイズから出力サイズへ
	UINT SourceToOutputSize(UINT size) const;

	// 出力サイズからソースサイズへ
	UINT OutputToSourceSize(UINT size) const;

protected:
	enum Conversion
	{
		PASS_THROUGH,
		INT8_TO_INT16,
		INT8_TO_INT24,
		INT8_TO_INT32,
		INT16_TO_INT24,
		INT16_TO_INT32,
		INT24_TO_INT32,
	};

	int			m_volume;
	int			m_out_bits;
	Conversion	m_conversion;
	bool		m_is_float;
};
