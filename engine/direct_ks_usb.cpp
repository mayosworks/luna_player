//=============================================================================
// USB Audio関係
//=============================================================================

#define INITGUID

#include <windows.h>
#include <setupapi.h>
#include "usbioctl.h"
#include "usbdesc.h"
#include "direct_ks_usb.h"

// 定義
namespace {

// 定数定義
const USHORT DEFAULT_LANG_ID = 0x0409;

// 関数定義
bool EnumerateHostController(UINT vender_id, UINT product_id, wchar_t* name);
bool EnumerateHub(const TCHAR* hub_name, UINT vender_id, UINT product_id, wchar_t* name);
bool EnumerateHubPorts(HANDLE hub, ULONG num_ports, UINT vender_id, UINT product_id, wchar_t* name);

bool GetRootHubName(HANDLE host, TCHAR* ext_hub_name);
bool GetExternalHubName(HANDLE hub, ULONG index, PTSTR ext_hub_name);

PUSB_DESCRIPTOR_REQUEST GetConfigDescriptor(HANDLE hub, ULONG conn_index, UCHAR desc_index);
bool GetStringDescriptor(HANDLE hub, ULONG conn_index, UCHAR desc_index, USHORT lang_id, wchar_t* data);
USHORT GetFirstStringLanguageId(HANDLE hub, ULONG conn_index);
bool GetAudioControlInterfaceString(HANDLE hub, ULONG conn_index, PUSB_CONFIGURATION_DESCRIPTOR config_desc, USHORT lang_id, wchar_t* data);

//-----------------------------------------------------------------------------
// ホストコントローラーを列挙
//-----------------------------------------------------------------------------
bool EnumerateHostController(UINT vender_id, UINT product_id, wchar_t* name)
{
	HDEVINFO dev_info = SetupDiGetClassDevs(&GUID_CLASS_USB_HOST_CONTROLLER,
		NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (dev_info == INVALID_HANDLE_VALUE) {
		return false;
	}

	SP_DEVICE_INTERFACE_DATA info_data;
	info_data.cbSize = sizeof(info_data);

	bool ret_val = false;
	for (ULONG i = 0; !ret_val; ++i) {
		if (!SetupDiEnumDeviceInterfaces(dev_info, 0, &GUID_CLASS_USB_HOST_CONTROLLER, i, &info_data)) {
			break;
		}

		ULONG requiredLength = 0;
		SetupDiGetDeviceInterfaceDetail(dev_info, &info_data, NULL, 0, &requiredLength, NULL);

		PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredLength));
		if (!detail_data) {
			break;
		}

		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		SetupDiGetDeviceInterfaceDetail(dev_info, &info_data, detail_data, requiredLength, &requiredLength, NULL);

		HANDLE host = CreateFile(detail_data->DevicePath,
			GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (host != INVALID_HANDLE_VALUE) {
			TCHAR root_hub_name[MAX_PATH];
			if (GetRootHubName(host, root_hub_name)) {
				ret_val = EnumerateHub(root_hub_name, vender_id, product_id, name);
			}

			CloseHandle(host);
		}

		HeapFree(GetProcessHeap(), 0, detail_data);
	}

	SetupDiDestroyDeviceInfoList(dev_info);
	return ret_val;
}

//-----------------------------------------------------------------------------
// 接続しているハブを列挙
//-----------------------------------------------------------------------------
bool EnumerateHub(const TCHAR* hub_name, UINT vender_id, UINT product_id, wchar_t* name)
{
	TCHAR hub_path[MAX_PATH];
	wsprintf(hub_path, TEXT("\\\\.\\%s"), hub_name);

	HANDLE hub = CreateFile(hub_path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hub == INVALID_HANDLE_VALUE) {
		return false;
	}

	USB_NODE_INFORMATION hub_info;
	ZeroMemory(&hub_info, sizeof(hub_info));

	DWORD bytes = 0;
	BOOL ret = DeviceIoControl(hub, IOCTL_USB_GET_NODE_INFORMATION,
		&hub_info, sizeof(hub_info), &hub_info, sizeof(hub_info), &bytes, NULL);

	bool ret_val = false;
	if (ret) {
		ULONG num_ports = hub_info.u.HubInformation.HubDescriptor.bNumberOfPorts;
		ret_val = EnumerateHubPorts(hub, num_ports, vender_id, product_id, name);
	}

	CloseHandle(hub);
	return ret_val;
}

//-----------------------------------------------------------------------------
// ポートにつながるデバイスを列挙
//-----------------------------------------------------------------------------
bool EnumerateHubPorts(HANDLE hub, ULONG num_ports, UINT vender_id, UINT product_id, wchar_t* name)
{
	DWORD info_size = sizeof(USB_NODE_CONNECTION_INFORMATION) + sizeof(USB_PIPE_INFO) * 30;
	void* info_data = HeapAlloc(GetProcessHeap(), 0, info_size);
	if (!info_data) {
		return false;
	}


	DWORD info_ex_size = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;
	void* info_ex_data = HeapAlloc(GetProcessHeap(), 0, info_ex_size);
	if (!info_ex_data) {
		HeapFree(GetProcessHeap(), 0, info_data);
		return false;
	}

	PUSB_NODE_CONNECTION_INFORMATION_EX info_ex = static_cast<PUSB_NODE_CONNECTION_INFORMATION_EX>(info_ex_data);
	PUSB_NODE_CONNECTION_INFORMATION info = static_cast<PUSB_NODE_CONNECTION_INFORMATION>(info_data);

	bool ret_val = false;

	//MEMO:
	// エンドポイントの番号は０～１５。ただし、０は標準コントロールエンドポイントなので、
	// １～１５を使う。INとOUTで合計３０エンドポイントが最大。詳しくはUSBの仕様書を。
	for (ULONG i = 1; i <= num_ports; ++i) {
		info_ex->ConnectionIndex = i;

		DWORD bytes;
		BOOL ret = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
						info_ex, info_ex_size, info_ex, info_ex_size, &bytes, NULL);
		if (!ret) {
			info->ConnectionIndex = i;

			ret = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
						info, info_size, info, info_size, &bytes, NULL);
			if (!ret) {
				continue;
			}

			if (info->ConnectionStatus == DeviceConnected) {
				info_ex->ConnectionIndex				= info->ConnectionIndex;
				info_ex->DeviceDescriptor			= info->DeviceDescriptor;
				info_ex->CurrentConfigurationValue	= info->CurrentConfigurationValue;
				info_ex->Speed						= static_cast<UCHAR>(info->LowSpeed ? UsbLowSpeed : UsbFullSpeed);
				info_ex->DeviceIsHub					= info->DeviceIsHub;
				info_ex->DeviceAddress				= info->DeviceAddress;
				info_ex->NumberOfOpenPipes			= info->NumberOfOpenPipes;
				info_ex->ConnectionStatus			= info->ConnectionStatus;

				CopyMemory(&info_ex->PipeList[0], &info->PipeList[0], sizeof(USB_PIPE_INFO) * 30);
			}
		}

