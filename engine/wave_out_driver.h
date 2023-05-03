//=============================================================================
// WaveOutドライバ
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#pragma warning(disable: 4201)	//非標準の拡張機能が使用されています : 無名の構造体または共用体です。

#include <windows.h>
#include <mmsystem.h>
#include "driver.h"

// WaveOutドライバ実装
class WaveOutDriver : public Driver
{
public:
	WaveOutDriver();
	virtual ~WaveOutDriver();
	
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
	bool WritePacket();

private:
	static const int PACKET_NUM = 4;

	HWAVEOUT	m_device;
	void*		m_buffer;
	WAVEHDR		m_packet[PACKET_NUM];
	UINT		m_index;
	UINT		m_buf_size;
	UINT		m_sec_size;
};
