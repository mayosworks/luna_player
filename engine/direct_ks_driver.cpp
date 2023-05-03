//=============================================================================
// DirectKS�h���C�o
//=============================================================================

#include <windows.h>
#include <shlwapi.h>
#include <setupapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include "lib/wx_misc.h"
#include "driver_utility.h"
#include "direct_ks_driver.h"
#include "direct_ks_filter.h"
#include "direct_ks_pin.h"
#include "direct_ks_usb.h"

namespace {

#if !defined(USE_EXTRA_BUFFER_SIZE)
const UINT BUFLEN_MS_DEFAULT = 40;
#else // !defined(USE_EXTRA_BUFFER_SIZE)
// �~���b�P�ʂ̃o�b�t�@�T�C�Y
// WinXP��USB�I�[�f�B�I�h���C�o�ŕs����N����̂ŁA300ms�ʂ������Ǝv���B
// ���Ȃ݂ɁAVista�ȍ~��USB�I�[�f�B�I�h���C�o�͎d�l���ς�������߁A���Ȃ��B
const UINT BUFLEN_MS_DEFAULT = 250;	// For XP(!USB), Vista, or later
const UINT BUFLEN_MS_NT5USB0 = 80;	// For NT5x USB Audio
const UINT BUFLEN_MS_NT5USB1 = 65;	// For NT5x USB Audio (44.1kHz,24bit�`)
const UINT BUFLEN_MS_NT5USB2 = 50;	// For NT5x USB Audio (HD)
//const UINT MIN_WRITE_LEN_MS = 10;	// �ŒႱ�̕b�����o�͂���i�h���C�o�����ŕs����ł邱�Ƃւ̑Ώ��j
#endif // !defined(USE_EXTRA_BUFFER_SIZE)

const UINT BYTES_THRESHOLD1 = 256 * 1024;
const UINT BYTES_THRESHOLD2 = 512 * 1024;

// name��MAX_PATH�ȏ�̗e�ʂ����邱�ƁB
bool GetUSBDeviceFriendlyName(const TCHAR* device_path, TCHAR* name)
{
	TCHAR buf[] = { TEXT("0x\0\0\0\0\0") };

	int vender_id = 0;
	int product_id = 0;

	CopyMemory(&buf[2], &device_path[12], sizeof(TCHAR) * 4);
	StrToIntEx(buf, STIF_SUPPORT_HEX, &vender_id);

	CopyMemory(&buf[2], &device_path[21], sizeof(TCHAR) * 4);
	StrToIntEx(buf, STIF_SUPPORT_HEX, &product_id);

	bool result = GetUSBAudioDeviceName(vender_id, product_id, name);
	return result;
}

// �␳���K�v�ȃf�o�C�X���H
bool IsNeedCorrectDevice(const TCHAR* path)
{
	OSVERSIONINFO osi;

	ZeroMemory(&osi, sizeof(osi));
	osi.dwOSVersionInfoSize = sizeof(osi);

	GetVersionEx(&osi);
	bool is_nt5 = (osi.dwMajorVersion == 5);
	bool is_usb = IsUSBDevice(path);

	bool need_correct = (is_nt5 && is_usb);
	return need_correct;
}

} //namespace

DirectKsDriver::DirectKsDriver()
	: m_filter(NULL)
	, m_pin(NULL)
	, m_buffer(NULL)
	, m_index(0)
	, m_buf_size(0)
	, m_sec_size(1)
	, m_padding(0)
	, m_written(0)
{
	ZeroMemory(m_packet, sizeof(m_packet));

	if (!KsPin::InitKsUser()) {
		WX_TRACE("KsPin::InitKsUser() failed.\n");
	}
}

DirectKsDriver::~DirectKsDriver()
{
	KsPin::TermKsUser();
}

