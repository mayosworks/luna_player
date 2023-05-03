//=============================================================================
// ドライバ管理
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include <stdarg.h>
#include <map>
#include <vector>
#include "lib/wx_misc.h"
#include "lib/wx_string.h"
#include "driver.h"
#include "driver_manager.h"
#include "wave_out_driver.h"
#include "core_audio_driver.h"
#include "direct_ks_driver.h"

// 内部クラス
DriverManager::Impl* DriverManager::m_impl = NULL;

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
namespace {

const int INVALID_INDEX = -1;

// デバイス
struct Device
{
	Driver*		driver;	// ドライバ
	wx::String	name;	// 選択用表示名
	wx::String	guid;	// デバイス識別子
};

} //namespace

//-----------------------------------------------------------------------------
// 内部処理クラス
//-----------------------------------------------------------------------------
class DriverManager::Impl
{
public:
	Impl(bool use_ks);
	~Impl();

	void EnumDevices();
	void AddDevice(Driver* driver, const wchar_t* name, const wchar_t* guid);

	int GetDeviceNum() const;
	const wchar_t* GetDeviceName(int index) const;

	int GetIndexByName(const wchar_t* name) const;
	void SelectDevice(int index);

	Driver* GetDriver();
	const wchar_t* GetDeviceName() const;
	const wchar_t* GetDeviceGuid() const;

private:
	typedef std::vector<Device>	DeviceVector;

	DeviceVector	m_devices;
	Driver*			m_driver;
	wx::String		m_name;
	wx::String		m_guid;
	WaveOutDriver	m_wave;
	CoreAudioDriver	m_core;
	DirectKsDriver	m_diks;
	bool			m_use_ks;
};

//-----------------------------------------------------------------------------
// 内部コンストラクタ
//-----------------------------------------------------------------------------
DriverManager::Impl::Impl(bool use_ks)
	: m_devices()
	, m_driver(&m_wave)
	, m_name()
	, m_guid()
	, m_wave()
	, m_core()
	, m_diks()
	, m_use_ks(use_ks)
{
}

//-----------------------------------------------------------------------------
// 内部デストラクタ
//-----------------------------------------------------------------------------
DriverManager::Impl::~Impl()
{
}

//-----------------------------------------------------------------------------
// デバイス列挙
//-----------------------------------------------------------------------------
void DriverManager::Impl::EnumDevices()
{
	m_devices.clear();

	m_wave.EnumDevices();
	m_core.EnumDevices();

	if (m_use_ks) {
		m_diks.EnumDevices();
	}
}

//-----------------------------------------------------------------------------
// デバイス追加
//-----------------------------------------------------------------------------
void DriverManager::Impl::AddDevice(Driver* driver, const wchar_t* name, const wchar_t* guid)
{
	Device device;

	device.driver = driver;
	device.name = name;
	device.guid = guid;

	m_devices.push_back(device);
}

//-----------------------------------------------------------------------------
// デバイス数取得
//-----------------------------------------------------------------------------
int DriverManager::Impl::GetDeviceNum() const
{
	return static_cast<int>(m_devices.size());
}

//-----------------------------------------------------------------------------
// デバイス名取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::Impl::GetDeviceName(int index) const
{
	return m_devices[index].name;
}

//-----------------------------------------------------------------------------
// デバイスを名前から選択
//-----------------------------------------------------------------------------
int DriverManager::Impl::GetIndexByName(const wchar_t* name) const
{
	int index = 0;
	for (DeviceVector::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it, ++index) {
		if (it->name == name) {
			return index;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// デバイス選択
//-----------------------------------------------------------------------------
void DriverManager::Impl::SelectDevice(int index)
{
	m_driver = NULL;
	m_name.Clear();
	m_guid.Clear();

	if (0 <= index && index < GetDeviceNum()) {
		m_driver = m_devices[index].driver;
		m_name = m_devices[index].name;
		m_guid = m_devices[index].guid;
	}
}

//-----------------------------------------------------------------------------
// 現在のドライバ取得
//-----------------------------------------------------------------------------
Driver* DriverManager::Impl::GetDriver()
{
	return m_driver;
}

//-----------------------------------------------------------------------------
// 現在のデバイス名取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::Impl::GetDeviceName() const
{
	return m_name;
}

//-----------------------------------------------------------------------------
// 現在のデバイス識別子取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::Impl::GetDeviceGuid() const
{
	return m_guid;
}


//-----------------------------------------------------------------------------
// マネージャー初期化
//-----------------------------------------------------------------------------
bool DriverManager::Initialize(const wchar_t* device_name, bool use_ks)
{
	m_impl = new Impl(use_ks);
	if (!m_impl) {
		return false;
	}

	m_impl->EnumDevices();

	// 起動時のデバイスで選択できない場合は、デフォルトを選ぶ
	int index = m_impl->GetIndexByName(device_name);
	if (index == INVALID_INDEX) {
		index = 0;
	}

	m_impl->SelectDevice(index);
	return true;
}

//-----------------------------------------------------------------------------
// マネージャー終了
//-----------------------------------------------------------------------------
void DriverManager::Finalize()
{
	WX_SAFE_DELETE(m_impl);
}

//-----------------------------------------------------------------------------
// デバイス列挙
//-----------------------------------------------------------------------------
void DriverManager::EnumDevices()
{
	m_impl->EnumDevices();
}

//-----------------------------------------------------------------------------
// デバイス数取得
//-----------------------------------------------------------------------------
int DriverManager::GetDeviceNum()
{
	return m_impl->GetDeviceNum();
}

//-----------------------------------------------------------------------------
// デバイス名取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::GetDeviceName(int index)
{
	return m_impl->GetDeviceName(index);
}

//-----------------------------------------------------------------------------
// デバイス選択
//-----------------------------------------------------------------------------
void DriverManager::SelectDevice(const wchar_t* device_name)
{
	int index = m_impl->GetIndexByName(device_name);
	m_impl->SelectDevice(index);
}

//-----------------------------------------------------------------------------
// デバイス選択
//-----------------------------------------------------------------------------
void DriverManager::SelectDevice(int index)
{
	m_impl->SelectDevice(index);
}

//-----------------------------------------------------------------------------
// 現在のドライバ取得
//-----------------------------------------------------------------------------
Driver* DriverManager::GetDriver()
{
	return m_impl->GetDriver();
}

//-----------------------------------------------------------------------------
// 現在のデバイス名取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::GetDeviceName()
{
	return m_impl->GetDeviceName();
}

//-----------------------------------------------------------------------------
// 現在のデバイス識別子取得
//-----------------------------------------------------------------------------
const wchar_t* DriverManager::GetDeviceGuid()
{
	return m_impl->GetDeviceGuid();
}

//-----------------------------------------------------------------------------
// ドライバ追加
//-----------------------------------------------------------------------------
void DriverManager::AddDevice(Driver* driver, const wchar_t* name, const wchar_t* guid)
{
	m_impl->AddDevice(driver, name, guid);
}
