//=============================================================================
// サウンドドライバ管理
//=============================================================================
#pragma once

class Driver;

//-----------------------------------------------------------------------------
// ドライバ管理マネージャー
//-----------------------------------------------------------------------------
class DriverManager
{
	friend class Driver;

public:
	static bool Initialize(const wchar_t* device_name, bool use_ks);
	static void Finalize();

	static void EnumDevices();

	static int GetDeviceNum();
	static const wchar_t* GetDeviceName(int index);

	static void SelectDevice(const wchar_t* device_name);
	static void SelectDevice(int index);

	static Driver* GetDriver();
	static const wchar_t* GetDeviceName();
	static const wchar_t* GetDeviceGuid();

private:
	DriverManager();
	~DriverManager();

	static void AddDevice(Driver* driver, const wchar_t* name, const wchar_t* guid);

private:
	class Impl;
	static Impl* m_impl;
};
