﻿//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================
#pragma once

#include <windows.h>

namespace wx {

//-----------------------------------------------------------------------------
//! @brief	CRC16 (CRC-16-CCITT)を計算する
//!
//! @param	data		CRCを計算するデータ
//! @param	size		データのサイズ、バイト数
//!
//! @return	16bit CRC値
//-----------------------------------------------------------------------------
WORD CalcCRC16(const void* data, UINT size);

//-----------------------------------------------------------------------------
//! @brief	CRC32 (CRC-32-IEEE 802.3)を計算する
//!
//! @param	data		CRCを計算するデータ
//! @param	size		データのサイズ、バイト数
//!
//! @return	32bit CRC値
//-----------------------------------------------------------------------------
UINT CalcCRC32(const void* data, UINT size);

//-----------------------------------------------------------------------------
//! @brief	FNV32-1aによるハッシュコードを計算する
//!
//! @param	data		ハッシュを計算するデータ
//! @param	size		データのサイズ、バイト数
//!
//! @return	32bitハッシュコード
//-----------------------------------------------------------------------------
UINT CalcFNV32a(const void* data, UINT size);

//-----------------------------------------------------------------------------
//! @brief	FNV64-1aによるハッシュコードを計算する
//!
//! @param	data		ハッシュを計算するデータ
//! @param	size		データのサイズ、バイト数
//!
//! @return	64bitハッシュコード
//-----------------------------------------------------------------------------
UINT64 CalcFNV64a(const void* data, UINT size);

} //namespace wx
