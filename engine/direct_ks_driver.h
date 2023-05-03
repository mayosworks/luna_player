//=============================================================================
// DirectKS�h���C�o
//=============================================================================
#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <ks.h>
#include "driver.h"

//-----------------------------------------------------------------------------
// Declarations
//-----------------------------------------------------------------------------
class KsFilter;
class KsPin;

//-----------------------------------------------------------------------------
// DirectKS�I�[�f�B�I�o��
// �E�Đ����ɂ͊Y���f�o�C�X��苒���܂��B	
//-----------------------------------------------------------------------------
class DirectKsDriver : public Driver
{
public:
	DirectKsDriver();
	virtual ~DirectKsDriver();
	
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
	static const UINT PACKET_NUM = 5;

	struct Packet
	{
		KSSTREAM_HEADER	header;
		OVERLAPPED		signal;
		bool			queued;
	};

	KsFilter*	m_filter;
	KsPin*		m_pin;
	void*		m_buffer;
	Packet		m_packet[PACKET_NUM];
	UINT32		m_index;
	UINT32		m_buf_size;
	UINT64		m_sec_size;
	UINT32		m_padding;
	UINT64		m_written;
};
