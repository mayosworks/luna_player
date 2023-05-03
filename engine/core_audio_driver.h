//=============================================================================
// CoreAudio排他モードドライバ
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <windows.h>
#include <mmsystem.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include "lib/wx_string.h"
#include "driver.h"

// CoreAudioドライバ実装
class CoreAudioDriver : public Driver
{
public:
	CoreAudioDriver();
	virtual ~CoreAudioDriver();
	
	virtual bool EnumDevices();

	virtual bool Open(const wchar_t* guid, const WAVEFORMATEX& wfx);
	virtual void Close();
	virtual void Reset();

	virtual bool Start();
	virtual bool Stop();

	virtual UINT Write(const void* data, UINT size);
	virtual bool Flush();

	virtual UINT GetPlayTime() const;
	virtual bool GetFormat(WAVEFORMATEX& wfx) const;

private:
	bool InitializeClient(AUDCLNT_SHAREMODE mode, DWORD flags, REFERENCE_TIME duration, WAVEFORMATEXTENSIBLE& wft);

private:
	IMMDevice*			m_device;
	IAudioClient*		m_client;
	IAudioRenderClient*	m_render;
	UINT				m_buf_size;
	UINT				m_smp_size;
	UINT				m_samples;
	UINT				m_align;
	int					m_bits;
};