		//未接続なら何もしない
		if (info_ex->ConnectionStatus != DeviceConnected) {
			continue;
		}

		// ハブなら、ぶら下がっているデバイスを再列挙。
		if (info_ex->DeviceIsHub) {
			TCHAR ext_hub_name[MAX_PATH];
			if (GetExternalHubName(hub, i, ext_hub_name)) {
				if (EnumerateHub(ext_hub_name, vender_id, product_id, name)) {
					break;
				}
			}

			continue;
		}

		// 目的のデバイスか？
		if (info_ex->DeviceDescriptor.idVendor != vender_id
		 || info_ex->DeviceDescriptor.idProduct != product_id) {
			continue;
		}

		// 構成デスクリプターをとれないなら何もせず。
		PUSB_DESCRIPTOR_REQUEST config_desc = GetConfigDescriptor(hub, i, 0);
		if (!config_desc) {
			return false;
		}

		USHORT lang_id = GetFirstStringLanguageId(hub, i);
		PUSB_CONFIGURATION_DESCRIPTOR cfg_desc = reinterpret_cast<PUSB_CONFIGURATION_DESCRIPTOR>(config_desc + 1);

		// 出力の名前がとれたらOK
		if (GetAudioControlInterfaceString(hub, i, cfg_desc, lang_id, name)) {
			HeapFree(GetProcessHeap(), 0, config_desc);
			ret_val = true;
			break;
		}

		// 製品名をかわりに取得
		if (info_ex->DeviceDescriptor.iProduct) {
			if (GetStringDescriptor(hub, i, info_ex->DeviceDescriptor.iProduct, lang_id, name)) {
				HeapFree(GetProcessHeap(), 0, config_desc);
				ret_val = true;
				break;
			}
		}

		HeapFree(GetProcessHeap(), 0, config_desc);
		break;
	}

	HeapFree(GetProcessHeap(), 0, info_ex_data);
	HeapFree(GetProcessHeap(), 0, info_data);
	return ret_val;
}

