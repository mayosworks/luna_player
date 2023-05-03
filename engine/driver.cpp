//=============================================================================
// サウンドドライバ共通
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "driver.h"
#include "driver_manager.h"
#include "driver_utility.h"

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Driver::Driver()
	: m_volume(100)
	, m_out_bits(16)
	, m_conversion(PASS_THROUGH)
	, m_is_float(false)
{
}

//-----------------------------------------------------------------------------
// ボリューム設定、0-100%で設定
//-----------------------------------------------------------------------------
void Driver::SetVolume(int volume)
{
	m_volume = volume;
}

//-----------------------------------------------------------------------------
// デバイス追加
//-----------------------------------------------------------------------------
void Driver::AddDevice(const wchar_t* name, const wchar_t* guid)
{
	DriverManager::AddDevice(this, name, guid);
}

//-----------------------------------------------------------------------------
// ボリューム設定初期化
//-----------------------------------------------------------------------------
void Driver::SetupVolume(const WAVEFORMATEX& wfx)
{
	m_out_bits = wfx.wBitsPerSample;
	m_is_float = (wfx.wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
}

//-----------------------------------------------------------------------------
// ボリューム制御
//-----------------------------------------------------------------------------
void Driver::ProcessVolume(void* dest, const void* data, UINT size) const
{
	int volume = m_volume;
	if ((m_conversion == PASS_THROUGH) && (volume >= 100)) {
		CopyMemory(dest, data, size);
		return;
	}

	if (m_is_float) {
#if 0
		if (m_out_bits == 32) {
			const float* data_ptr = static_cast<const float*>(data);
			float* dest_ptr = static_cast<float*>(dest);
			for (UINT i = 0; i < size / 4; ++i) {
				dest_ptr[i] = data_ptr[i] * float(volume) / 100.0f;
			}
		}
#endif //0
	}
	else {
		if (m_conversion == INT8_TO_INT16) {
		}
		else if (m_conversion == INT8_TO_INT32) {
		}
		else if (m_conversion == INT16_TO_INT32) {
			const short* data_ptr = static_cast<const short*>(data);
			int* dest_ptr = static_cast<int*>(dest);
			for (UINT i = 0; i < size / 4; ++i) {
				dest_ptr[i] = int(data_ptr[i]) * 65536 / 100 * volume;
			}
		}
		else if (m_conversion == INT24_TO_INT32) {
			const BYTE* data_ptr = static_cast<const BYTE*>(data);
			int* dest_ptr = static_cast<int*>(dest);
			for (UINT i = 0; i < size / 4; ++i) {
				int value = GetInt24Value(data_ptr + i * 3);

				dest_ptr[i] = value / 100 * volume;
			}
		}
		else if (m_out_bits == 32) {
			const int* data_ptr = static_cast<const int*>(data);
			int* dest_ptr = static_cast<int*>(dest);
			for (UINT i = 0; i < size / 4; ++i) {
				dest_ptr[i] = data_ptr[i] / 100 * volume;
			}
		}
		else if (m_out_bits == 24) {
			const BYTE* data_ptr = static_cast<const BYTE*>(data);
			BYTE* dest_ptr = static_cast<BYTE*>(dest);
			for (UINT i = 0; i < size; i += 3) {
				int value = GetInt24Value(data_ptr + i);

				value = value / 100 * volume;
				SetInt24Value(dest_ptr + i, value);
			}
		}
		else if (m_out_bits == 16) {
			const short* data_ptr = static_cast<const short*>(data);
			short* dest_ptr = static_cast<short*>(dest);
			for (UINT i = 0; i < size / 2; ++i) {
				int value = data_ptr[i];
				value = value * volume / 100;

				dest_ptr[i] = static_cast<short>(value);
			}
		}
		else if (m_out_bits == 8) {
			const BYTE* data_ptr = static_cast<const BYTE*>(data);
			BYTE* dest_ptr = static_cast<BYTE*>(dest);
			for (UINT i = 0; i < size; ++i) {
				int value = data_ptr[i];
				value = value * volume / 100;

				dest_ptr[i] = static_cast<BYTE>(value);
			}
		}
	}
}

UINT Driver::SourceToOutputSize(UINT size) const
{
	switch (m_conversion) {
	default:
		break;

	case INT8_TO_INT16:
	case INT16_TO_INT32:
		return size * 2;

	case INT24_TO_INT32:
		return size / 3 * 4;
	}

	return size;
}

UINT Driver::OutputToSourceSize(UINT size) const
{
	switch (m_conversion) {
	default:
		break;

	case INT8_TO_INT16:
	case INT16_TO_INT32:
		return size / 2;

	case INT24_TO_INT32:
		return size / 4 * 3;
	}

	return size;
}
