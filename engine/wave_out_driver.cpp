//=============================================================================
// MME/WaveOutドライバ
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys.h>
#include "driver_utility.h"
#include "wave_out_driver.h"

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
namespace {

// 状態
enum State
{
	STATE_UNUSED,	// 未使用
	STATE_QUEUED,	// 出力待ち
	STATE_PLAYING,	// 再生中
};

bool GetMMDeviceName(IMMDevice* device, wchar_t* device_name);
bool GetDeviceId(const wchar_t* device_name, UINT& device_id);

} //namespace

#define DRV_QUERYFUNCTIONINSTANCEID		(DRV_RESERVED + 17)
#define DRV_QUERYFUNCTIONINSTANCEIDSIZE	(DRV_RESERVED + 18)

namespace {

//-----------------------------------------------------------------------------
// CoreAudio/DeviceAPIを使用して、デバイス名を取得
//-----------------------------------------------------------------------------
bool GetMMDeviceName(IMMDevice* device, wchar_t* device_name)
{
	bool result = false;

	PROPVARIANT friendly_name;
	PropVariantInit(&friendly_name);

	IPropertyStore* property_store = NULL;
	HRESULT hr = device->OpenPropertyStore(STGM_READ, &property_store);
	if (SUCCEEDED(hr)) {
		hr = property_store->GetValue(PKEY_Device_FriendlyName, &friendly_name);
		if (SUCCEEDED(hr) && friendly_name.vt == VT_LPWSTR) {
			lstrcpyn(device_name, friendly_name.pwszVal, MAX_PATH);
			StrTrim(device_name, TEXT(" "));
			result = true;
		}
	}

	WX_SAFE_RELEASE(property_store);
	PropVariantClear(&friendly_name);
	return result;
}

//-----------------------------------------------------------------------------
// デバイス検索
//-----------------------------------------------------------------------------
bool GetDeviceId(const wchar_t* device_name, UINT& device_id)
{
	device_id = WAVE_MAPPER;

	UINT device_num = waveOutGetNumDevs();
	if (device_num == 0) {
		WX_TRACE("[ERROR] オーディオデバイスが１つもありません！\n");
		return false;
	}

	// WAVE_MAPPER使用の時
	if (device_name[0] == L'\0') {
		return true;
	}

	// それ以外のオーディオデバイス選択時
	for (UINT i = 0; i < device_num; ++i) {
		WAVEOUTCAPS caps;
		if (waveOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
			StrTrim(caps.szPname, TEXT(" "));
			if (lstrcmp(caps.szPname, device_name) == 0) {
				device_id = i;
			}
		}
	}

	// 指定のオーディオデバイスがない場合は、エラーとする
	if (device_id == WAVE_MAPPER) {
		WX_TRACE("[ERROR] 選択しているオーディオデバイスがありません！\n");
		return false;
	}

	return true;
}

} //namespace

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
WaveOutDriver::WaveOutDriver()
	: m_device(NULL)
	, m_buffer(NULL)
	, m_index(0)
	, m_buf_size(0)
	, m_sec_size(1)
{
	ZeroMemory(m_packet, sizeof(m_packet));
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
WaveOutDriver::~WaveOutDriver()
{
}

//-----------------------------------------------------------------------------
// デバイス列挙
//-----------------------------------------------------------------------------
bool WaveOutDriver::EnumDevices()
{
	UINT device_num = waveOutGetNumDevs();
	if (device_num == 0) {
		return 0;
	}

	// Vista以降は、名前が切れることがあるため、CoreAudioAPIを使用して取得。
	OSVERSIONINFO osi;

	ZeroMemory(&osi, sizeof(osi));
	osi.dwOSVersionInfoSize = sizeof(osi);

	GetVersionEx(&osi);
  
	IMMDeviceEnumerator* device_enumerator = NULL;
	if (osi.dwMajorVersion > 5) {
		HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
			NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_enumerator)); 
		if (FAILED(hr)) {
			WX_TRACE("Can't create IMMDeviceEnumerator instance. hr = %x", hr);
		}
	}

	wchar_t name[MAX_PATH + 8];
	lstrcpy(name, L"Wave: ");

	lstrcpyn(&name[6], L"既定のオーディオデバイス", MAX_PATH);
	AddDevice(name, L"");

	for (UINT i = 0; i < device_num; ++i) {
		WAVEOUTCAPS caps;
		if (waveOutGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
			continue;
		}

		int len = lstrlen(caps.szPname) + 1;
		StrTrim(caps.szPname, TEXT(" "));
		lstrcpyn(&name[6], caps.szPname, MAX_PATH);

		if (device_enumerator && len == MAXPNAMELEN) {
			wchar_t dev_id[MAX_PATH];

			MMRESULT ret = waveOutMessage(reinterpret_cast<HWAVEOUT>(i),
				DRV_QUERYFUNCTIONINSTANCEID, reinterpret_cast<DWORD_PTR>(dev_id), MAX_PATH);
			if (ret == MMSYSERR_NOERROR) {
				IMMDevice* device = NULL;
				HRESULT hr = device_enumerator->GetDevice(dev_id, &device);
				if (SUCCEEDED(hr)) {
					GetMMDeviceName(device, &name[6]);
				}

				WX_SAFE_RELEASE(device);
			}
		}

		AddDevice(name, caps.szPname);
	}

	WX_SAFE_RELEASE(device_enumerator);
	return true;
}

