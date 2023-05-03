//=============================================================================
// Built-in Standard (Media Foundation) Plugin.
//                                                     Copyright (c) 2011 MAYO.
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#undef WINVER
#define WINVER 0x0601

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <propsys.h>
#include <propkey.h>
#include <algorithm>
#include "luna_pi.h"
#include "builtin_plugin.h"

//-----------------------------------------------------------------------------
// API定義
//-----------------------------------------------------------------------------

#undef RtlZeroMemory
#undef RtlMoveMemory

#undef ZeroMemory
#undef MoveMemory
#undef CopyMemory

extern "C" NTSYSAPI void NTAPI RtlZeroMemory(void* dest, size_t length);
extern "C" NTSYSAPI void NTAPI RtlMoveMemory(void* dest, const void* src, size_t length);

#define ZeroMemory RtlZeroMemory
#define MoveMemory RtlMoveMemory
#define CopyMemory RtlMoveMemory

#define SAFE_RELEASE(obj)	if (obj) {obj->Release(); obj = NULL;}

// MediaFoundationAPI定義
typedef HRESULT (STDAPICALLTYPE* declMFStartup)(ULONG Version, DWORD dwFlags);
typedef HRESULT (STDAPICALLTYPE* declMFShutdown)();
typedef HRESULT (STDAPICALLTYPE* declMFCreateMediaType)(IMFMediaType** ppMFType);
typedef HRESULT (STDAPICALLTYPE* declMFCreateSourceResolver)(IMFSourceResolver** ppISourceResolver);
typedef HRESULT (STDAPICALLTYPE* declMFCreateWaveFormatExFromMFMediaType)(IMFMediaType* pMFType, WAVEFORMATEX** ppWF, UINT32* pcbSize, UINT32 Flags);
typedef HRESULT (STDAPICALLTYPE* declMFGetService)(IUnknown* punkObject, REFGUID guidService, REFIID riid, LPVOID* ppvObject);
typedef HRESULT (STDAPICALLTYPE* declMFCreateSourceReaderFromURL)(LPCWSTR pwszURL, IMFAttributes *pAttributes, IMFSourceReader** ppSourceReader);

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------
namespace {

// ストリームインデックス定数
const DWORD STREAM_INDEX = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

// 再生時コンテキスト
struct Context
{
	IMFSourceReader*	reader;
	IMFSample*			sample;
	IMFMediaBuffer*		buffer;
	UINT32				used_len;
	UINT32				out_bits;
	bool				is_float;
	bool				is_eos;
};

HMODULE	s_hmf = NULL;
HMODULE	s_hmfplat = NULL;
HMODULE	s_hmfread = NULL;

declMFStartup fpMFStartup = NULL;
declMFShutdown fpMFShutdown = NULL;
declMFCreateMediaType fpMFCreateMediaType = NULL;
declMFCreateSourceResolver fpMFCreateSourceResolver = NULL;
declMFCreateWaveFormatExFromMFMediaType fpMFCreateWaveFormatExFromMFMediaType = NULL;
declMFGetService fpMFGetService = NULL;
declMFCreateSourceReaderFromURL fpMFCreateSourceReaderFromURL = NULL;

} //namespace


//-----------------------------------------------------------------------------
// intに入れたfloatをInt24に変換する
//-----------------------------------------------------------------------------
static int Unpack24(int x)
{
	const int INT24_MAX_VALUE = (1 << 23) - 1;

	int e = (x >> 23) & 0xFF;
	e = (127 < e)? 127 : ((e < 104)? 104 : e);

	int value = ((x & 0x7FFFFF) + (1 << 23)) >> (127 - e);
	value = (INT24_MAX_VALUE < value)? INT24_MAX_VALUE : value;

	return (x < 0)? -value : value;
}

//-----------------------------------------------------------------------------
// 64bit/32bit -> 32bit への割り算,asm版
//-----------------------------------------------------------------------------
static UINT32 Div6432To32(UINT64 dividend, UINT32 divisor)
{
#if defined(_WIN32) && !defined(_WIN64)
	UINT32 remainder = 0;
	UINT32* rem = &remainder;

	// 64bit を上位・下位32bitに分割
	UINT32 low_part  = static_cast<UINT32> (0x00000000FFFFFFFF & dividend);
	UINT32 high_part = static_cast<UINT32>((0xFFFFFFFF00000000 & dividend) >> 32);
	UINT32 result = 0;

	// 割り算実施
	_asm
	{
		mov eax, low_part
		mov edx, high_part
		mov ecx, rem
		div divisor
		or  ecx, ecx
		jz  short label
		mov [ecx], edx
	label:
		mov result, eax
	}

	return result;
#else //defined(_WIN32) && !defined(_WIN64)
	return static_cast<UINT32>(dividend / static_cast<UINT64>(divisor));
#endif //defined(_WIN32) && !defined(_WIN64)
}

