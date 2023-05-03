//=============================================================================
// KernelStreaming / Filter
//=============================================================================
#pragma once

#include <windows.h>
#include <ks.h>
#include "direct_ks_base.h"

class KsPin;

// KernelStreaming�t�B���^�N���X
class KsFilter : protected KsBase
{
public:
	// �I�[�f�B�I�iWave�j�t�B���^���ǂ����̃`�F�b�N
	static bool IsValidPin(const TCHAR* filter_name);

public:
	explicit KsFilter(const TCHAR* filter_name);
	virtual ~KsFilter();

	// �s������
	KsPin* CreatePin(const WAVEFORMATEX& wfx);
	static void DestroyPin(KsPin* pin);

protected:
	// Pin Property
	bool GetPinPropertySimple(ULONG pin_id, REFGUID set, ULONG id, void* data, ULONG size);
	bool GetPinPropertyMulti(ULONG pin_id, REFGUID set, ULONG id, KSMULTIPLE_ITEM** item = NULL);

	ULONG GetPinNum();
	bool IsPinRenderer(ULONG pin_id);
	bool IsPinViable(ULONG pin_id);
	bool HasDataRange(ULONG pin_id);
	bool IsFormatSupported(ULONG pin_id, const WAVEFORMATEX& wfx);

private:
	KsFilter();
};
