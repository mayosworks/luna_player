//=============================================================================
// Auxiliary library for Windows API (C++)
//                                                     Copyright (c) 2007 MAYO.
//=============================================================================
#pragma once

#define NOMINMAX

#include <windows.h>

namespace wx {

//-----------------------------------------------------------------------------
//! @brief	アサート
//!
//! @param	file	ファイル名
//! @param	line	行数
//! @param	exp		処理式
//! @param	msg		メッセージ
//! @param	...		メッセージの引数
//-----------------------------------------------------------------------------
void Assert(const char* file, int line, const char* exp, const char* msg = NULL, ...);

//-----------------------------------------------------------------------------
//! @brief	トレースログ出力（通常はLOGマクロを使用してください）
//!
//! @param	msg		メッセージ
//! @param	...		メッセージの引数
//-----------------------------------------------------------------------------
void Trace(const char* msg, ...);

//-----------------------------------------------------------------------------
//! @brief	システムエラー表示
//-----------------------------------------------------------------------------
void DisplayError();

//-----------------------------------------------------------------------------
//! @brief	２つのうち小さいほうの値を取得
//!
//! @param	a	値１
//! @param	b	値２
//!
//! @return	小さいほうの値
//-----------------------------------------------------------------------------
template <typename Type>
inline Type Min(Type a, Type b)
{
	return (a < b)? a : b;
}

//-----------------------------------------------------------------------------
//! @brief	２つのうち大さいほうの値を取得
//!
//! @param	a	値１
//! @param	b	値２
//!
//! @return	大さいほうの値
//-----------------------------------------------------------------------------
template <typename Type>
inline Type Max(Type a, Type b)
{
	return (a < b)? b : a;
}

//-----------------------------------------------------------------------------
//! @brief	範囲を制限する
//!
//! @param	x	値
//! @param	min	最小
//! @param	max	最大
//!
//! @return	範囲内の値
//-----------------------------------------------------------------------------
template <typename Type>
inline Type Clamp(Type x, Type min, Type max)
{
	return ((x < min)? min : ((max < x)? max : x));
}

//-----------------------------------------------------------------------------
//! @brief	指定値にアラインメントする
//!
//! @param	x	値
//! @param	a	切り上げる値
//!
//! @return	アラインメントした値
//-----------------------------------------------------------------------------
template <typename Type>
inline Type Align(Type x, Type a)
{
	T align = a - 1;
	return (x + align) & (~align);
}

//-----------------------------------------------------------------------------
//! @brief	２値の差を求める
//!
//! @param	a	値１
//! @param	b	値２
//!
//! @return	差
//-----------------------------------------------------------------------------
template <typename Type>
inline Type Diff(Type a, Type b)
{
	return (a < b)? (b - a) : (a - b);
}

//-----------------------------------------------------------------------------
//! @brief	ポインタにオフセットを加算
//!
//! @param	ptr		ポインタ
//! @param	offset	加算するオフセット
//!
//! @return	加算後のポインタ
//-----------------------------------------------------------------------------
inline void* AddOffset(void* ptr, int offset)
{
	return (static_cast<char*>(ptr) + offset);
}

//-----------------------------------------------------------------------------
//! @brief	constポインタにオフセットを加算
//!
//! @param	ptr		constポインタ
//! @param	offset	加算するオフセット
//!
//! @return	加算後のポインタ
//-----------------------------------------------------------------------------
inline const void* AddOffset(const void* ptr, int offset)
{
	return (static_cast<const char*>(ptr) + offset);
}

//-----------------------------------------------------------------------------
// 配列サイズを求めるヘルパー
//-----------------------------------------------------------------------------
template <typename T, size_t N>
char (*lengthof_helper_(T (&a)[N]))[N];

} // namespace wx

//-----------------------------------------------------------------------------
// 配列算出マクロ
//-----------------------------------------------------------------------------
#define WX_LENGTHOF(a) (sizeof(*wx::lengthof_helper_(a)))

//-----------------------------------------------------------------------------
// デバッグログ定義
//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define	WX_TRACE(msg,...)	wx::Trace(msg, __VA_ARGS__)
#else
#define	WX_TRACE(msg,...)
#endif

//-----------------------------------------------------------------------------
// アサートマクロ定義
//-----------------------------------------------------------------------------

// アサート（メッセージを追加できる）
#ifndef WX_ASSERT
#ifdef _DEBUG
#define WX_ASSERT(exp,...)	(void)((!!(exp)) || (wx::Assert(__FILE__, __LINE__, #exp, __VA_ARGS__), 0))
#else
#define WX_ASSERT(exp,...)
#endif
#endif

// ポインタNULLアサート
#ifndef WX_NULL_ASSERT
#define WX_NULL_ASSERT(exp)	WX_ASSERT(exp, "Pointer must not be NULL.")
#endif

// コンパイル時アサート
#ifndef WX_STATIC_ASSERT
#ifdef _DEBUG
#define WX_STATIC_ASSERT(exp)	extern void static_assert_(arg[(exp)? 1 : -1])
#else
#define WX_STATIC_ASSERT(exp)
#endif
#endif //WX_STATIC_ASSERT

//-----------------------------------------------------------------------------
// 汎用マクロ定義
//-----------------------------------------------------------------------------

// 未使用引数
#ifndef WX_UNUSED
#define WX_UNUSED(x)	((void)&x)
#endif

// 配列の要素数計算
#ifndef WX_NUMBEROF
#define WX_NUMBEROF(ary)	(sizeof(ary) / sizeof(ary[0]))
#endif

// 解放(Release)マクロ
#ifndef WX_SAFE_RELEASE
#define WX_SAFE_RELEASE(ptr)	{ if (ptr) { ptr->Release(); ptr = NULL; } }
#endif

// 単一メモリ解放マクロ
#ifndef WX_SAFE_DELETE
#define WX_SAFE_DELETE(ptr)	{ delete ptr; ptr = NULL; }
#endif

// 配列メモリ解放マクロ
#ifndef WX_SAFE_DELETE_ARRAY
#define WX_SAFE_DELETE_ARRAY(ptr)	{ delete [] ptr; ptr = NULL; }
#endif

//-----------------------------------------------------------------------------
// メモリマクロ定義
//-----------------------------------------------------------------------------

// 既存のマクロ定義を解除する
#ifdef RtlZeroMemory
#undef RtlZeroMemory
#endif

#ifdef RtlMoveMemory
#undef RtlMoveMemory
#endif

#ifdef RtlFillMemory
#undef RtlFillMemory
#endif

#ifdef ZeroMemory
#undef ZeroMemory
#endif

#ifdef MoveMemory
#undef MoveMemory
#endif

#ifdef CopyMemory
#undef CopyMemory
#endif

#ifdef FillMemory
#undef FillMemory
#endif

// インポート定義
#ifdef __cplusplus
#define NTIMPORT extern "C" NTSYSAPI
#else
#define NTIMPORT NTSYSAPI
#endif

// カーネル側で定義されたメモリ操作APIをインポート
NTIMPORT void NTAPI RtlZeroMemory(void* dest, size_t length);
NTIMPORT void NTAPI RtlMoveMemory(void* dest, const void* src, size_t length);
NTIMPORT void NTAPI RtlFillMemory(void* dest, size_t length, BYTE value);

// マクロを再定義する
#define ZeroMemory RtlZeroMemory
#define MoveMemory RtlMoveMemory
#define CopyMemory RtlMoveMemory
#define FillMemory RtlFillMemory
