//=============================================================================
// サウンドドライバユーティリティ
//=============================================================================

#pragma once

// void*にオフセットを加算
void* AddOffset(void* ptr, UINT offset);

// WAVEFORMATEXからWAVEFORMATEXTENSIBLEへコピー
void CopyToExtensible(WAVEFORMATEXTENSIBLE& wft, const WAVEFORMATEX& wfx);

// WAVE_FORMAT_xxxに対応した、GUIDを取得
GUID GetSubFormatGUID(WORD format_tag);

// チャネル数に対応した、スピーカーマスク値を取得
DWORD GetChannelMask(WORD channels);

// 24bit分のPCMを32bit上位に詰めて取得
int GetInt24Value(const void* buffer);

// 24bit分のPCMを32bit上位に詰めて取得
void SetInt24Value(void* buffer, int value);