bool DirectKsDriver::EnumDevices()
{
	bool is_nt5 = false;

	// TODO: �o�͂�ʃX���b�h��������A�o�b�t�@�T�C�Y���������ł���̂ŕs�v�ɂȂ�B
	// NT5x�́AUSB�I�[�f�B�I�f�o�C�X�̏o�̓o�b�t�@�T�C�Y��傫���ł��Ȃ��̂ŁA
	// OS�̎�ނ𒲂ׂĂ����B(NT5x:Win2000/XP)
	{
		OSVERSIONINFO osi;

		ZeroMemory(&osi, sizeof(osi));
		osi.dwOSVersionInfoSize = sizeof(osi);

		GetVersionEx(&osi);
		is_nt5 = (osi.dwMajorVersion == 5);
	}

	DWORD detail_size = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (MAX_PATH * 2) * sizeof(TCHAR);
	void* detail_data = HeapAlloc(GetProcessHeap(), 0, detail_size);
	if (!detail_data) {
		return false;
	}

	const GUID dev_id = { STATIC_KSCATEGORY_AUDIO };
	const GUID subID = { STATIC_KSCATEGORY_RENDER };

	HDEVINFO di = SetupDiGetClassDevs(&dev_id, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (!di) {
		HeapFree(GetProcessHeap(), 0, detail_data);
		return false;
	}

	const TCHAR TYPE_PREFIX[] = TEXT("KS: ");
	const UINT TYPE_PREFIX_LEN = WX_LENGTHOF(TYPE_PREFIX) - 1;

	TCHAR device_name[MAX_PATH + TYPE_PREFIX_LEN];
	wchar_t* device_name_ptr = &device_name[TYPE_PREFIX_LEN];

	CopyMemory(device_name, TYPE_PREFIX, sizeof(TYPE_PREFIX));

	SP_DEVICE_INTERFACE_DETAIL_DATA* details = static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detail_data);
	SP_DEVICE_INTERFACE_DATA dev_if;

	for (DWORD i = 0; ; ++i) {
		ZeroMemory(&dev_if, sizeof(dev_if));
		dev_if.cbSize = sizeof(dev_if);
		dev_if.Reserved = 0;

		BOOL ret = SetupDiEnumDeviceInterfaces(di, NULL, &dev_id, i, &dev_if);
		if (!ret) {
			break;
		}

		HKEY key = SetupDiOpenDeviceInterfaceRegKey(di, &dev_if, 0, KEY_READ);
		if (key == INVALID_HANDLE_VALUE) {
			continue;
		}

		SP_DEVINFO_DATA info;

		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);

		details->cbSize = sizeof(*details);
		ret = SetupDiGetDeviceInterfaceDetail(di, &dev_if, details, detail_size, NULL, &info);
		if (!ret) {
			continue;
		}

		SP_DEVICE_INTERFACE_DATA alias;

		ZeroMemory(&alias, sizeof(alias));
		alias.cbSize = sizeof(alias);

		ret = SetupDiGetDeviceInterfaceAlias(di, &dev_if, &subID, &alias);
		if (!ret) {
			continue;
		}

		if (!alias.Flags || (alias.Flags & SPINT_REMOVED)) {
			continue;
		}

		// �s���̃f�[�^�𗬂������Ȃǂ��m�F�B
		// �o�͕����łȂ��Ƃ����Ȃ��̂ŁB
		if (!KsFilter::IsValidPin(details->DevicePath)) {
			continue;
		}

		*device_name_ptr = TEXT('\0');

		static const TCHAR* REG_VALUE = TEXT("FriendlyName");
		DWORD reg_type = REG_SZ;
		BYTE* reg_data = reinterpret_cast<BYTE*>(device_name_ptr);
		DWORD reg_size = sizeof(device_name) - TYPE_PREFIX_LEN * sizeof(device_name[0]);

		// ���W�X�g����FriendlyName�ɕ\���p�̖��O������̂ŁA������擾�B
		// Vista�ȍ~�͂������������Ă��邪�AXP�̓h���C�o�Ɉˑ�����݂����B
		LONG kret = RegQueryValueEx(key, REG_VALUE, 0, &reg_type, reg_data, &reg_size);
		RegCloseKey(key);

		// ���O�̂Ȃ��f�o�C�X�́A�I���ł��Ȃ��̂ŏ��O�B
		if (kret != ERROR_SUCCESS || *device_name_ptr == TEXT('\0')) {
			continue;
		}

		// USB�I�[�f�B�I�f�o�C�X�̏ꍇ�A"USB�I�[�f�B�I�f�o�C�X"�ɂȂ�̂ŁA�ʂ̕��@�Ŗ��O���擾
		// �������AVista�ȍ~�A���O�����W�X�g���ɕۑ�����悤�ɂȂ����l�Ȃ̂ŁAXP�̏ꍇ�����B
		bool is_usb = IsUSBDevice(details->DevicePath);
		if (is_nt5 && is_usb) {
			GetUSBDeviceFriendlyName(details->DevicePath, device_name_ptr);
		}

		StrTrim(device_name_ptr, TEXT(" "));

		AddDevice(device_name, details->DevicePath);
	}

	SetupDiDestroyDeviceInfoList(di);

	HeapFree(GetProcessHeap(), 0, detail_data);
	return true;
}

