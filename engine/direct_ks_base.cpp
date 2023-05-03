//=============================================================================
// KernelStreaming / IRP
//=============================================================================

#include <windows.h>
#include <winioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include "lib/wx_misc.h"
#include "direct_ks_base.h"

//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
KsBase::KsBase(HANDLE handle)
	: m_handle(handle)
{ 
}

//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
KsBase::~KsBase()
{
	if (m_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
}

//-----------------------------------------------------------------------------
// DeviceIoControl���s
//-----------------------------------------------------------------------------
BOOL KsBase::Ioctl(DWORD code, void* in_data, DWORD in_size,
		void* out_data, DWORD out_size, DWORD* returned, OVERLAPPED& ov)
{
	return DeviceIoControl(m_handle, code, in_data, in_size, out_data, out_size, returned, &ov);
}

//-----------------------------------------------------------------------------
// SynchronizeDeviceIoControl���s
//-----------------------------------------------------------------------------
bool KsBase::SyncIoctl(DWORD code, void* in_data, DWORD in_size,
					void* out_data, DWORD out_size, DWORD* returned)
{
	DWORD temp = 0;
	if (!returned) {
		returned = &temp;
	}

	OVERLAPPED ov;
	ZeroMemory(&ov, sizeof(ov));

	ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!ov.hEvent) {
		WX_TRACE("CreateEvent failed.");
		return false;
	}

	//���̏����́H�s���c�c�B
	//ov.hEvent = (HANDLE)((DWORD_PTR)ov.hEvent | 0x1);

	HRESULT hr = S_OK;
	BOOL ret = Ioctl(code, in_data, in_size, out_data, out_size, returned, ov);
	if (!ret) {
		DWORD err = GetLastError();
		if (err == ERROR_IO_PENDING) {
			DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);
			if (wait != WAIT_OBJECT_0) {
				hr = E_FAIL;
				WX_TRACE("WaitForSingleObject failed. ret = 0x%08x.", wait);
			}
		}
		else if (((err == ERROR_INSUFFICIENT_BUFFER) || (err == ERROR_MORE_DATA))
			&& (IOCTL_KS_PROPERTY == code) && (out_size == 0)) {
			hr = S_OK;
			ret = TRUE;
		}
		else {
			hr = E_FAIL;
		}
	}

	if (!ret) {
		*returned = 0;
	}

	CloseHandle(ov.hEvent);
	return SUCCEEDED(hr);
}

//-----------------------------------------------------------------------------
// �v���p�e�B�Z�b�g���T�|�[�g���Ă邩�H
//-----------------------------------------------------------------------------
bool KsBase::IsPropertySetSupported(REFGUID set)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Flags = KSPROPERTY_TYPE_SETSUPPORT;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), NULL, 0);
}

//-----------------------------------------------------------------------------
// ��{�̃v���p�e�B�Z�b�g���T�|�[�g���Ă邩�H
//-----------------------------------------------------------------------------
bool KsBase::IsPropertyBasicSupported(REFGUID set, ULONG id, DWORD* result)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_BASICSUPPORT;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), result, sizeof(*result));
}

//-----------------------------------------------------------------------------
// �P��v���p�e�B�擾
//-----------------------------------------------------------------------------
bool KsBase::GetPropertySimple(REFGUID set, ULONG id, void* data, ULONG size)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_GET;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// �P��v���p�e�B�ݒ�
//-----------------------------------------------------------------------------
bool KsBase::SetPropertySimple(REFGUID set, ULONG id, void* data, ULONG size)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_SET;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// �����v���p�e�B�擾
//-----------------------------------------------------------------------------
bool KsBase::GetPropertyMulti(REFGUID set, ULONG id, KSMULTIPLE_ITEM** item)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_GET;

	ULONG bytes = 0;
	if (!SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), NULL, 0, &bytes)) {
		return false;
	}

	void* buf = MemAlloc<void*>(bytes);
	if (!buf) {
		return false;
	}

	if (!SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), buf, bytes)) {
		MemFree(buf);
		return false;
	}

	*item = static_cast<KSMULTIPLE_ITEM*>(buf);
	return true;
}

//-----------------------------------------------------------------------------
// �����v���p�e�B�ݒ�
//-----------------------------------------------------------------------------
bool KsBase::SetPropertyMulti(REFGUID set, ULONG id, KSMULTIPLE_ITEM** item)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_SET;

	ULONG bytes = 0;
	if (!SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), NULL, 0, &bytes)) {
		return false;
	}

	void* buf = MemAlloc<void*>(bytes);
	if (!buf) {
		return false;
	}

	if (!SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), buf, bytes)) {
		MemFree(buf);
		return false;
	}

	*item = static_cast<KSMULTIPLE_ITEM*>(buf);
	return true;
}

//-----------------------------------------------------------------------------
// �����v���p�e�B���f�[�^���g���擾
//-----------------------------------------------------------------------------
bool KsBase::GetPropertyMulti(REFGUID set, ULONG id, void* data, ULONG size, KSMULTIPLE_ITEM** item)
{
	KSPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Set = set;
	prop.Id = id;
	prop.Flags = KSPROPERTY_TYPE_GET;

	ULONG bytes = 0;
	if (!SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), NULL, 0, &bytes)) {
		return false;
	}

	void* buf = MemAlloc<void*>(bytes); 
	if (!buf) {
		return false;
	}

	if (!SyncIoctl(IOCTL_KS_PROPERTY, data, size, buf, bytes)) {
		MemFree(buf);
		return false;
	}

	*item = static_cast<KSMULTIPLE_ITEM*>(buf);
	return true;
}

//-----------------------------------------------------------------------------
// �P��m�[�h�v���p�e�B�擾
//-----------------------------------------------------------------------------
bool KsBase::GetNodePropertySimple(ULONG node, REFGUID set, ULONG id, void* data, ULONG size)
{
	KSNODEPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Property.Set = set;
	prop.Property.Id = id;
	prop.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
	prop.NodeId = node;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// �P��m�[�h�v���p�e�B�ݒ�
//-----------------------------------------------------------------------------
bool KsBase::SetNodePropertySimple(ULONG node, REFGUID set, ULONG id, void* data, ULONG size)
{
	KSNODEPROPERTY prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Property.Set = set;
	prop.Property.Id = id;
	prop.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
	prop.NodeId = node;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// �`�����l���m�[�h�v���p�e�B�擾
//-----------------------------------------------------------------------------
bool KsBase::GetNodePropertyChannel(ULONG node, ULONG ch, REFGUID set, ULONG id, void* data, ULONG size)
{
	KSNODEPROPERTY_AUDIO_CHANNEL prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.NodeProperty.Property.Set = set;
	prop.NodeProperty.Property.Id = id;
	prop.NodeProperty.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
	prop.NodeProperty.NodeId = node;
	prop.Channel = ch;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// �`�����l���m�[�h�v���p�e�B�ݒ�
//-----------------------------------------------------------------------------
bool KsBase::SetNodePropertyChannel(ULONG node, ULONG ch, REFGUID set, ULONG id, void* data, ULONG size)
{
	KSNODEPROPERTY_AUDIO_CHANNEL prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.NodeProperty.Property.Set = set;
	prop.NodeProperty.Property.Id = id;
	prop.NodeProperty.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
	prop.NodeProperty.NodeId = node;
	prop.Channel = ch;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}
