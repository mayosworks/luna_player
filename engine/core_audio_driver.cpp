//=============================================================================
// CoreAudio排他モードドライバ
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#define CTRL_INTERNAL_VOLUME
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <windows.h>
#include <shlwapi.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys.h>
#include "driver_utility.h"
#include "core_audio_driver.h"

//-----------------------------------------------------------------------------
// 出力ビット数を更新する
//-----------------------------------------------------------------------------
static void ModifyBitsPerSample(WAVEFORMATEXTENSIBLE& wft, WORD bits)
{
	wft.Format.wBitsPerSample = bits;
	wft.Format.nBlockAlign = wft.Format.nChannels * wft.Format.wBitsPerSample / 8;
	wft.Format.nAvgBytesPerSec = wft.Format.nSamplesPerSec * wft.Format.nBlockAlign;
	wft.Samples.wValidBitsPerSample = wft.Format.wBitsPerSample;
}

//-----------------------------------------------------------------------------
// CoreAudio/DeviceAPIを使用して、デバイス名を取得
//-----------------------------------------------------------------------------
static bool GetMMDeviceName(IMMDevice* device, wchar_t* name)
{
	bool result = false;

	PROPVARIANT friendly_name;
	PropVariantInit(&friendly_name);

	IPropertyStore* property_store = NULL;
	HRESULT hr = device->OpenPropertyStore(STGM_READ, &property_store);
	if (SUCCEEDED(hr)) {
		hr = property_store->GetValue(PKEY_Device_FriendlyName, &friendly_name);
		if (SUCCEEDED(hr) && friendly_name.vt == VT_LPWSTR) {
			lstrcpyn(name, friendly_name.pwszVal, MAX_PATH);
			StrTrim(name, TEXT(" "));
			result = true;
		}
	}

	WX_SAFE_RELEASE(property_store);
	PropVariantClear(&friendly_name);
	return result;
}

//-----------------------------------------------------------------------------
// デバイス名取得補助関数
//-----------------------------------------------------------------------------
static bool GetDeviceName(IMMDeviceCollection* device_collection, UINT device_index, wchar_t* device_name, wchar_t* device_id)
{
	HRESULT hr = S_OK;

	IMMDevice* device = NULL;
	hr = device_collection->Item(device_index, &device);
	if (FAILED(hr)) {
		WX_TRACE("Unable to get device %d: %x\n", device_index, hr);
		return false;
	}

	wchar_t* device_id_buf = NULL;
	hr = device->GetId(&device_id_buf);
	if (FAILED(hr)) {
		WX_TRACE("Unable to get device %d id: %x\n", device_index, hr);
		WX_SAFE_RELEASE(device);
		return false;
	}

	lstrcpy(device_id, device_id_buf);
	CoTaskMemFree(device_id_buf);

	bool gotName = GetMMDeviceName(device, device_name);
	WX_SAFE_RELEASE(device);

	if (!gotName) {
		WX_TRACE("Unable to retrieve friendly name for device %d\n", device_index);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CoreAudioDriver::CoreAudioDriver()
	: m_device(NULL)
	, m_client(NULL)
	, m_render(NULL)
	, m_buf_size(0)
	, m_smp_size(1)
	, m_samples(0)
	, m_align(0)
	, m_bits(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CoreAudioDriver::~CoreAudioDriver()
{
}

//-----------------------------------------------------------------------------
// デバイス列挙
//-----------------------------------------------------------------------------
bool CoreAudioDriver::EnumDevices()
{
	IMMDeviceEnumerator* device_enumerator = NULL;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
		NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_enumerator));
	if (FAILED(hr)) {
		return false;
	}

	wchar_t name[MAX_PATH + 8];
	lstrcpy(name, L"Core: ");

	lstrcpyn(&name[6], L"既定のオーディオデバイス", MAX_PATH);
	AddDevice(name, L"");

	IMMDeviceCollection* device_collection = NULL;

	hr = device_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &device_collection);
	if (FAILED(hr)) {
		WX_SAFE_RELEASE(device_enumerator);
		return true;
	}

	UINT device_num = 0;
	hr = device_collection->GetCount(&device_num);
	if (FAILED(hr)) {
		WX_SAFE_RELEASE(device_collection);
		WX_SAFE_RELEASE(device_enumerator);
		return true;
	}

	wchar_t guid[MAX_PATH];
	for (UINT i = 0; i < device_num; ++i) {
		if (GetDeviceName(device_collection, i, &name[6], guid)) {
			AddDevice(name, guid);
		}
	}

	WX_SAFE_RELEASE(device_collection);
	WX_SAFE_RELEASE(device_enumerator);
	return true;
}

