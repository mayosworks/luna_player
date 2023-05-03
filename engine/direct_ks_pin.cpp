//=============================================================================
// KernelStreaming / PIN
//=============================================================================

#define KS_NO_CREATE_FUNCTIONS

#include <windows.h>
#include <winioctl.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include "lib/wx_misc.h"
#include "driver_utility.h"
#include "direct_ks_filter.h"
#include "direct_ks_pin.h"

typedef KSDDKAPI DWORD WINAPI fnKsCreatePin(HANDLE filter_handle,
	KSPIN_CONNECT* connect, ACCESS_MASK desired_access, HANDLE* connection_handle);

static HMODULE			g_ksuser = NULL;
static fnKsCreatePin*	KsCreatePin = NULL;


//-----------------------------------------------------------------------------
// KsUser関連の初期化
//-----------------------------------------------------------------------------
bool KsPin::InitKsUser()
{
	// このDLLはアンロードしなくていい。
	HMODULE ksuser = LoadLibrary(TEXT("ksuser.dll"));
	if (!ksuser) {
		return false;
	}

	KsCreatePin = reinterpret_cast<fnKsCreatePin*>(
				GetProcAddress(ksuser, "KsCreatePin"));
	if (!KsCreatePin) {
		FreeLibrary(ksuser);
		return false;
	}

	g_ksuser = ksuser;
	return true;
}