//-----------------------------------------------------------------------------
// デバイスを開く
//-----------------------------------------------------------------------------
bool WaveOutDriver::Open(const wchar_t* guid, const WAVEFORMATEX& wfx)
{
	WAVEFORMATEXTENSIBLE wft;
	CopyToExtensible(wft, wfx);
	SetupVolume(wfx);

	UINT device_id = WAVE_MAPPER;
	if (!GetDeviceId(guid, device_id)) {
		return false;
	}

	MMRESULT ret = waveOutOpen(&m_device, device_id, &wft.Format, 0, 0, CALLBACK_NULL);
	if (ret != MMSYSERR_NOERROR) {
		wft.Format.wFormatTag = wfx.wFormatTag;
		wft.Format.cbSize = 0;

		ret = waveOutOpen(&m_device, device_id, &wft.Format, 0, 0, 0);
		if (ret != MMSYSERR_NOERROR) {
			WX_TRACE("[ERROR] デバイスを開けませんでした！ ret=%d\n", ret);
			return false;
		}
	}

	waveOutPause(m_device);

	m_buf_size = wfx.nAvgBytesPerSec / PACKET_NUM;
	m_buf_size -= (m_buf_size % wfx.nBlockAlign);

	m_buffer = HeapAlloc(GetProcessHeap(), 0, m_buf_size * PACKET_NUM);
	if (!m_buffer) {
		Close();
		return false;
	}

	ZeroMemory(m_packet, sizeof(m_packet));

	for (int i = 0; i < PACKET_NUM; ++i) {
		m_packet[i].lpData = static_cast<char*>(wx::AddOffset(m_buffer, i * m_buf_size));
	}

	m_index = 0;
	m_sec_size = wfx.nAvgBytesPerSec;

	Reset();
	return true;
}

//-----------------------------------------------------------------------------
// デバイスを閉じる
//-----------------------------------------------------------------------------
void WaveOutDriver::Close()
{
	Stop();

	if (m_device) {
		waveOutReset(m_device);
		Sleep(0);

		for (int i = 0; i < PACKET_NUM; ++i) {
			waveOutUnprepareHeader(m_device, &m_packet[i], sizeof(m_packet[0]));
		}

		waveOutClose(m_device);
		m_device = NULL;
	}

	if (m_buffer) {
		HeapFree(GetProcessHeap(), 0, m_buffer);
		m_buffer = NULL;
	}

	m_index = 0;
	m_buf_size = 0;
	m_sec_size = 1;
}

//-----------------------------------------------------------------------------
// リセット
//-----------------------------------------------------------------------------
void WaveOutDriver::Reset()
{
	m_index = 0;

	if (m_device) {
		waveOutReset(m_device);

		for (int i = 0; i < PACKET_NUM; ++i) {
			waveOutUnprepareHeader(m_device, &m_packet[i], sizeof(m_packet[0]));
			m_packet[i].dwBufferLength = 0;
			m_packet[i].dwUser = STATE_UNUSED;
		}
	}
}

