//=============================================================================
// 演奏時間管理
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

// 演奏時間管理タイマー
class Timer
{
public:
	static void Initialize();
	static void Finalize();

public:
	Timer();
	~Timer();

	void Start();
	void Stop();

	void Suspend();
	void Resume();

	UINT GetTime() const;
	void SetTime(UINT time_ms);

private:
	static UINT m_period;

public:
	UINT m_std_time;
	UINT m_cur_time;
};
