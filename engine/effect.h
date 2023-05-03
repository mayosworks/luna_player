//=============================================================================
// エフェクト処理
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

// 前方宣言
interface IMediaObject;
interface IMediaObjectInPlace;
interface IDirectSoundFXCompressor8;
interface IDirectSoundFXWavesReverb8;

//-----------------------------------------------------------------------------
// エフェクト
//-----------------------------------------------------------------------------
class Effect
{
public:
	Effect();
	~Effect();

	// バッファサイズは、エフェクトを施す際の最大サイズを指定のこと
	bool Initialize(UINT fx_mode, const WAVEFORMATEX& wfx, UINT buf_size);
	void Finalize();

	// エフェクト処理をかける、残りデータを取得する際は、書込みだけします。
	UINT Process(void* data, UINT size, UINT fx_mode);

	// まだ、データが残っているか？（ディレイ等）
	bool HasMoreData() const;

private:
	bool InitWavesReverb();
	bool InitCompressor();

	UINT WavesReverb(void* data, UINT size);
	UINT Compressor(void* data, UINT size);
	UINT VocalCancel(void* data, UINT size);
	UINT VocalBoost(void* data, UINT size);

private:
	// エフェクトモード
	UINT	m_fx_mode;

	// DMO/リバーブエフェクトオブジェクト関係
	IDirectSoundFXWavesReverb8*	m_reverb;
	IMediaObject*				m_object1;
	IMediaObjectInPlace*		m_in_place1;

	// DMO/コンプレッションエフェクトオブジェクト関係
	IDirectSoundFXCompressor8*	m_compress;
	IMediaObject*				m_object2;
	IMediaObjectInPlace*		m_in_place2;

	// 変換用Floatバッファ
	void*	m_buffer;

	// バッファ等のパラメーター
	UINT	m_buf_size;
	UINT	m_format;	// Int,Float,etc.
	UINT	m_channels;	// チャネル数
	UINT	m_samples;	// サンプル数
	UINT	m_bits;		// ビット数
	bool	m_has_more;	// まだ残りがあるか？
};