//-----------------------------------------------------------------------------
// 32bitx32bit -> 64bit への掛け算,asm版
//-----------------------------------------------------------------------------
static UINT64 Mul3232To64(UINT32 x, UINT32 y)
{
#if defined(_WIN32) && !defined(_WIN64)
	// 64bit を上位・下位32bitに分割
	UINT32 low_part  = 0;
	UINT32 high_part = 0;

	// 掛け算実施
	_asm
	{
		mov eax,x
		mul y
		mov low_part, eax
		mov high_part, edx
	}

	UINT64 result = static_cast<UINT64>(high_part) << 32 | static_cast<UINT64>(low_part);
	return result;
#else //defined(_WIN32) && !defined(_WIN64)
	return static_cast<UINT64>(x) * static_cast<UINT64>(y);
#endif //defined(_WIN32) && !defined(_WIN64)
}

//-----------------------------------------------------------------------------
// Setup Audio stream and Get LPCM Format.
//-----------------------------------------------------------------------------
static bool ConfigureAudioStream(IMFSourceReader* reader, UINT32& rate, UINT32& bits, UINT32& ch, bool& is_float)
{
	HRESULT hr = S_OK;

	IMFMediaType* audio_type = NULL;
	hr = fpMFCreateMediaType(&audio_type);
	if (FAILED(hr)) {
		return false;
	}

	hr = audio_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	if (FAILED(hr)) {
		audio_type->Release();
		return false;
	}

	if (is_float) {
		// 後で必要なビット数へ変換するため、Floatでデコードする。
		hr = audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
		if (FAILED(hr)) {
			audio_type->Release();
			return false;
		}
	}

#ifdef CHANNLES_OVERWRITE
	// チャンネル数は、ステレオ固定。
	if (ch) {
		hr = audio_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, ch);
		if (FAILED(hr)) {
			audio_type->Release();
			return false;
		}
	}
#endif //CHANNLES_OVERWRITE

	hr = reader->SetCurrentMediaType(STREAM_INDEX, NULL, audio_type);
	if (FAILED(hr)) {
		if (is_float) {
			// Floatで失敗したら、PCMにしてみる。
			hr = audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
			if (FAILED(hr)) {
				audio_type->Release();
				return false;
			}

			hr = reader->SetCurrentMediaType(STREAM_INDEX, NULL, audio_type);
			if (FAILED(hr)) {
				audio_type->Release();
				return false;
			}

			is_float = false;
		}
		else {
			audio_type->Release();
			return false;
		}
	}

	audio_type->Release();

	hr = reader->SetStreamSelection(STREAM_INDEX, TRUE);
	if (FAILED(hr)) {
		return false;
	}

	IMFMediaType* media_type = NULL;
	hr = reader->GetCurrentMediaType(STREAM_INDEX, &media_type);
	if (FAILED(hr)) {
		return false;
	}

	WAVEFORMATEX* wfx = NULL;
	UINT32 wfx_size = 0;

	hr = fpMFCreateWaveFormatExFromMFMediaType(media_type, &wfx, &wfx_size, MFWaveFormatExConvertFlag_Normal);
	media_type->Release();

	if (FAILED(hr)) {
		return false;
	}

	rate = static_cast<UINT32>(wfx->nSamplesPerSec);
	bits = static_cast<UINT32>(wfx->wBitsPerSample);
	ch   = static_cast<UINT32>(wfx->nChannels);

	bool is_valid = false;
	if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE* wfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx);
		if (InlineIsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
			is_float = false;
			is_valid = true;
		}
		else if (InlineIsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
			is_float = true;
			is_valid = true;
		}
	}
	else {
		is_float = (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
		is_valid = (wfx->wFormatTag == WAVE_FORMAT_PCM || wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
	}

	CoTaskMemFree(wfx);
	return is_valid;
}

//-----------------------------------------------------------------------------
// Get IMFSample & IMFMediaBuffer from Reader.
//-----------------------------------------------------------------------------
static bool GetSampleBuffer(IMFSourceReader* reader,
	IMFSample** out_sample, IMFMediaBuffer** out_buffer, bool& is_eos)
{
	HRESULT hr = S_OK; 
	DWORD flags = 0;

	IMFSample* sample = NULL;
	IMFMediaBuffer* buffer = NULL;

	hr = reader->ReadSample(STREAM_INDEX, 0, NULL, &flags, NULL, &sample);
	if (FAILED(hr) || !sample) {
		return false;
	}

	// フォーマット変更には対応しない。
	if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
		sample->Release();
		return false;
	}

	hr = sample->ConvertToContiguousBuffer(&buffer);
	if (FAILED(hr) || !buffer) {
		sample->Release();
		return false;
	}

	if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
		is_eos = true;
	}

	*out_sample = sample;
	*out_buffer = buffer;
	return true;
}

