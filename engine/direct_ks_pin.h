//=============================================================================
// KernelStreaming / PIN
//=============================================================================
#pragma once

#include <windows.h>
#include <ks.h>
#include "direct_ks_base.h"

class KsFilter;

// KernelStreamingピンクラス
class KsPin : protected KsBase
{
friend KsFilter;
public:
	static bool InitKsUser();
	static void TermKsUser();

private:
	// KSFilter経由でのみ生成・破棄が可能。
	KsPin(HANDLE filter, ULONG pin_id, const WAVEFORMATEX& wfx);
	virtual ~KsPin();

public:
	void Reset();
	bool SetState(KSSTATE state);
	bool GetState(KSSTATE& state);

	bool SetPosition(KSAUDIO_POSITION& pos);
	bool GetPosition(KSAUDIO_POSITION& pos);

	bool WriteData(KSSTREAM_HEADER& header, OVERLAPPED& ov);

private:
	void SetConnect(ULONG pin_id, const WAVEFORMATEX& wfx);

#if defined(SUPPORT_WAVERT)
	bool KsPin::AllocLooped(UINT buf_size);
#endif //defined(SUPPORT_WAVERT)

private:
	static const UINT CONNECT_SETTING_SIZE
		= sizeof(KSPIN_CONNECT)
		+ sizeof(KSDATAFORMAT_WAVEFORMATEX)
		+ sizeof(WAVEFORMATEXTENSIBLE);

	BYTE	m_buffer[CONNECT_SETTING_SIZE];

#if defined(SUPPORT_WAVERT)
	void*	m_hw_buffer;
	UINT	m_hw_bufsize;
#endif //defined(SUPPORT_WAVERT)
};
