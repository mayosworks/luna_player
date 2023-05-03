//=============================================================================
// サウンドドライバユーティリティ
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <windows.h>
#include <setupapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include "driver_utility.h"

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
namespace {

// 24bitと32bitの変換用
union Int4Byte
{
	int		i;
	BYTE	b[4];
};

#define KSAUDIO_SPEAKER_3	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT| SPEAKER_LOW_FREQUENCY)

#define KSAUDIO_SPEAKER_5	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
							 SPEAKER_FRONT_CENTER | \
							 SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | \
							 SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)

#define KSAUDIO_SPEAKER_7	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
							 SPEAKER_FRONT_CENTER | \
							 SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | \
							 SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)

} //namespace

//-----------------------------------------------------------------------------
// void*にオフセットを加算
//-----------------------------------------------------------------------------
void* AddOffset(void* ptr, UINT offset)
{
	return (static_cast<BYTE*>(ptr) + offset);
}

//-----------------------------------------------------------------------------
// EXTENSIBLE形式にコピー
//-----------------------------------------------------------------------------
void CopyToExtensible(WAVEFORMATEXTENSIBLE& wft, const WAVEFORMATEX& wfx)
{
	wft.Format = wfx;
	wft.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wft.Format.cbSize = sizeof(wft) - sizeof(wft.Format);
	wft.Samples.wValidBitsPerSample	= wfx.wBitsPerSample;
	wft.dwChannelMask = GetChannelMask(wfx.nChannels);
	wft.SubFormat = GetSubFormatGUID(wfx.wFormatTag);
}

//-----------------------------------------------------------------------------
// サブ形式のGUID取得
//-----------------------------------------------------------------------------
GUID GetSubFormatGUID(WORD format_tag)
{
	switch (format_tag) {
	case WAVE_FORMAT_PCM:			return KSDATAFORMAT_SUBTYPE_PCM;
	case WAVE_FORMAT_IEEE_FLOAT:	return KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	}

	return GUID_NULL;
}

//-----------------------------------------------------------------------------
// チャネルマスク取得(Max.8ch)
//-----------------------------------------------------------------------------
DWORD GetChannelMask(WORD channels)
{
	switch (channels) {
	case 1: return KSAUDIO_SPEAKER_MONO;
	case 2: return KSAUDIO_SPEAKER_STEREO;

	case 3: return KSAUDIO_SPEAKER_3;
	case 4: return KSAUDIO_SPEAKER_SURROUND;
	case 5: return KSAUDIO_SPEAKER_5;
	case 6: return KSAUDIO_SPEAKER_5POINT1;
	case 7: return KSAUDIO_SPEAKER_7;
	case 8: return KSAUDIO_SPEAKER_7POINT1;

	default:
		break;
	}

	return KSAUDIO_SPEAKER_DIRECTOUT;
}

//-----------------------------------------------------------------------------
// 24bit分のPCMを32bit上位に詰めて取得
//-----------------------------------------------------------------------------
int GetInt24Value(const void* buffer)
{
	const BYTE* data = static_cast<const BYTE*>(buffer);
	Int4Byte ib;

	ib.b[0] = 0;
	ib.b[1] = data[0];
	ib.b[2] = data[1];
	ib.b[3] = data[2];

	return ib.i;
}

//-----------------------------------------------------------------------------
// 24bit分のPCMを32bit上位に詰めて取得
//-----------------------------------------------------------------------------
void SetInt24Value(void* buffer, int value)
{
	BYTE* data = static_cast<BYTE*>(buffer);
	Int4Byte ib;

	ib.i = value;

	data[0] = ib.b[1];
	data[1] = ib.b[2];
	data[2] = ib.b[3];
}