bool DirectKsDriver::Open(const wchar_t* guid, const WAVEFORMATEX& wfx)
{
	SetupVolume(wfx);

	KsFilter* filter = new KsFilter(guid);
	if (!filter) {
		WX_TRACE("�t�B���^�𐶐��ł��܂���I\n");
		return false;
	}

	KsPin* pin = filter->CreatePin(wfx);
	if (!pin) {
		WX_TRACE("�s���𐶐��ł��܂���I\n");
		delete filter;
		return false;
	}

	m_filter = filter;
	m_pin = pin;

#if 1
	// �\�t�g�E�F�A�{�����[���������������̂ŁA�o�b�t�@�T�C�Y�͍ŏ����ɉ�������B
	// �������A�f�R�[�h�X���b�h�ƃ����_�����O�X���b�h���������ꂽ��B
	// ����܂ł́A�����̂܂܁B
	UINT bufLenMS = BUFLEN_MS_DEFAULT;
#else
	UINT bufLenMS = BUFLEN_MS_DEFAULT;
	if (IsNeedCorrectDevice(guid)) {
		// �Đ����Ƀm�C�Y������̂ŁA�ׂ��������E�E�E
		if (BYTES_THRESHOLD2 < wft.Format.nAvgBytesPerSec) {
			bufLenMS = BUFLEN_MS_NT5USB2;
		}
		else if (BYTES_THRESHOLD1 < wft.Format.nAvgBytesPerSec) {
			bufLenMS = BUFLEN_MS_NT5USB1;
		}
		else {
			bufLenMS = BUFLEN_MS_NT5USB0;
		}
	}
#endif

	m_buf_size = MulDiv(wfx.nAvgBytesPerSec, bufLenMS, 1000);
	m_buf_size -= (m_buf_size % wfx.nBlockAlign);
	m_padding = 0;

	// minSize��NT5x/USB�̂݁B�i�ق��̊��ł͖��Ȃ��͗l���j
	// �������Ȃ��ƁA�Đ����I���Ȃ��Ȃ�f�o�C�X�i�h���C�o�j������̂ŁA�␳�B
	if (IsNeedCorrectDevice(guid)) {
		m_padding = m_buf_size;
	}

	m_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, m_buf_size * PACKET_NUM);
	if (!m_buffer) {
		Close();
		return false;
	}

	ZeroMemory(m_packet, sizeof(m_packet));

	for (UINT i = 0; i < WX_LENGTHOF(m_packet); ++i) {
		Packet& packet = m_packet[i];

		packet.header.Size = sizeof(packet.header);
		packet.header.PresentationTime.Numerator = 1;
		packet.header.PresentationTime.Denominator = 1;
		packet.header.FrameExtent = m_buf_size;
		packet.header.Data = AddOffset(m_buffer, i * m_buf_size);

		packet.signal.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!packet.signal.hEvent) {
			Close();
			return false;
		}

		ResetEvent(packet.signal.hEvent);
		packet.queued = false;
	}

	m_index = 0;

	m_sec_size = wfx.nAvgBytesPerSec;
	m_written = 0LL;

	m_pin->Reset();

	bool ret1 = m_pin->SetState(KSSTATE_ACQUIRE);
	bool ret2 = m_pin->SetState(KSSTATE_PAUSE);
	if (!ret1 || !ret2) {
		Close();
		return false;
	}

	return true;
}

void DirectKsDriver::Close()
{
	if (m_pin) {
		m_pin->SetState(KSSTATE_ACQUIRE);
		m_pin->SetState(KSSTATE_STOP);

		KsFilter::DestroyPin(m_pin);
		m_pin = NULL;
	}

	if (m_filter) {
		delete m_filter;
		m_filter = NULL;
	}

	for (UINT i = 0; i < WX_LENGTHOF(m_packet); ++i) {
		if (m_packet[i].signal.hEvent) {
			CloseHandle(m_packet[i].signal.hEvent);
		}
	}

	if (m_buffer) {
		HeapFree(GetProcessHeap(), 0, m_buffer);
		m_buffer = NULL;
	}

	ZeroMemory(&m_packet, sizeof(m_packet));

	m_index = 0;
	m_buf_size = 0;
	m_sec_size = 1;
	m_padding = 0;
	m_written = 0LL;
}

void DirectKsDriver::Reset()
{
	m_index = 0;
	m_written = 0;

	if (!m_pin) {
		WX_TRACE("DirectKsDriver::Reset() Pin is NULL!\n");
		return;
	}

	m_pin->SetState(KSSTATE_ACQUIRE);
	m_pin->SetState(KSSTATE_STOP);

	m_pin->Reset();
	Sleep(10);

	m_pin->SetState(KSSTATE_ACQUIRE);
	m_pin->SetState(KSSTATE_PAUSE);

	for (UINT i = 0; i < WX_LENGTHOF(m_packet); ++i) {
		ResetEvent(m_packet[i].signal.hEvent);
		m_packet[i].header.DataUsed = 0;
		m_packet[i].queued = false;
	}
}

