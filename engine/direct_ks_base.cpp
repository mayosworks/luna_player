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
// コンストラクタ
//-----------------------------------------------------------------------------
KsBase::KsBase(HANDLE handle)
	: m_handle(handle)
{ 
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
KsBase::~KsBase()
{
	if (m_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
}

//-----------------------------------------------------------------------------
// DeviceIoControl実行
//-----------------------------------------------------------------------------
BOOL KsBase::Ioctl(DWORD code, void* in_data, DWORD in_size,
		void* out_data, DWORD out_size, DWORD* returned, OVERLAPPED& ov)
{
	return DeviceIoControl(m_handle, code, in_data, in_size, out_data, out_size, returned, &ov);
}

//-----------------------------------------------------------------------------
// SynchronizeDeviceIoControl実行
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

	//この処理は？不明……。
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
// プロパティセットをサポートしてるか？
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
// 基本のプロパティセットをサポートしてるか？
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
// 単一プロパティ取得
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
// 単一プロパティ設定
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
// 複数プロパティ取得
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
// 複数プロパティ設定
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
// 複数プロパティをデータを使い取得
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
// 単一ノードプロパティ取得
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
// 単一ノードプロパティ設定
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
// チャンネルノードプロパティ取得
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
// チャンネルノードプロパティ設定
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