//-----------------------------------------------------------------------------
// デバイスを開く
//-----------------------------------------------------------------------------
bool CoreAudioDriver::Open(const wchar_t* guid, const WAVEFORMATEX& wfx)
{
	WAVEFORMATEXTENSIBLE wft;
	CopyToExtensible(wft, wfx);
	SetupVolume(wfx);

	m_conversion = PASS_THROUGH;
	m_samples = 0;

	IMMDeviceEnumerator* device_enumerator = NULL;
	HRESULT hr = S_OK;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
		NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_enumerator));
	if (FAILED(hr)) {
		WX_TRACE("[ERROR] IMMDeviceEnumeratorの生成に失敗しました！ hr=%x\n", hr);
		return false;
	}

	if (guid[0] == L'\0') {
		hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_device);
		WX_SAFE_RELEASE(device_enumerator);
	}
	else {
		hr = device_enumerator->GetDevice(guid, &m_device);
		WX_SAFE_RELEASE(device_enumerator);
	}

	if (FAILED(hr)) {
		WX_TRACE("[ERROR] デフォルトのエンドポイント取得に失敗しました！ hr=%x\n", hr);
		return false;
	}

	const UINT BUFSIZE_MS = 500;
	REFERENCE_TIME duration = BUFSIZE_MS * 10000;

	AUDCLNT_SHAREMODE mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
	DWORD flags = AUDCLNT_STREAMFLAGS_NOPERSIST;

	hr = m_device->Activate(__uuidof(IAudioClient),
		CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void**>(&m_client));
	if (FAILED(hr)) {
		WX_SAFE_RELEASE(m_device);
		WX_TRACE("[ERROR] IAudioClientのアクティブ化に失敗しました！ hr=%x\n", hr);
		return false;
	}

	// WAVEFORMATEXTENSIBLEで初期化
	hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
	if (FAILED(hr)) {
		// WAVEFORMATEXで初期化してみる
		wft.Format.wFormatTag = wfx.wFormatTag;
		wft.Format.cbSize = 0;

		if (!InitializeClient(mode, flags, duration, wft)) {
			WX_SAFE_RELEASE(m_client);
			WX_SAFE_RELEASE(m_device);
			WX_TRACE("[ERROR] IAudioClientの初期化に失敗しました！ hr=%x\n", hr);
			return false;
		}
	}

	UINT32 buf_size = 0;
	hr = m_client->GetBufferSize(&buf_size);
	if (FAILED(hr)) {
		WX_SAFE_RELEASE(m_client);
		WX_SAFE_RELEASE(m_device);
		WX_TRACE("[ERROR] バッファサイズの取得に失敗しました！ hr=%x\n", hr);
		return false;
	}

	hr = m_client->GetService(IID_PPV_ARGS(&m_render));
	if (FAILED(hr)) {
		WX_SAFE_RELEASE(m_client);
		WX_SAFE_RELEASE(m_device);
		WX_TRACE("[ERROR] IAudioRenderClientの取得に失敗しました！ hr=%x\n", hr);
		return false;
	}

	m_buf_size = buf_size;
	m_smp_size = wft.Format.nAvgBytesPerSec / wft.Format.nBlockAlign;

	m_align = wft.Format.nBlockAlign;
	m_bits = wft.Format.wBitsPerSample;

	Reset();
	return true;
}

//-----------------------------------------------------------------------------
// デバイスを閉じる
//-----------------------------------------------------------------------------
void CoreAudioDriver::Close()
{
	Stop();

	WX_SAFE_RELEASE(m_render);
	WX_SAFE_RELEASE(m_client);
	WX_SAFE_RELEASE(m_device);

	m_conversion = PASS_THROUGH;
	m_buf_size = 0;
	m_smp_size = 0;
	m_samples = 0;
	m_align = 0;
	m_bits = 0;
}

//-----------------------------------------------------------------------------
// リセット
//-----------------------------------------------------------------------------
void CoreAudioDriver::Reset()
{
	if (m_client) {
		m_client->Reset();
	}

	m_samples = 0;
}

