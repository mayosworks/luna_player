//=============================================================================
// USB Audio関係
//=============================================================================
#pragma once

//-----------------------------------------------------------------------------
// USBデバイスかどうかを判定する。
//-----------------------------------------------------------------------------
bool IsUSBDevice(const wchar_t* path);

//-----------------------------------------------------------------------------
// 指定のベンダーID、プロダクトIDを持つUSBオーディオデバイスのデバイスIF名を取得。
// ※たぶん、これでコンパネと同じ名前がとれると思う。参考：DDK/USBView
//-----------------------------------------------------------------------------
bool GetUSBAudioDeviceName(UINT vender_id, UINT product_id, wchar_t* name);