//-----------------------------------------------------------------------------
// ルートハブ名を取得
//-----------------------------------------------------------------------------
bool GetRootHubName(HANDLE host, TCHAR* root_hub_name)
{
	//ノード名サイズを固定でとるための構造体。
	struct USB_ROOT_HUB_NAME_MAXLEN
	{
		USB_ROOT_HUB_NAME data;
		WCHAR buf[MAX_PATH];
	};

	USB_ROOT_HUB_NAME_MAXLEN hub_name;

	DWORD bytes = 0;
	BOOL ret = DeviceIoControl(host, IOCTL_USB_GET_ROOT_HUB_NAME,
		NULL, 0, &hub_name, sizeof(hub_name), &bytes, NULL);
	if (!ret) {
		return false;
	}

	// このサイズ分はとれているはず。
	if (bytes < sizeof(USB_ROOT_HUB_NAME)) {
		return false;
	}

	lstrcpyn(root_hub_name, hub_name.data.RootHubName, MAX_PATH);
	return true;
}

//-----------------------------------------------------------------------------
// 外部ハブ名を取得
//-----------------------------------------------------------------------------
bool GetExternalHubName(HANDLE hub, ULONG /*index*/, PTSTR ext_hub_name)
{
	//ノード名サイズを固定でとるための構造体。
	struct USB_NODE_CONNECTION_NAME_MAXLEN
	{
		USB_NODE_CONNECTION_NAME	data;
		WCHAR						buf[MAX_PATH];
	};

	USB_NODE_CONNECTION_NAME_MAXLEN hub_name;

	DWORD bytes = 0;
	BOOL ret = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_NAME,
		&hub_name, sizeof(hub_name), &hub_name, sizeof(hub_name), &bytes, NULL);
	if (!ret) {
		return false;
	}

	// このサイズ分はとれているはず。
	if (bytes < sizeof(USB_NODE_CONNECTION_NAME)) {
		return false;
	}

	lstrcpyn(ext_hub_name, hub_name.data.NodeName, MAX_PATH);
	return true;
}

//-----------------------------------------------------------------------------
// 構成デスクリプターを取得
//-----------------------------------------------------------------------------
PUSB_DESCRIPTOR_REQUEST GetConfigDescriptor(HANDLE hub, ULONG conn_index, UCHAR desc_index)
{
	ULONG bytes;
	ULONG bytes_returned;

	BYTE config_desc_req_buf[sizeof(USB_DESCRIPTOR_REQUEST) + sizeof(USB_CONFIGURATION_DESCRIPTOR)];
	ZeroMemory(config_desc_req_buf, sizeof(config_desc_req_buf));

	PUSB_DESCRIPTOR_REQUEST config_desc_req;
	PUSB_CONFIGURATION_DESCRIPTOR config_desc;

	config_desc_req = (PUSB_DESCRIPTOR_REQUEST)config_desc_req_buf;
	config_desc = (PUSB_CONFIGURATION_DESCRIPTOR)(config_desc_req + 1);

	config_desc_req->ConnectionIndex = conn_index;

	bytes = sizeof(config_desc_req_buf);

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	config_desc_req->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8) | desc_index;
	config_desc_req->SetupPacket.wLength = (USHORT)(bytes - sizeof(USB_DESCRIPTOR_REQUEST));


	BOOL ret = DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		config_desc_req, bytes, config_desc_req, bytes, &bytes_returned, NULL);
	if (!ret) {
		return NULL;
	}

	if (bytes != bytes_returned) {
		return NULL;
	}

	if (config_desc->wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
		return NULL;
	}

	bytes = sizeof(USB_DESCRIPTOR_REQUEST) + config_desc->wTotalLength;

	config_desc_req = (PUSB_DESCRIPTOR_REQUEST)HeapAlloc(GetProcessHeap(), 0, bytes);
	if (!config_desc_req) {
		return NULL;
	}

	config_desc = (PUSB_CONFIGURATION_DESCRIPTOR)(config_desc_req + 1);
	config_desc_req->ConnectionIndex = conn_index;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	config_desc_req->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8) | desc_index;
	config_desc_req->SetupPacket.wLength = (USHORT)(bytes - sizeof(USB_DESCRIPTOR_REQUEST));

	ret = DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		config_desc_req, bytes, config_desc_req, bytes, &bytes_returned, NULL);
	if (!ret) {
		HeapFree(GetProcessHeap(), 0, config_desc_req);
		return NULL;
	}

	if (bytes != bytes_returned) {
		HeapFree(GetProcessHeap(), 0, config_desc_req);
		return NULL;
	}

	if (config_desc->wTotalLength != (bytes - sizeof(USB_DESCRIPTOR_REQUEST))) {
		HeapFree(GetProcessHeap(), 0, config_desc_req);
		return NULL;
	}

	return config_desc_req;
}

