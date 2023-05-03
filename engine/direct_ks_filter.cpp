//=============================================================================
// KernelStreaming / Filter
//=============================================================================

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include "lib/wx_misc.h"
#include "direct_ks_pin.h"
#include "direct_ks_filter.h"

//-----------------------------------------------------------------------------
// 内部関数プロトタイプ宣言
//-----------------------------------------------------------------------------
static KSDATARANGE_AUDIO* GetDataRangeAudio(KSDATARANGE* data_range);
static bool IsFormatInRange(KSDATARANGE* data_range, const WAVEFORMATEX& wfx);

//-----------------------------------------------------------------------------
// オーディオ（Wave）フィルタかどうかのチェック
//-----------------------------------------------------------------------------
bool KsFilter::IsValidPin(const TCHAR* filter_name)
{
	KsFilter filter(filter_name);
	if (!filter.IsValid()) {
		return false;
	}

	ULONG num_pins = filter.GetPinNum();
	for (ULONG i = 0; i < num_pins; ++i) {
		if (filter.IsPinRenderer(i) && filter.IsPinViable(i) && filter.HasDataRange(i)) {
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
KsFilter::KsFilter(const TCHAR* filter_name)
{
	HANDLE device = CreateFile(filter_name, GENERIC_READ | GENERIC_WRITE, 0,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	if (device == INVALID_HANDLE_VALUE) {
		WX_TRACE("Create Filter failed. Device = %s. Error = 0x%08x", filter_name, GetLastError());
	}

	SetHandle(device);
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
KsFilter::~KsFilter()
{
}

//-----------------------------------------------------------------------------
// ピン生成
//-----------------------------------------------------------------------------
KsPin* KsFilter::CreatePin(const WAVEFORMATEX& wfx)
{
	if (!IsValid()) {
		return NULL;
	}

	ULONG num_pins = GetPinNum();
	for(ULONG i = 0; i < num_pins; ++i) {
		if (IsPinRenderer(i) && IsPinViable(i) && IsFormatSupported(i, wfx)) {
			KsPin* pin = new KsPin(GetHandle(), i, wfx);
			if (pin && pin->IsValid()) {
				return pin;
			}

			delete pin;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// ピン破棄
//-----------------------------------------------------------------------------
void KsFilter::DestroyPin(KsPin* pin)
{
	delete pin;
}

//-----------------------------------------------------------------------------
// ピンの単一プロパティ取得
//-----------------------------------------------------------------------------
bool KsFilter::GetPinPropertySimple(ULONG pin_id, REFGUID set, ULONG id, void* data, ULONG size)
{
	KSP_PIN prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Property.Set = set;
	prop.Property.Id = id;
	prop.Property.Flags = KSPROPERTY_TYPE_GET;
	prop.PinId = pin_id;

	return SyncIoctl(IOCTL_KS_PROPERTY, &prop, sizeof(prop), data, size);
}

//-----------------------------------------------------------------------------
// ピンの複数プロパティ取得
//-----------------------------------------------------------------------------
bool KsFilter::GetPinPropertyMulti(ULONG pin_id, REFGUID set, ULONG id, KSMULTIPLE_ITEM** item)
{
	KSP_PIN prop;
	ZeroMemory(&prop, sizeof(prop));

	prop.Property.Set = set;
	prop.Property.Id = id;
	prop.Property.Flags = KSPROPERTY_TYPE_GET;
	prop.PinId = pin_id;

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
// ピン数取得
//-----------------------------------------------------------------------------
ULONG KsFilter::GetPinNum()
{
	// ピンとは、スピーカ等の端子を指すもの。
	ULONG num_pins = 0;
	if (!GetPinPropertySimple(0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &num_pins, sizeof(num_pins))) {
		WX_TRACE("[ERROR] 出力ピン数を取得できません。\n");
		return 0;
	}

	return num_pins;
}

//-----------------------------------------------------------------------------
// ピンはレンダラー？
//-----------------------------------------------------------------------------
bool KsFilter::IsPinRenderer(ULONG pin_id)
{
	KSPIN_DATAFLOW flow;
	if (!GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &flow, sizeof(flow))) {
		return false;
	}

	// Pinに入力であること
	if (flow != KSPIN_DATAFLOW_IN) {
		return false;
	}

	KSPIN_COMMUNICATION comm;
	if (!GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &comm, sizeof(comm))) {
		return false;
	}

	// SINK or BOTH
	return (comm == KSPIN_COMMUNICATION_SINK || comm == KSPIN_COMMUNICATION_BOTH);
}

//-----------------------------------------------------------------------------
// ピンは使用可能？
//-----------------------------------------------------------------------------
bool KsFilter::IsPinViable(ULONG pin_id)
{
	bool viable = false;
	KSMULTIPLE_ITEM* mi_interfaces = NULL;
	if (!GetPinPropertyMulti(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_INTERFACES, &mi_interfaces)) {
		return false;
	}

	KSPIN_INTERFACE* pin_ifs = reinterpret_cast<KSPIN_INTERFACE*>(mi_interfaces + 1);
	viable = false;
	for(ULONG i = 0; i < mi_interfaces->Count && !viable; ++i) {
		// KSINTERFACE_STANDARD_LOOPED_STREAMING(WaveRT)はサポートしない。
		viable |= (InlineIsEqualGUID(pin_ifs[i].Set, KSINTERFACESETID_Standard)
			&& (pin_ifs[i].Id == KSINTERFACE_STANDARD_STREAMING));
	}

	MemFree(mi_interfaces);
	if (!viable) {
		return false;
	}

	KSMULTIPLE_ITEM* mi_mediums = NULL;
	if (!GetPinPropertyMulti(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_MEDIUMS, &mi_mediums)) {
		return false;
	}

	KSPIN_MEDIUM* pin_mediums = reinterpret_cast<KSPIN_MEDIUM*>(mi_mediums + 1);
	viable = false;
	for(ULONG i = 0; i < mi_interfaces->Count && !viable; ++i) {
		viable |= (IsEqualGUID(pin_mediums[i].Set, KSMEDIUMSETID_Standard)
			&& (pin_mediums[i].Id == KSMEDIUM_STANDARD_DEVIO));
	}

	MemFree(mi_mediums);
	if (!viable) {
		return false;
	}

#ifdef _DEBUG
	TCHAR name[MAX_PATH];
	if (GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_NAME, &name, sizeof(name) - 1)) {
		WX_TRACE("PinID[%d]'s PinName:%ws\n", pin_id, name);
	}
#endif //_DEBUG

#if 0
	KSPIN_CINSTANCES cinstances;
	hr = filter->GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_CINSTANCES, &cinstances, sizeof(cinstances));
	if (FAILED(hr)) {
		WX_TRACE("Failed to retrieve pin property KSPROPERTY_PIN_CINSTANCES on PinID: %d", pin_id);
		return false;
	}

	/*KSPIN_CINSTANCES cinstancesGlobal;
	hr = filter->GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_GLOBALCINSTANCES, &cinstancesGlobal, sizeof(cinstancesGlobal));
	if (FAILED(hr)) {
		WX_TRACE("Failed to retrieve pin property KSPROPERTY_PIN_GLOBALCINSTANCES on PinID: %d", pin_id);
		return false;
	}*/

	GUID category;
	hr = filter->GetPinPropertySimple(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_CATEGORY, &category, sizeof(category));
	if (FAILED(hr)) {
		WX_TRACE("Failed to retrieve pin property KSPROPERTY_PIN_CATEGORY on PinID: %d", pin_id);
		return false;
	}
#endif //0

	return true;
}

//-----------------------------------------------------------------------------
// データレンジを持っているか？
//-----------------------------------------------------------------------------
bool KsFilter::HasDataRange(ULONG pin_id)
{
	KSMULTIPLE_ITEM* mi_data_ranges = NULL;
	if (!GetPinPropertyMulti(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_DATARANGES, &mi_data_ranges)) {
		return false;
	}

	KSDATARANGE* data_ranges = reinterpret_cast<KSDATARANGE*>(mi_data_ranges + 1);
	bool result = false;
	for(ULONG i = 0; i < mi_data_ranges->Count && !result; ++i) {
		KSDATARANGE_AUDIO* data_range_audio = GetDataRangeAudio(data_ranges);
		if (data_range_audio) {
			const GUID GUID_WAVE_FORMAT_EX = { DEFINE_WAVEFORMATEX_GUID(WAVE_FORMAT_PCM) };

			bool chk = true;

			if (!InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, KSDATAFORMAT_SUBTYPE_WILDCARD)
			 && !InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, GUID_WAVE_FORMAT_EX)
			 && !InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, KSDATAFORMAT_SUBTYPE_PCM)
			 /*&& !InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)*/) {
				chk = false;
			}

			if (!InlineIsEqualGUID(data_range_audio->DataRange.Specifier, KSDATAFORMAT_SPECIFIER_WILDCARD)
			 && !InlineIsEqualGUID(data_range_audio->DataRange.Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) {
				chk = false;
			}

			result |= chk;
		}

		data_ranges = reinterpret_cast<KSDATARANGE*>(reinterpret_cast<BYTE*>(data_ranges) + data_ranges->FormatSize);
	}

	MemFree(mi_data_ranges);
	return result;
}

//-----------------------------------------------------------------------------
// 指定フォーマットに対応？
//-----------------------------------------------------------------------------
bool KsFilter::IsFormatSupported(ULONG pin_id, const WAVEFORMATEX& wfx)
{
	KSMULTIPLE_ITEM* mi_data_ranges = NULL;
	if (!GetPinPropertyMulti(pin_id, KSPROPSETID_Pin, KSPROPERTY_PIN_DATARANGES, &mi_data_ranges)) {
		return false;
	}

	KSDATARANGE* data_ranges = reinterpret_cast<KSDATARANGE*>(mi_data_ranges + 1);
	bool found = false;
	for(ULONG i = 0; i < mi_data_ranges->Count && !found; ++i) {
		found = IsFormatInRange(data_ranges, wfx);
		data_ranges = reinterpret_cast<KSDATARANGE*>(reinterpret_cast<BYTE*>(data_ranges) + data_ranges->FormatSize);
    }

	MemFree(mi_data_ranges);
	return found;
}

//-----------------------------------------------------------------------------
// オーディオレンジを取得
//-----------------------------------------------------------------------------
KSDATARANGE_AUDIO* GetDataRangeAudio(KSDATARANGE* data_range)
{
	if (!IS_VALID_WAVEFORMATEX_GUID(&data_range->SubFormat)
	 && !InlineIsEqualGUID(data_range->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)
	 && !InlineIsEqualGUID(data_range->SubFormat, KSDATAFORMAT_SUBTYPE_WILDCARD)) {
		return NULL;
	}

	if (!InlineIsEqualGUID(data_range->MajorFormat, KSDATAFORMAT_TYPE_AUDIO)
	 && !InlineIsEqualGUID(data_range->MajorFormat, KSDATAFORMAT_TYPE_WILDCARD)) {
		return NULL;
	}

	return reinterpret_cast<KSDATARANGE_AUDIO*>(data_range);
}

//-----------------------------------------------------------------------------
// 指定フォーマットは範囲内？
//-----------------------------------------------------------------------------
bool IsFormatInRange(KSDATARANGE* data_range, const WAVEFORMATEX& wfx)
{
	KSDATARANGE_AUDIO* data_range_audio = GetDataRangeAudio(data_range);
	if (!data_range_audio) {
		return NULL;
	}

	GUID guid_format = { DEFINE_WAVEFORMATEX_GUID(wfx.wFormatTag) };
	if (WAVE_FORMAT_EXTENSIBLE == wfx.wFormatTag) {
		guid_format = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(wfx).SubFormat;
	}

	if (!InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, KSDATAFORMAT_SUBTYPE_WILDCARD)
	 && !InlineIsEqualGUID(data_range_audio->DataRange.SubFormat, guid_format)) {
		return false;
	}

	if (!InlineIsEqualGUID(data_range_audio->DataRange.Specifier, KSDATAFORMAT_SPECIFIER_WILDCARD)
	 && !InlineIsEqualGUID(data_range_audio->DataRange.Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) {
		return false;
	}

	// 要求フォーマット範囲内であることを確認
	if (data_range_audio->MaximumChannels >= wfx.nChannels
	 && data_range_audio->MinimumBitsPerSample <= wfx.wBitsPerSample
	 && data_range_audio->MaximumBitsPerSample >= wfx.wBitsPerSample
	 && data_range_audio->MinimumSampleFrequency <= wfx.nSamplesPerSec
	 && data_range_audio->MaximumSampleFrequency >= wfx.nSamplesPerSec) {
		return true;
	}

	return false;
}
