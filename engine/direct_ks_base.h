//=============================================================================
// KernelStreaming / IRP base
//=============================================================================
#pragma once

#include <windows.h>
#include <ks.h>

// KernelStreaming基本クラス
class KsBase
{
public:
	// Memory Alloc
	template <typename T> static inline T* MemAlloc(size_t size)
	{
		void* mem = malloc(size);
		return static_cast<T*>(mem);
	}

	// Memory Free
	static inline void MemFree(void* mem)
	{
		free(mem);
	}

protected:
	KsBase(HANDLE handle = INVALID_HANDLE_VALUE);
	virtual ~KsBase();

	// handle getter/setter
	HANDLE GetHandle() { return m_handle; }
	void SetHandle(HANDLE handle) { m_handle = handle; }

	// このインターフェイスは有効か？
	bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE; }

public:
	// DeviceIoControl
	BOOL Ioctl(DWORD code, void* in_data, DWORD in_size,
		void* out_data, DWORD out_size, DWORD* returned, OVERLAPPED& ov);

	// SynchronizeDeviceIoControl
	bool SyncIoctl(DWORD code, void* in_data, DWORD in_size,
		void* out_data, DWORD out_size, DWORD* returned = NULL);

	// Property Support Checker
	bool IsPropertySetSupported(REFGUID set);
	bool IsPropertyBasicSupported(REFGUID set, ULONG id, DWORD* result);

	// Simple Property
	bool GetPropertySimple(REFGUID set, ULONG id, void* data, ULONG size);
	bool SetPropertySimple(REFGUID set, ULONG id, void* data, ULONG size);

	// Multiple Property
	bool GetPropertyMulti(REFGUID set, ULONG id, KSMULTIPLE_ITEM** item);
	bool SetPropertyMulti(REFGUID set, ULONG id, KSMULTIPLE_ITEM** item);

	bool GetPropertyMulti(REFGUID set, ULONG id, void* data, ULONG size, KSMULTIPLE_ITEM** item);

	// Simple Node Property
	bool GetNodePropertySimple(ULONG node, REFGUID set, ULONG id, void* data, ULONG size);
	bool SetNodePropertySimple(ULONG node, REFGUID set, ULONG id, void* data, ULONG size);

	// Node Channel Property
	bool GetNodePropertyChannel(ULONG node, ULONG ch, REFGUID set, ULONG id, void* data, ULONG size);
	bool SetNodePropertyChannel(ULONG node, ULONG ch, REFGUID set, ULONG id, void* data, ULONG size);

protected:
	HANDLE	m_handle;
};