//-----------------------------------------------------------------------------
// 出力開始
//-----------------------------------------------------------------------------
bool CoreAudioDriver::Start()
{
	if (!m_render || !m_client) {
		return false;
	}

	HRESULT hr = m_client->Start();
	if (FAILED(hr)) {
		WX_TRACE("[ERROR] 再生できません！ hr=%x\n", hr);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 出力停止
//-----------------------------------------------------------------------------
bool CoreAudioDriver::Stop()
{
	if (!m_render || !m_client) {
		return false;
	}

	HRESULT hr = m_client->Stop();
	if (FAILED(hr)) {
		WX_TRACE("[ERROR] 停止できません！ hr=%x\n", hr);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 書き込み
//-----------------------------------------------------------------------------
UINT CoreAudioDriver::Write(const void* data, UINT size)
{
	if (size == 0 || !m_render) {
		return 0;
	}

	UINT32 padding = 0;
	HRESULT hr = m_client->GetCurrentPadding(&padding);
	if (FAILED(hr)) {
		return 0;
	}

	UINT frameSize = m_buf_size / m_align;
	if (frameSize < padding) {
		return 0;
	}

	UINT conv_size = SourceToOutputSize(size);
	UINT frames = wx::Min(conv_size / m_align, frameSize - padding);
	if (frames == 0) {
		return 0;
	}

	BYTE* buffer = NULL;
	hr = m_render->GetBuffer(frames, &buffer);
	if (FAILED(hr) || !buffer) {
		return 0;
	}

	UINT copy_size = frames * m_align;
	ProcessVolume(buffer, data, copy_size);
	m_samples += frames;

	hr = m_render->ReleaseBuffer(frames, 0);
	return OutputToSourceSize(copy_size);
}

//-----------------------------------------------------------------------------
// 書き込みフラッシュ
//-----------------------------------------------------------------------------
bool CoreAudioDriver::Flush()
{
	UINT32 padding = 0;

	if (m_client) {
		HRESULT hr = m_client->GetCurrentPadding(&padding);
		if (FAILED(hr)) {
			return true;
		}
	}

	return (padding == 0);
}

//-----------------------------------------------------------------------------
// 再生時間取得
//-----------------------------------------------------------------------------
UINT CoreAudioDriver::GetPlayTime() const
{
	if (!m_render) {
		return 0;
	}

	UINT32 padding = 0;
	HRESULT hr = m_client->GetCurrentPadding(&padding);
	if (FAILED(hr)) {
		return true;
	}

	return MulDiv(m_samples - padding, 1000, m_smp_size);
}

#if !defined(CTRL_INTERNAL_VOLUME)
//-----------------------------------------------------------------------------
// ボリューム設定
//-----------------------------------------------------------------------------
void CoreAudioDriver::SetVolume(int volume)
{
	if (m_device) {
		IAudioEndpointVolume* ep_volume = NULL;
		HRESULT hr = m_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&ep_volume));
		if (SUCCEEDED(hr)) {
			ep_volume->SetMasterVolumeLevelScalar(float(volume) * 0.01f, NULL);
		}

		SAFE_RELEASE(ep_volume);
	}

	m_volume = volume;
}
#endif //!defined(CTRL_INTERNAL_VOLUME)

bool CoreAudioDriver::GetFormat(WAVEFORMATEX& wfx) const
{
	wfx.wBitsPerSample = static_cast<WORD>(m_bits);
	return true;
}

bool CoreAudioDriver::InitializeClient(AUDCLNT_SHAREMODE mode, DWORD flags, REFERENCE_TIME duration, WAVEFORMATEXTENSIBLE& wft)
{
	HRESULT hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
	if (SUCCEEDED(hr)) {
		return true;
	}

	// 8bit -> 16/24/32bit conversion.
	if (wft.Format.wBitsPerSample == 8) {
		// 8bit -> 16bit.
		ModifyBitsPerSample(wft, 16);
		hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
		if (SUCCEEDED(hr)) {
			m_conversion = INT8_TO_INT16;
			return true;
		}

		// 8bit -> 32bit.
		ModifyBitsPerSample(wft, 32);
		hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
		if (SUCCEEDED(hr)) {
			m_conversion = INT8_TO_INT32;
			return true;
		}
	}
	// 16bit -> 32bit conversion.
	else if (wft.Format.wBitsPerSample == 16) {
		// 16bit -> 32bit.
		ModifyBitsPerSample(wft, 32);
		hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
		if (SUCCEEDED(hr)) {
			m_conversion = INT16_TO_INT32;
			return true;
		}

	}
	// 24bit -> 32bit conversion.
	else if (wft.Format.wBitsPerSample == 24) {
		// 24bit -> 32bit.
		ModifyBitsPerSample(wft, 32);
		hr = m_client->Initialize(mode, flags, duration, 0, &wft.Format, NULL);
		if (SUCCEEDED(hr)) {
			m_conversion = INT24_TO_INT32;
			return true;
		}
	}

	return false;
}