//-----------------------------------------------------------------------------
// MediaSourceを生成する
//-----------------------------------------------------------------------------
static HRESULT CreateMediaSource(const wchar_t* source_url, IMFMediaSource** media_source)
{
    if (!source_url) {
        return E_INVALIDARG;
    }

    if (!media_source) {
        return E_POINTER;
    }

    HRESULT hr = S_OK;
    
    MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;
    IMFSourceResolver* resolver = NULL;
    IUnknown* unk_source = NULL;

    // Create the source resolver.
    hr = fpMFCreateSourceResolver(&resolver);

    // Use the source resolver to create the media source.
    if (SUCCEEDED(hr)) {
        hr = resolver->CreateObjectFromURL(source_url, MF_RESOLUTION_MEDIASOURCE, NULL, &object_type, &unk_source);
    }

    // Get the IMFMediaSource from the IUnknown pointer.
    if (SUCCEEDED(hr)) {
        hr = unk_source->QueryInterface(IID_PPV_ARGS(media_source));
    }

	SAFE_RELEASE(resolver);
	SAFE_RELEASE(unk_source);

	return hr;
}

//-----------------------------------------------------------------------------
// 解放
//-----------------------------------------------------------------------------
static void LPAPI Release()
{
	fpMFShutdown();

	FreeLibrary(s_hmfread);
	s_hmfread = NULL;

	FreeLibrary(s_hmfplat);
	s_hmfplat = NULL;

	FreeLibrary(s_hmf);
	s_hmf = NULL;
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	HRESULT hr = S_OK;

	IMFMediaSource* source = NULL;
	hr = CreateMediaSource(path, &source);
	if (FAILED(hr)) {
		return false;
	}

	IMFPresentationDescriptor* pdesc = NULL;
	hr = source->CreatePresentationDescriptor(&pdesc);
	if (FAILED(hr)) {
		source->Release();
		return false;
	}

	UINT64 duration = 0;
	hr = pdesc->GetUINT64(MF_PD_DURATION, &duration);
	if (FAILED(hr)) {
		pdesc->Release();
		source->Release();
		return false;
	}

	// 最低限、時間がとれること。
	meta->duration = Div6432To32(duration, 10000);
	meta->seekable = true;

	pdesc->Release();

	IPropertyStore* props = NULL;
	hr = fpMFGetService(source, MF_PROPERTY_HANDLER_SERVICE, IID_PPV_ARGS(&props));
	if (SUCCEEDED(hr)) {
		PROPVARIANT var;

		hr = props->GetValue(PKEY_Title, &var);
		if (SUCCEEDED(hr)) {
			lstrcpyn(meta->title, var.pwszVal, META_MAXLEN);
			PropVariantClear(&var);
		}

		hr = props->GetValue(PKEY_Music_Artist, &var);
		if (SUCCEEDED(hr)) {
			if (var.vt & VT_VECTOR) {
				int used = 0;
				for (ULONG i = 0; ((i < var.calpwstr.cElems) && (used < META_MAXLEN)); ++i) {
					lstrcpyn(&meta->artist[used], var.calpwstr.pElems[i], META_MAXLEN - used);
					used += lstrlen(var.calpwstr.pElems[i]);

					if (((i + 1) < var.calpwstr.cElems) && (used < META_MAXLEN)) {
						meta->artist[used++] = L',';
					}
				}

				meta->artist[META_MAXLEN - 1] = L'\0';
			}
			else {
				lstrcpyn(meta->artist, var.pwszVal, META_MAXLEN);
			}

			PropVariantClear(&var);
		}

		hr = props->GetValue(PKEY_Music_AlbumTitle, &var);
		if (SUCCEEDED(hr)) {
			lstrcpyn(meta->album, var.pwszVal, META_MAXLEN);
			PropVariantClear(&var);
		}

#if defined(MFND_SUPPORT_ARTWORK)
		// アートワーク画像
		// PKEY_Thumbnailだと別の情報が取れる模様
		hr = props->GetValue(PKEY_ThumbnailStream, &var);
		if (SUCCEEDED(hr)) {
			//VT_STREAM -> var.pStream
			if (var.pStream) {
				LARGE_INTEGER zero_pos = {0};
				ULARGE_INTEGER tail_pos = {0};

				HRESULT hr1 = var.pStream->Seek(zero_pos, STREAM_SEEK_END, &tail_pos);
				HRESULT hr2 = var.pStream->Seek(zero_pos, STREAM_SEEK_SET, NULL);
				if (SUCCEEDED(hr1) && SUCCEEDED(hr2)) {
					ULONG length = static_cast<ULONG>(tail_pos.QuadPart);
					ULONG readed = 0;

					HANDLE artwork = GlobalAlloc(GHND, length);
					if (artwork) {
						hr = E_FAIL;

						void* buffer = GlobalLock(artwork);
						if (buffer) {
							hr = var.pStream->Read(buffer, length, &readed);
							GlobalUnlock(artwork);
						}

						if (FAILED(hr) || (length != readed)) {
							GlobalFree(artwork);
							artwork = NULL;
						}
					}

					meta->artwork = artwork;
				}
			}

			PropVariantClear(&var);
		}
#endif //defined(MFND_SUPPORT_ARTWORK)

		props->Release();
	}

	source->Release();
	return true;
}