bool DirectKsDriver::Start()
{
	if (!m_pin) {
		WX_TRACE("DirectKsDriver::Start() Pin is NULL!\n");
		return false;
	}

	HRESULT hr = m_pin->SetState(KSSTATE_RUN);
	if (FAILED(hr)) {
		WX_TRACE("[ERROR] �Đ��ł��܂���I hr=%x\n", hr);
		return false;
	}

	return true;
}

bool DirectKsDriver::Stop()
{
	if (!m_pin) {
		WX_TRACE("DirectKsDriver::Stop() Pin is NULL!\n");
		return false;
	}

	if (!m_pin->SetState(KSSTATE_PAUSE)) {
		WX_TRACE("��~�ł��܂���I\n");
		return false;
	}

	return true;
}

UINT DirectKsDriver::Write(const void* data, UINT size)
{
	Packet& packet = m_packet[m_index];
	if (packet.queued) 	{
		DWORD ret = WaitForSingleObject(packet.signal.hEvent, 0);
		if (ret == WAIT_FAILED) {
			WX_TRACE("WaitForSingleObject() failed. error = %d\n", GetLastError());
			return 0;
		}

		if (ret != WAIT_OBJECT_0) {
			return 0;
		}

		ResetEvent(packet.signal.hEvent);
		packet.header.DataUsed = 0;
		packet.queued = false;
	}

	UINT copy_size = size;
	UINT used_size = packet.header.DataUsed;
	if (m_buf_size < used_size + copy_size) {
		copy_size = m_buf_size - used_size;
	}

	if (0 < copy_size) {
		void* dest = AddOffset(packet.header.Data, used_size);
		//CopyMemory(dest, data, copy_size);
		ProcessVolume(dest, data, copy_size);
		packet.header.DataUsed += copy_size;

	}

	if (packet.header.DataUsed == m_buf_size) {
		WritePacket();
	}

	return copy_size;
}

bool DirectKsDriver::Flush()
{
	{
		Packet& packet = m_packet[m_index];
		KSSTREAM_HEADER& header = packet.header;

		// �܂��o�͂��ĂȂ��f�[�^���c���Ă�����A������o�͂���B
		if (!packet.queued && 0 < header.DataUsed) {
			UINT padding = 0;

			// �T�C�Y�����ɏ������ꍇ�A�n���O�A�b�v��I�������m�ł��Ȃ��h���C�o�ւ̑Ή�
			if (m_padding > header.DataUsed) {
				padding = m_padding - header.DataUsed;
				ZeroMemory(AddOffset(header.Data, header.DataUsed), padding);

				header.DataUsed = m_padding;
			}

			if (!WritePacket()) {
				return 0;
			}

			m_written -= padding;
		}
	}

	UINT left_size = 0;
	for (UINT i = 0; i < WX_LENGTHOF(m_packet); ++i) {
		Packet& packet = m_packet[i];
		if (packet.queued) {
			DWORD ret = WaitForSingleObject(packet.signal.hEvent, 0);
			if (ret == WAIT_FAILED) {
				WX_TRACE("WaitForSingleObject() failed. error = %d\n", GetLastError());
				return 0;
			}

			if (ret == WAIT_OBJECT_0) {
				ResetEvent(packet.signal.hEvent);
				packet.header.DataUsed = 0;
				packet.queued = false;
			}
			else {
				left_size = packet.header.DataUsed;
			}
		}
	}

	// �C�x���g�ŏI�����o�ł��Ȃ��f�o�C�X�E�h���C�o�ւ̑Ή�
	if (0 < left_size) {
		KSAUDIO_POSITION pos;
		bool ret = m_pin->GetPosition(pos);
		if (!ret) {
			return 0;
		}

		if (m_written <= pos.PlayOffset) {
			return 0;
		}

		left_size = static_cast<UINT>(m_written - pos.PlayOffset);
	}

	return left_size != 0;
}

UINT DirectKsDriver::GetPlayTime() const
{
	if (!m_pin) {
		return 0;
	}

	KSAUDIO_POSITION pos;
	bool ret = m_pin->GetPosition(pos);
	if (!ret) {
		return 0;
	}

	UINT64 played_size_ms = static_cast<UINT64>(1000) * pos.PlayOffset;
	return static_cast<UINT>(played_size_ms / m_sec_size);
}

bool DirectKsDriver::GetFormat(WAVEFORMATEX& /*wfx*/) const
{
	return false;
}

bool DirectKsDriver::WritePacket()
{
	Packet& packet = m_packet[m_index];
	bool ret = m_pin->WriteData(packet.header, packet.signal);
	if (!ret) {
		WX_TRACE("[ERROR] �f�[�^�̏o�͂Ɏ��s���܂����I\n");
		return false;
	}

	packet.queued = true;
	m_written += static_cast<UINT64>(packet.header.DataUsed);

	if (++m_index >= PACKET_NUM) {
		m_index = 0;
	}

	return true;
}