//-----------------------------------------------------------------------------
// 出力開始
//-----------------------------------------------------------------------------
bool WaveOutDriver::Start()
{
	if (!m_device) {
		return false;
	}

	MMRESULT ret = waveOutRestart(m_device);
	if (ret != MMSYSERR_NOERROR) {
		WX_TRACE("[ERROR] 再生できません！ ret=%d\n", ret);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 出力停止
//-----------------------------------------------------------------------------
bool WaveOutDriver::Stop()
{
	if (!m_device) {
		return false;
	}

	MMRESULT ret = waveOutPause(m_device);
	if (ret != MMSYSERR_NOERROR) {
		WX_TRACE("[ERROR] 停止できません！ ret=%d\n", ret);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 書き込み
//-----------------------------------------------------------------------------
UINT WaveOutDriver::Write(const void* data, UINT size)
{
	if (size == 0) {
		return 0;
	}

	WAVEHDR& packet = m_packet[m_index];
	if (packet.dwUser == STATE_PLAYING) {
		if (!(packet.dwFlags & WHDR_DONE)) {
			return 0;
		}

		waveOutUnprepareHeader(m_device, &packet, sizeof(packet));
		packet.dwUser = STATE_UNUSED;
	}

	if (packet.dwUser == STATE_UNUSED) {
		packet.dwBufferLength = m_buf_size;
		packet.dwFlags = 0;

		MMRESULT ret = waveOutPrepareHeader(m_device, &packet, sizeof(packet));
		if (ret != MMSYSERR_NOERROR) {
			WX_TRACE("[ERROR] バッファの準備に失敗しました！ ret=%d\n", ret);
			return 0;
		}

		packet.dwBufferLength = 0;
	}

	UINT used = m_packet[m_index].dwBufferLength;
	if (used + size > m_buf_size) {
		size = m_buf_size - used;
	}

	//CopyMemory(packet.lpData + used, data, size);
	ProcessVolume(packet.lpData + used, data, size);
	m_packet[m_index].dwBufferLength += size;
	m_packet[m_index].dwUser = STATE_QUEUED;

	if (m_packet[m_index].dwBufferLength == m_buf_size) {
		WritePacket();
	}

	return size;
}

//-----------------------------------------------------------------------------
// 書き込みフラッシュ
//-----------------------------------------------------------------------------
bool WaveOutDriver::Flush()
{
	if (m_packet[m_index].dwUser != STATE_QUEUED) {
		if (!WritePacket()) {
			return true;
		}
	}

	for (int i = 0; i < PACKET_NUM; ++i) {
		if (m_packet[i].dwUser == STATE_PLAYING) {
			if (!(m_packet[i].dwFlags & WHDR_DONE)) {
				return false;
			}

			m_packet[i].dwUser = STATE_UNUSED;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// 再生時間取得
//-----------------------------------------------------------------------------
UINT WaveOutDriver::GetPlayTime() const
{
	if (!m_device) {
		return 0;
	}

	MMTIME time;
	time.wType = TIME_BYTES;

	MMRESULT ret = waveOutGetPosition(m_device, &time, sizeof(time));
	if (ret != MMSYSERR_NOERROR) {
		WX_TRACE("[ERROR] 再生位置を取得できません！ ret = %d\n", ret);
		return 0;
	}

	if (time.wType == TIME_BYTES) {
		return MulDiv(time.u.cb, 1000, m_sec_size);
	}
	else if (time.wType == TIME_MS) {
		return time.u.ms;
	}

	return 0;
}

bool WaveOutDriver::GetFormat(WAVEFORMATEX& /*wfx*/) const
{
	return false;
}

//-----------------------------------------------------------------------------
// パケット書き込み
//-----------------------------------------------------------------------------
bool WaveOutDriver::WritePacket()
{
	MMRESULT ret = waveOutWrite(m_device, &m_packet[m_index], sizeof(m_packet[0]));
	if (ret != MMSYSERR_NOERROR) {
		WX_TRACE("[ERROR] バッファの送信に失敗しました！ ret=%d\n", ret);
		waveOutUnprepareHeader(m_device, &m_packet[m_index], sizeof(m_packet[0]));
		return false;
	}

	m_packet[m_index].dwUser = STATE_PLAYING;
	if (++m_index >= PACKET_NUM) {
		m_index = 0;
	}

	return true;
}