//-----------------------------------------------------------------------------
// 再生するデータを開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	HRESULT hr = S_OK;

	IMFSourceReader* reader = NULL;
	hr = fpMFCreateSourceReaderFromURL(path, NULL, &reader);
	if (FAILED(hr)) {
		return NULL;
	}

	UINT32 rate = out->sample_rate;
	UINT32 bits = 24;//(out->sample_bits == 24)? 24 : 16;
	UINT32 ch   = 2;//out->num_channels;
	bool is_float = true;

	if (!ConfigureAudioStream(reader, rate, bits, ch, is_float)) {
		reader->Release();
		return NULL;
	}

	if (is_float) {
		bits = 16;//(out.sampleSize == 24)? 24 : 16;
	}

	out->sample_rate  = rate;
	out->sample_bits  = bits;
	out->num_channels = ch;

	Context* cxt = static_cast<Context*>(HeapAlloc(GetProcessHeap(), 0, sizeof(Context)));
	if (!cxt) {
		reader->Release();
		return NULL;
	}

	cxt->reader = reader;
	cxt->sample = NULL;
	cxt->buffer = NULL;
	cxt->used_len = 0;
	cxt->out_bits = bits;
	cxt->is_float = is_float;
	cxt->is_eos = false;

	return cxt;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return;
	}

	SAFE_RELEASE(cxt->buffer);
	SAFE_RELEASE(cxt->sample);
	SAFE_RELEASE(cxt->reader);

	HeapFree(GetProcessHeap(), 0, cxt);
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
static int LPAPI Render(Handle handle, void* buffer, int length)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return 0;
	}

	BYTE* copy_to_buf = static_cast<BYTE*>(buffer);
	int fill_len = 0;

	int mul = cxt->out_bits / 8;
	while (fill_len < length) {
		// MFMedaiBufferを取得。
		if (!cxt->sample && !cxt->is_eos) {
			if (!GetSampleBuffer(cxt->reader, &cxt->sample, &cxt->buffer, cxt->is_eos)) {
				return fill_len;
			}

			cxt->used_len = 0;
		}

		// もうない(=EOS)なら、何もせず。
		if (!cxt->buffer) {
			return fill_len;
		}

		BYTE* audio_data = NULL;
		DWORD audio_size = 0;

		HRESULT hr = cxt->buffer->Lock(&audio_data, NULL, &audio_size);
		if (FAILED(hr)) {
			return fill_len;
		}

		audio_data += cxt->used_len;
		audio_size -= cxt->used_len;

		UINT32 need_size = length - fill_len;
		UINT32 copy_size = 0;
		UINT32 int_audio_size = audio_size;

		if (cxt->is_float) {
			int_audio_size = audio_size / 4 * mul;
			copy_size = std::min<UINT32>(need_size, int_audio_size);

			union IntByte4
			{
				INT32	i;
				BYTE	b[4];
			};

			UINT32 samples = copy_size / mul;
			IntByte4 ib;
			int* sample_data = reinterpret_cast<int*>(audio_data);
			for (UINT32 i = 0; i < samples; ++i) {
				ib.i = Unpack24(sample_data[i]);
				if (cxt->out_bits == 16) {
					*copy_to_buf++ = ib.b[1];
					*copy_to_buf++ = ib.b[2];
				}
				else {
					*copy_to_buf++ = ib.b[0];
					*copy_to_buf++ = ib.b[1];
					*copy_to_buf++ = ib.b[2];
				}
			}
		}
		else {
			copy_size = std::min<UINT32>(need_size, audio_size);
			CopyMemory(copy_to_buf, audio_data, copy_size);
		}

		cxt->buffer->Unlock();

		fill_len += copy_size;
		cxt->used_len += (cxt->is_float? (copy_size / mul * 4) : copy_size);

		// 使い切ったら、解放。
		if (copy_size == int_audio_size) {
			SAFE_RELEASE(cxt->buffer);
			SAFE_RELEASE(cxt->sample);
			cxt->used_len = 0;
		}
	}

	return fill_len;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return 0;
	}

	SAFE_RELEASE(cxt->buffer);
	SAFE_RELEASE(cxt->sample);
	cxt->used_len = 0;

	PROPVARIANT seek_pos;

	seek_pos.vt = VT_I8;
	seek_pos.hVal.QuadPart = Mul3232To64(time_ms, 10000);

	HRESULT hr = cxt->reader->SetCurrentPosition(GUID_NULL, seek_pos);
	return SUCCEEDED(hr)? time_ms : 0;
}