//-----------------------------------------------------------------------------
// 文字デスクリプターを取得
//-----------------------------------------------------------------------------
bool GetStringDescriptor(HANDLE hub, ULONG conn_index, UCHAR desc_index, USHORT lang_id, wchar_t* data)
{
	struct USB_DESCRIPTOR_REQUEST_MAXLEN
	{
		//USB_DESCRIPTOR_REQUEST	req;
		BYTE					req[sizeof(USB_DESCRIPTOR_REQUEST)];
		USB_STRING_DESCRIPTOR	data;
		WCHAR					buf[MAXIMUM_USB_STRING_LENGTH];
	};

	USB_DESCRIPTOR_REQUEST_MAXLEN string_desc;

	ZeroMemory(&string_desc, sizeof(string_desc));

	USB_DESCRIPTOR_REQUEST* req = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(string_desc.req);
	req->ConnectionIndex		= conn_index;
	req->SetupPacket.wValue		= ((USB_STRING_DESCRIPTOR_TYPE << 8) | desc_index);
	req->SetupPacket.wIndex		= lang_id;
	req->SetupPacket.wLength	= sizeof(string_desc) - sizeof(USB_DESCRIPTOR_REQUEST);

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//

	DWORD bytes = 0;
	BOOL ret = DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		&string_desc, sizeof(string_desc), &string_desc, sizeof(string_desc), &bytes, NULL);
	if (!ret || bytes < 2) {
		return false;
	}

	if (string_desc.data.bDescriptorType != USB_STRING_DESCRIPTOR_TYPE) {
		return false;
	}

	if (string_desc.data.bLength != bytes - sizeof(USB_DESCRIPTOR_REQUEST)) {
		return false;
	}

	if (string_desc.data.bLength % 2 != 0) {
		return false;
	}

	size_t copy_size = sizeof(wchar_t) * (MAX_PATH - 1);
	if (string_desc.data.bLength < copy_size) {
		copy_size = string_desc.data.bLength;
	}

	data[copy_size] = L'\0';

	CopyMemory(data, string_desc.data.bString, string_desc.data.bLength);
	return true;
}

//-----------------------------------------------------------------------------
// 文字の言語IDを取得
//-----------------------------------------------------------------------------
USHORT GetFirstStringLanguageId(HANDLE hub, ULONG conn_index)
{
	wchar_t data[MAX_PATH];
	if (!GetStringDescriptor(hub, conn_index, 0, 0, data)) {
		return DEFAULT_LANG_ID;
	}

	return static_cast<USHORT>(data[0]);
}

//-----------------------------------------------------------------------------
// オーディオコントロールインターフェイスの名前を取得
//-----------------------------------------------------------------------------
bool GetAudioControlInterfaceString(HANDLE hub, ULONG conn_index, PUSB_CONFIGURATION_DESCRIPTOR config_desc, USHORT lang_id, wchar_t* data)
{
	// Check the Configuration and Interface Descriptor strings
	BYTE* desc_ptr = reinterpret_cast<BYTE*>(config_desc);
	BYTE* desc_end = desc_ptr + config_desc->wTotalLength;

	while(desc_ptr + sizeof(USB_COMMON_DESCRIPTOR) < desc_end &&
		desc_ptr + reinterpret_cast<PUSB_COMMON_DESCRIPTOR>(desc_ptr)->bLength <= desc_end) {
		PUSB_COMMON_DESCRIPTOR commonDesc = reinterpret_cast<PUSB_COMMON_DESCRIPTOR>(desc_ptr);
		if (commonDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR)
			 && commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2)) {
				return false;
			}

			PUSB_INTERFACE_DESCRIPTOR ifDesc = reinterpret_cast<PUSB_INTERFACE_DESCRIPTOR>(desc_ptr);
			if (ifDesc->bInterfaceClass == USB_DEVICE_CLASS_AUDIO
			 && ifDesc->bInterfaceSubClass == USB_AUDIO_SUBCLASS_AUDIOCONTROL
			 && ifDesc->iInterface > 0) {
				return GetStringDescriptor(hub, conn_index, ifDesc->iInterface, lang_id, data);
			}
		}

		desc_ptr += commonDesc->bLength;
	}

	return false;
}

} //namespace

//-----------------------------------------------------------------------------
// USBデバイスかどうか？
//-----------------------------------------------------------------------------
bool IsUSBDevice(const wchar_t* path)
{
	TCHAR buf[16];

	lstrcpyn(buf, path, 9);
	return (lstrcmpi(buf, TEXT("\\\\?\\usb#")) == 0);
}

//-----------------------------------------------------------------------------
// デバイス名取得
//-----------------------------------------------------------------------------
bool GetUSBAudioDeviceName(UINT vender_id, UINT product_id, wchar_t* name)
{
	return EnumerateHostController(vender_id, product_id, name);
}
