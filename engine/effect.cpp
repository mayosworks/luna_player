//=============================================================================
// エフェクト処理
//=============================================================================

#define INITGUID
#define WIN32_LEAN_AND_MEAN
#define STRSAFE_NO_DEPRECATE
#define ENABLE_DMO_EFFECT
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <initguid.h>
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#ifdef ENABLE_DMO_EFFECT
#include <dsound.h>
#endif //ENABLE_DMO_EFFECT
#include <dshow.h>
#include <dmo.h>
#include "lib/wx_misc.h"
#include "config.h"
#include "effect.h"
#include "driver_utility.h"

#pragma comment(lib, "dmoguids.lib")

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Effect::Effect()
	: m_fx_mode(FX_NONE)
	, m_reverb(NULL)
	, m_object1(NULL)
	, m_in_place1(NULL)
	, m_compress(NULL)
	, m_object2(NULL)
	, m_in_place2(NULL)
	, m_buffer(NULL)
	, m_format(WAVE_FORMAT_PCM)
	, m_channels(0)
	, m_samples(0)
	, m_bits(0)
	, m_has_more(false)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Effect::~Effect()
{
}

//-----------------------------------------------------------------------------
// エフェクト初期化
//-----------------------------------------------------------------------------
bool Effect::Initialize(UINT fx_mode, const WAVEFORMATEX& wfx, UINT buf_size)
{
	// 形式判定
	if (wfx.wFormatTag != WAVE_FORMAT_PCM && wfx.wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
		return false;
	}

	// 16bit～32bitのみ対応。
	if (wfx.wBitsPerSample < 16 || wfx.wBitsPerSample > 32) {
		return false;
	}

	// 変換用にFloatバッファを生成する
	if (wfx.wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
		size_t buffer_size = buf_size / (wfx.wBitsPerSample / 8) * sizeof(float);
		m_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_size);
		if (!m_buffer) {
			return false;
		}
	}

	m_fx_mode	= fx_mode;
	m_format	= wfx.wFormatTag;
	m_buf_size	= buf_size;
	m_channels	= wfx.nChannels;
	m_samples	= wfx.nSamplesPerSec;
	m_bits		= wfx.wBitsPerSample;
	m_has_more	= false;

	if (m_fx_mode & FX_WAVES_REVERB) {
#ifdef ENABLE_DMO_EFFECT
		HRESULT hr = S_OK;

		IDirectSoundFXWavesReverb8* reverb = NULL;
		hr = CoCreateInstance(GUID_DSFX_WAVES_REVERB, NULL,
			CLSCTX_INPROC, IID_IDirectSoundFXWavesReverb8, (void**)&reverb);
		if (FAILED(hr)) {
			WX_TRACE("IDirectSoundFXWavesReverb8 Create failed! hr = %x\n", hr);
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		IMediaObject* object = NULL;
		hr = reverb->QueryInterface(IID_IMediaObject, (void**)&object); 
		if (FAILED(hr)) {
			WX_TRACE("QueryInterface/IMediaObject failed! hr = %x\n", hr);
			reverb->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		IMediaObjectInPlace* in_place = NULL;
		hr = reverb->QueryInterface(IID_IMediaObjectInPlace, (void**)&in_place); 
		if (FAILED(hr)) {
			WX_TRACE("QueryInterface/IMediaObjectInPlace failed! hr = %x\n", hr);
			object->Release();
			reverb->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		hr = object->AllocateStreamingResources();
		if (FAILED(hr)) {
			WX_TRACE("AllocateStreamingResources failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			reverb->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		DMO_MEDIA_TYPE mt;

		ZeroMemory(&mt, sizeof(mt));

		// Float形式にする
		WAVEFORMATEX fxfmt		= wfx;
		fxfmt.cbSize			= sizeof(fxfmt);
		fxfmt.wFormatTag		= WAVE_FORMAT_IEEE_FLOAT;
		fxfmt.wBitsPerSample	= 32;
		fxfmt.nBlockAlign		= wfx.nChannels * fxfmt.wBitsPerSample / 8;
		fxfmt.nAvgBytesPerSec	= wfx.nSamplesPerSec * fxfmt.nBlockAlign;

		mt.majortype			= MEDIATYPE_Audio;
		mt.subtype				= MEDIASUBTYPE_IEEE_FLOAT;
		mt.bFixedSizeSamples	= TRUE;
		mt.formattype			= FORMAT_WaveFormatEx;
		mt.pbFormat				= const_cast<BYTE*>(reinterpret_cast<const BYTE*>(&fxfmt));
		mt.cbFormat				= sizeof(fxfmt);

		// 変換はしないので、入出力とも同じ形式を指定する。
		hr = object->SetInputType(0, &mt, 0);
		if (FAILED(hr)) {
			WX_TRACE("SetInputType failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			reverb->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		hr = object->SetOutputType(0, &mt, 0);
		if (FAILED(hr)) {
			WX_TRACE("SetOutputType failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			reverb->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		// エフェクトパラメーター設定
		DSFXWavesReverb dswr;

		dswr.fInGain			= -3.0f;	//DSFX_WAVESREVERB_INGAIN_DEFAULT;
		dswr.fReverbMix			= -7.0f;	//DSFX_WAVESREVERB_REVERBMIX_DEFAULT;
		dswr.fReverbTime		= 500.0f;	//DSFX_WAVESREVERB_REVERBTIME_DEFAULT;
		dswr.fHighFreqRTRatio	= DSFX_WAVESREVERB_HIGHFREQRTRATIO_DEFAULT;

		reverb->SetAllParameters(&dswr);

		m_reverb  = reverb;
		m_object1  = object;
		m_in_place1 = in_place;
#endif //ENABLE_DMO_EFFECT
	}

	if (m_fx_mode & FX_COMPRESSOR) {
#ifdef ENABLE_DMO_EFFECT
		HRESULT hr = S_OK;

		IDirectSoundFXCompressor8* compress = NULL;
		hr = CoCreateInstance(GUID_DSFX_STANDARD_COMPRESSOR, NULL,
			CLSCTX_INPROC, IID_IDirectSoundFXCompressor8, (void**)&compress);
		if (FAILED(hr)) {
			WX_TRACE("IDirectSoundFXCompressor8 Create failed! hr = %x\n", hr);
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		IMediaObject* object = NULL;
		hr = compress->QueryInterface(IID_IMediaObject, (void**)&object); 
		if (FAILED(hr)) {
			WX_TRACE("QueryInterface/IMediaObject failed! hr = %x\n", hr);
			compress->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		IMediaObjectInPlace* in_place = NULL;
		hr = object->QueryInterface(IID_IMediaObjectInPlace, (void**)&in_place); 
		if (FAILED(hr)) {
			WX_TRACE("QueryInterface/IMediaObjectInPlace failed! hr = %x\n", hr);
			object->Release();
			compress->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		hr = object->AllocateStreamingResources();
		if (FAILED(hr)) {
			WX_TRACE("AllocateStreamingResources failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			compress->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		DMO_MEDIA_TYPE mt;

		ZeroMemory(&mt, sizeof(mt));

		// Float形式にする
		WAVEFORMATEX fxfmt		= wfx;
		fxfmt.cbSize			= sizeof(fxfmt);
		fxfmt.wFormatTag		= WAVE_FORMAT_IEEE_FLOAT;
		fxfmt.wBitsPerSample	= 32;
		fxfmt.nBlockAlign		= wfx.nChannels * fxfmt.wBitsPerSample / 8;
		fxfmt.nAvgBytesPerSec	= wfx.nSamplesPerSec * fxfmt.nBlockAlign;

		mt.majortype			= MEDIATYPE_Audio;
		mt.subtype				= MEDIASUBTYPE_IEEE_FLOAT;
		mt.bFixedSizeSamples	= TRUE;
		mt.formattype			= FORMAT_WaveFormatEx;
		mt.pbFormat				= const_cast<BYTE*>(reinterpret_cast<const BYTE*>(&fxfmt));
		mt.cbFormat				= sizeof(fxfmt);

		// 変換はしないので、入出力とも同じ形式を指定する。
		hr = object->SetInputType(0, &mt, 0);
		if (FAILED(hr)) {
			WX_TRACE("SetInputType failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			compress->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		hr = object->SetOutputType(0, &mt, 0);
		if (FAILED(hr)) {
			WX_TRACE("SetOutputType failed! hr = %x\n", hr);
			in_place->Release();
			object->Release();
			compress->Release();
			if (m_buffer) {
				HeapFree(GetProcessHeap(), 0, m_buffer);
				m_buffer = NULL;
			}
			return false;
		}

		// エフェクトパラメーター設定
		DSFXCompressor dscmp;

		dscmp.fGain			= -3.0f;
		dscmp.fAttack		= 20.0f;
		dscmp.fRelease		= 200.0f;
		dscmp.fThreshold	= -20.0f;
		dscmp.fRatio		= 3.0f;
		dscmp.fPredelay		= 4.0f;

		compress->SetAllParameters(&dscmp);

		m_compress = compress;
		m_object2  = object;
		m_in_place2 = in_place;
#endif //ENABLE_DMO_EFFECT
	}

	return true;
}

//-----------------------------------------------------------------------------
// エフェクト終了
//-----------------------------------------------------------------------------
void Effect::Finalize()
{
#ifdef ENABLE_DMO_EFFECT
	WX_SAFE_RELEASE(m_in_place2);
	if (m_in_place2) {
		m_in_place2->Release();
		m_in_place2 = NULL;
	}

	if (m_object2) {
		m_object2->FreeStreamingResources();

		m_object2->Release();
		m_compress->Release();

		m_in_place2 = NULL;
		m_object2  = NULL;
		m_compress  = NULL;
	}

	WX_SAFE_RELEASE(m_in_place1);
	if (m_in_place1) {
		m_in_place1->Release();
		m_in_place1 = NULL;
	}

	if (m_object1) {
		m_object1->FreeStreamingResources();

		m_object1->Release();
		m_reverb->Release();

		m_in_place1 = NULL;
		m_object1  = NULL;
		m_reverb  = NULL;
	}
#endif //ENABLE_DMO_EFFECT

	if (m_buffer) {
		HeapFree(GetProcessHeap(), 0, m_buffer);
		m_buffer = NULL;
	}

	m_fx_mode	= FX_NONE;
	m_format	= 0;
	m_buf_size	= 0;
	m_channels	= 0;
	m_samples	= 0;
	m_bits		= 0;
	m_has_more	= false;
}

//-----------------------------------------------------------------------------
// エフェクト処理をかける、残りデータを取得する際は、書込みだけします。
//-----------------------------------------------------------------------------
UINT Effect::Process(void* data, UINT size, UINT fx_mode)
{
	// 処理は、下記の優先順で行う

	if (fx_mode & FX_VOCAL_CANCEL) {
		size = VocalCancel(data, size);
	}

	if (fx_mode & FX_VOCAL_BOOST) {
		size = VocalBoost(data, size);
	}

	if (fx_mode & FX_WAVES_REVERB) {
		size = WavesReverb(data, size);
	}

	if (fx_mode & FX_COMPRESSOR) {
		size = Compressor(data, size);
	}

	return size;
}

//-----------------------------------------------------------------------------
// まだ、データが残っているか？（ディレイ等）
//-----------------------------------------------------------------------------
bool Effect::HasMoreData() const
{
	return m_has_more;
}

//-----------------------------------------------------------------------------
// WavesReverb処理
//-----------------------------------------------------------------------------
UINT Effect::WavesReverb(void* data, UINT size)
{
	HRESULT hr = S_OK;
	if (m_in_place1) {
		const float DIV_VALUE_16 = 32767.0f * 1.2f;
		const float MUL_VALUE_16 = 32767.0f;

		const float DIV_VALUE_32 = 2147483647.0f * 1.2f;
		const float MUL_VALUE_32 = 2147483647.0f;

		float* buffer = static_cast<float*>(m_buffer);
		UINT process_size = size;

		if (m_bits == 16) {
			const short* source = static_cast<short*>(data);
			const UINT loop = (size / 2);
			for (UINT i = 0; i < loop; ++i) {
				buffer[i] = static_cast<float>(source[i]) / DIV_VALUE_16;
			}
			process_size = size * 2;
		}
		else if (m_bits == 24) {
			const BYTE* source = static_cast<BYTE*>(data);
			const UINT loop = (size / 3);
			for (UINT i = 0; i < loop; ++i) {
				int value = GetInt24Value(static_cast<const void*>(source + i * 3));
				buffer[i] = static_cast<float>(value) / DIV_VALUE_32;
			}
			process_size = size / 3 * 4;
		}
		else if (m_bits == 32) {
			const int* source = static_cast<int*>(data);
			const UINT loop = (size / 4);
			for (UINT i = 0; i < loop; ++i) {
				buffer[i] = static_cast<float>(source[i]) / DIV_VALUE_32;
			}
		}

		hr = m_in_place1->Process(process_size, static_cast<BYTE*>(m_buffer), 0, DMO_INPLACE_NORMAL);

		// Clamp
		for (UINT i = 0; i < process_size / 4; ++i) {
			buffer[i] = wx::Clamp(buffer[i], -1.0f, 1.0f);
		}

		if (m_bits == 16) {
			short* source = static_cast<short*>(data);
			const UINT loop = (size / 2);
			for (UINT i = 0; i < loop; ++i) {
				source[i] = static_cast<short>(buffer[i] * MUL_VALUE_16);
			}
		}
		else if (m_bits == 24) {
			BYTE* source = static_cast<BYTE*>(data);
			const UINT loop = (size / 3);
			for (UINT i = 0; i < loop; ++i) {
				int value = static_cast<int>(buffer[i] * MUL_VALUE_32);
				SetInt24Value(static_cast<void*>(source + i * 3), value);
			}
		}
		else if (m_bits == 32) {
			int* source = static_cast<int*>(data);
			const UINT loop = (size / 4);
			for (UINT i = 0; i < loop; ++i) {
				source[i] = static_cast<int>(buffer[i] * MUL_VALUE_32);
			}
		}
	}

	return size;
}

//-----------------------------------------------------------------------------
// コンプレション処理
//-----------------------------------------------------------------------------
UINT Effect::Compressor(void* data, UINT size)
{
	HRESULT hr = S_OK;
	if (m_in_place2) {
		const float DIV_VALUE_16 = 32767.0f * 1.2f;
		const float MUL_VALUE_16 = 32767.0f;

		const float DIV_VALUE_32 = 2147483647.0f * 1.2f;
		const float MUL_VALUE_32 = 2147483647.0f;

		float* buffer = static_cast<float*>(m_buffer);
		UINT process_size = size;

		if (m_bits == 16) {
			const short* source = static_cast<short*>(data);
			const UINT loop = (size / 2);
			for (UINT i = 0; i < loop; ++i) {
				buffer[i] = static_cast<float>(source[i]) / DIV_VALUE_16;
			}
			process_size = size * 2;
		}
		else if (m_bits == 24) {
			const BYTE* source = static_cast<BYTE*>(data);
			const UINT loop = (size / 3);
			for (UINT i = 0; i < loop; ++i) {
				int value = GetInt24Value(static_cast<const void*>(source + i * 3));
				buffer[i] = static_cast<float>(value) / DIV_VALUE_32;
			}
			process_size = size / 3 * 4;
		}
		else if (m_bits == 32) {
			const int* source = static_cast<int*>(data);
			const UINT loop = (size / 4);
			for (UINT i = 0; i < loop; ++i) {
				buffer[i] = static_cast<float>(source[i]) / DIV_VALUE_32;
			}
		}

		hr = m_in_place2->Process(process_size, static_cast<BYTE*>(m_buffer), 0, DMO_INPLACE_NORMAL);

		// Clamp
		for (UINT i = 0; i < process_size / 4; ++i) {
			buffer[i] = wx::Clamp(buffer[i], -1.0f, 1.0f);
		}

		if (m_bits == 16) {
			short* source = static_cast<short*>(data);
			const UINT loop = (size / 2);
			for (UINT i = 0; i < loop; ++i) {
				source[i] = static_cast<short>(buffer[i] * MUL_VALUE_16);
			}
		}
		else if (m_bits == 24) {
			BYTE* source = static_cast<BYTE*>(data);
			const UINT loop = (size / 3);
			for (UINT i = 0; i < loop; ++i) {
				int value = static_cast<int>(buffer[i] * MUL_VALUE_32);
				SetInt24Value(static_cast<void*>(source + i * 3), value);
			}
		}
		else if (m_bits == 32) {
			int* source = static_cast<int*>(data);
			const UINT loop = (size / 4);
			for (UINT i = 0; i < loop; ++i) {
				source[i] = static_cast<int>(buffer[i] * MUL_VALUE_32);
			}
		}
	}

	return size;
}

//-----------------------------------------------------------------------------
// ボーカル消し処理
//-----------------------------------------------------------------------------
UINT Effect::VocalCancel(void* data, UINT size)
{
	if (m_format == WAVE_FORMAT_PCM && m_channels == 2) {
		if (m_bits == 32) {
			int* ptr = static_cast<int*>(data);
			for (UINT i = 0; i < size / 4; i += 2) {
				int mix = (ptr[i+1] - ptr[i]);

				ptr[i  ] = mix;
				ptr[i+1] = mix;
			}
		}
		else if (m_bits == 24) {
			BYTE* ptr = static_cast<BYTE*>(data);
			for (UINT i = 0; i < size; i += 6) {
				int value0 = GetInt24Value(ptr + i + 0) / 2;
				int value1 = GetInt24Value(ptr + i + 3) / 2;

				int mix = value1 - value0;

				SetInt24Value(ptr + i + 0, mix);
				SetInt24Value(ptr + i + 3, mix);
			}
		}
		else if (m_bits == 16) {
			short* ptr = static_cast<short*>(data);
			for (UINT i = 0; i < size / 2; i += 2) {
				int mix = wx::Clamp(ptr[i+1] - ptr[i], -32768, 32767);

				ptr[i  ] = static_cast<short>(mix);
				ptr[i+1] = static_cast<short>(mix);
			}
		}
	}
	else if (m_format == WAVE_FORMAT_IEEE_FLOAT && m_channels == 2) {
#if 0
		if (m_bits == 32) {
			float* ptr = static_cast<float*>(data);
			for (UINT i = 0; i < size / 4; i += 2) {
				float mix = (ptr[i+1] - ptr[i]);

				ptr[i  ] = mix;
				ptr[i+1] = mix;
			}
		}
#endif //0
	}

	return size;
}

//-----------------------------------------------------------------------------
// ボーカル強調処理
//-----------------------------------------------------------------------------
UINT Effect::VocalBoost(void* data, UINT size)
{
	if (m_format == WAVE_FORMAT_PCM && m_channels == 2) {
		if (m_bits == 32) {
			int* ptr = static_cast<int*>(data);
			for (UINT i = 0; i < size / 4; i += 2) {
				int mono = (ptr[i+1] + ptr[i]) / 4;
				int mix = (ptr[i+1] - ptr[i]) / 4;

				ptr[i  ] = ptr[i  ] / 4 + mono - mix;
				ptr[i+1] = ptr[i+1] / 4 + mono - mix;
			}
		}
		else if (m_bits == 24) {
			BYTE* ptr = static_cast<BYTE*>(data);
			for (UINT i = 0; i < size; i += 6) {
				int value0 = GetInt24Value(ptr + i + 0) / 2;
				int value1 = GetInt24Value(ptr + i + 3) / 2;

				int mono = (value1 + value0) / 4;
				int mix = (value1 - value0) / 4;

				value0 = (value0 / 4 + mono - mix);
				value1 = (value1 / 4 + mono - mix);

				SetInt24Value(ptr + i + 0, value0);
				SetInt24Value(ptr + i + 3, value1);
			}
		}
		else if (m_bits == 16) {
			short* ptr = static_cast<short*>(data);
			for (UINT i = 0; i < size / 2; i += 2) {
				int mono = (ptr[i+1] + ptr[i]) / 4;
				int mix = (ptr[i+1] - ptr[i]) / 4;

				ptr[i  ] = static_cast<short>(wx::Clamp(ptr[i  ] / 4 + mono - mix, -32768, 32767));
				ptr[i+1] = static_cast<short>(wx::Clamp(ptr[i+1] / 4 + mono - mix, -32768, 32767));
			}
		}
	}
	else if (m_format == WAVE_FORMAT_IEEE_FLOAT && m_channels == 2) {
#if 0
		if (m_bits == 32) {
			float* ptr = static_cast<float*>(data);
			for (UINT i = 0; i < size / 4; i += 2) {
				float mono = (ptr[i+1] + ptr[i]) / 4.0f;
				float mix = (ptr[i+1] - ptr[i]) / 4.0f;

				ptr[i  ] = ptr[i  ] / 4.0f + mono - mix;
				ptr[i+1] = ptr[i+1] / 4.0f + mono - mix;
			}
		}
#endif //0
	}

	return size;
}