//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
//LPEXPORT LunaPlugin* GetLunaPlugin(HINSTANCE /*instance*/)
LunaPlugin* GetBuiltinPlugin(HINSTANCE /*instance*/)
{
	// クリティカルエラーなどのメッセージを非表示化
	UINT err_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	s_hmf = LoadLibrary(TEXT("mf.dll"));
	s_hmfplat = LoadLibrary(TEXT("mfplat.dll"));
	s_hmfread = LoadLibrary(TEXT("mfreadwrite.dll"));

	// エラーモードを戻す
	SetErrorMode(err_mode);

	if (!s_hmf || !s_hmfplat || !s_hmfread) {
		return NULL;
	}

	fpMFStartup = (declMFStartup)GetProcAddress(s_hmfplat, "MFStartup");
	fpMFShutdown = (declMFShutdown)GetProcAddress(s_hmfplat, "MFShutdown");
	fpMFCreateMediaType = (declMFCreateMediaType)GetProcAddress(s_hmfplat, "MFCreateMediaType");
	fpMFCreateSourceResolver = (declMFCreateSourceResolver)GetProcAddress(s_hmfplat, "MFCreateSourceResolver");
	fpMFCreateWaveFormatExFromMFMediaType = (declMFCreateWaveFormatExFromMFMediaType)GetProcAddress(s_hmfplat, "MFCreateWaveFormatExFromMFMediaType");
	fpMFGetService = (declMFGetService)GetProcAddress(s_hmf, "MFGetService");
	fpMFCreateSourceReaderFromURL = (declMFCreateSourceReaderFromURL)GetProcAddress(s_hmfread, "MFCreateSourceReaderFromURL");
	if (!fpMFStartup || !fpMFShutdown || !fpMFCreateMediaType || !fpMFCreateSourceResolver || !fpMFCreateWaveFormatExFromMFMediaType || !fpMFGetService || !fpMFCreateSourceReaderFromURL) {
		return NULL;
	}

	static LunaPlugin plugin;

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = L"Built-in plugin v1.00";
	plugin.support_type = L"*.wav;*.wma;*.mp3;*.m4a;";

	plugin.Release	= Release;
	plugin.Property	= NULL;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	HRESULT hr = fpMFStartup(MF_VERSION, MFSTARTUP_LITE);
	if (FAILED(hr)) {
		return NULL;
	}

	return &plugin;
}