//-----------------------------------------------------------------------------
// KsUser関連の破棄
//-----------------------------------------------------------------------------
void KsPin::TermKsUser()
{
	if (g_ksuser) {
		KsCreatePin = NULL;

		FreeLibrary(g_ksuser);
		g_ksuser = NULL;
	}
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
KsPin::KsPin(HANDLE filter, ULONG pin_id, const WAVEFORMATEX& wfx)
{
	ZeroMemory(m_buffer, sizeof(m_buffer));
	SetConnect(pin_id, wfx);

	if (KsCreatePin) {
		KSPIN_CONNECT* connect = reinterpret_cast<KSPIN_CONNECT*>(m_buffer);
		HANDLE pin = INVALID_HANDLE_VALUE;

		DWORD ret = KsCreatePin(filter, connect, FILE_READ_ACCESS | FILE_WRITE_ACCESS, &pin);
		if (ret != ERROR_SUCCESS) {
			WX_TRACE("Failed to create pin. result = %d\n", ret);
		}

		SetHandle(pin);
	}

	SetState(KSSTATE_STOP);
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
KsPin::~KsPin()
{
}

//-----------------------------------------------------------------------------
// ピンの状態をリセットする
//-----------------------------------------------------------------------------
void KsPin::Reset()
{
	ULONG in = 0;

	in = KSRESET_BEGIN;
	if (!SyncIoctl(IOCTL_KS_RESET_STATE, &in, sizeof(in), NULL, 0)) {
		WX_TRACE("IOCTL_KS_RESET_STATE/KSRESET_BEGIN failed.\n");
	}

	in = KSRESET_END;
	if (!SyncIoctl(IOCTL_KS_RESET_STATE, &in, sizeof(in), NULL, 0)) {
		WX_TRACE("IOCTL_KS_RESET_STATE/KSRESET_END failed.\n");
	}
}

//-----------------------------------------------------------------------------
// ピンの状態を設定する
//-----------------------------------------------------------------------------
bool KsPin::SetState(KSSTATE state)
{
	return SetPropertySimple(KSPROPSETID_Connection,
		KSPROPERTY_CONNECTION_STATE, &state, sizeof(state));
}

//-----------------------------------------------------------------------------
// ピンの状態を取得する
//-----------------------------------------------------------------------------
bool KsPin::GetState(KSSTATE& state)
{
	return GetPropertySimple(KSPROPSETID_Connection,
			KSPROPERTY_CONNECTION_STATE, &state, sizeof(state));
}

//-----------------------------------------------------------------------------
// 再生位置・書き込み位置をセットする
//-----------------------------------------------------------------------------
bool KsPin::SetPosition(KSAUDIO_POSITION& pos)
{
	return SetPropertySimple(KSPROPSETID_Audio, KSPROPERTY_AUDIO_POSITION, &pos, sizeof(pos));
}

//-----------------------------------------------------------------------------
// 再生位置・書き込み位置を取得する
//-----------------------------------------------------------------------------
bool KsPin::GetPosition(KSAUDIO_POSITION& pos)
{
	return GetPropertySimple(KSPROPSETID_Audio, KSPROPERTY_AUDIO_POSITION, &pos, sizeof(pos));
}

//-----------------------------------------------------------------------------
// データを書く
//-----------------------------------------------------------------------------
bool KsPin::WriteData(KSSTREAM_HEADER& header, OVERLAPPED& ov)
{
	DWORD returned = 0;
	BOOL ret = Ioctl(IOCTL_KS_WRITE_STREAM, NULL, 0, &header, header.Size, &returned, ov);
	if (ret) {
		return true;
	}

	DWORD err = GetLastError();
	if (err == ERROR_IO_PENDING) {
		return true;
	}

	WX_TRACE("DeviceIoControl Failed! Error=%d.\n", err);
	return false;
}

//-----------------------------------------------------------------------------
// ピン接続を設定する
//-----------------------------------------------------------------------------
void KsPin::SetConnect(ULONG pin_id, const WAVEFORMATEX& wfx)
{
	KSPIN_CONNECT* connect = reinterpret_cast<KSPIN_CONNECT*>(m_buffer);

	connect->Interface.Set              = KSINTERFACESETID_Standard;
	connect->Interface.Id               = KSINTERFACE_STANDARD_STREAMING;
	connect->Interface.Flags            = 0;
	connect->Medium.Set                 = KSMEDIUMSETID_Standard;
	connect->Medium.Id                  = KSMEDIUM_TYPE_ANYINSTANCE;
	connect->Medium.Flags               = 0;
	connect->PinId                      = pin_id;
	connect->PinToHandle                = NULL;
	connect->Priority.PriorityClass     = KSPRIORITY_HIGH;//KSPRIORITY_NORMAL;
	connect->Priority.PrioritySubClass  = 1;

	KSDATAFORMAT_WAVEFORMATEX* kswfx = reinterpret_cast<KSDATAFORMAT_WAVEFORMATEX*>(connect + 1);
	CopyMemory(&kswfx->WaveFormatEx, &wfx, sizeof(wfx) + wfx.cbSize);

	kswfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEX) + wfx.cbSize;
	kswfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
	kswfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	kswfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
	kswfx->DataFormat.SampleSize = wfx.nChannels * wfx.wBitsPerSample / 8;

	if (wfx.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		const WAVEFORMATEXTENSIBLE& wft = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(wfx);
		kswfx->DataFormat.SubFormat = wft.SubFormat;
	}
	else {
		kswfx->DataFormat.SubFormat = GetSubFormatGUID(wfx.wFormatTag);
		//	(wfx.wFormatTag == WAVE_FORMAT_PCM)? KSDATAFORMAT_SUBTYPE_PCM : KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	}

	//SetPropertySimple(KSPROPSETID_Connection, KSPROPERTY_CONNECTION_DATAFORMAT,
	//	static_cast<void*>(kswfx), kswfx->DataFormat.FormatSize, NULL, 0);
}

#if defined(SUPPORT_WAVERT)
//-----------------------------------------------------------------------------
// Looped用バッファを確保(for WaveRT)
//-----------------------------------------------------------------------------
bool KsPin::AllocLooped(UINT buf_size)
{
	KSRTAUDIO_BUFFER ksbuf;
	ZeroMemory(&ksbuf, sizeof(ksbuf));

	KSRTAUDIO_BUFFER_PROPERTY prop;

	ZeroMemory(&prop, sizeof(prop));
	prop.Property.Set = KSPROPSETID_RtAudio;
	prop.Property.Id = KSPROPERTY_RTAUDIO_BUFFER;
	prop.Property.Flags = KSPROPERTY_TYPE_GET;
	prop.RequestedBufferSize = buf_size;

	DWORD returned = 0;
	bool ret = SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), &ksbuf, sizeof(ksbuf), &returned);
	if (!ret) {
		WX_TRACE("Failed to Set Pin Buffer.\n");
		return false;
	}

	if (!ksbuf.BufferAddress) {
		return false;
	}

	m_hw_buffer = ksbuf.BufferAddress;
	m_hw_bufsize = ksbuf.ActualBufferSize;
	return true;
}
#endif //defined(SUPPORT_WAVERT)
