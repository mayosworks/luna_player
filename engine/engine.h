//=============================================================================
// 再生エンジン
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "lib/wx_stacked_state.h"
#include "lib/wx_string.h"
#include "timer.h"
#include "effect.h"

class Driver;
class Plugin;

// 再生エンジン
class Engine
{
public:
	enum
	{
		ERR_SUCCESS,
		ERR_NOMEMORY,
		ERR_OPENFAIL,
		ERR_PLAYFAIL,

		ERR_MAX,
	};

public:
	Engine();
	~Engine();

	bool Initialize();
	void Finalize();

	void SetEffect(UINT fx_mode);
	void SetDriver(Driver* driver, const wchar_t* guid);

	// 実行制御
	int  Play(Plugin* plugin, const WAVEFORMATEX& wfx, UINT block_size);
	void Stop();
	bool Seek(UINT time);
	void Pause();

	// 現在の状態
	bool IsPlaying() const;
	bool IsPausing() const;

	UINT GetPlayTime() const;
	void SetVolume(int volume);

	bool GetOutputFormat(WAVEFORMATEX& wfx) const;

private:
	static DWORD WINAPI ThreadProc(void* param);
	DWORD Main();

	UINT Request(UINT cmd, UINT arg = 0);
	void Response(UINT ret);

	// メイン：レンダリング実行
	void Execute();

	int  InitializeOutput();
	void FinalizeOutput();
	UINT RenderBlock();

	// コマンド処理
	void DoCommand();
	void ExecSeek();
	void ExecPause();

private:
	HANDLE	m_thread;
	HANDLE	m_req_ev;
	HANDLE	m_res_ev;
	UINT	m_cmd;
	UINT	m_arg;
	UINT	m_ret;

	Timer		m_timer;
	Effect		m_effect;
	UINT		m_fx_mode;
	Driver*		m_driver;
	wx::String	m_guid;
	int			m_volume;
	Plugin*		m_plugin;

	wx::StackedState<>	m_state;
	WAVEFORMATEX		m_wfx;
	BYTE*				m_buffer;
	UINT				m_block_size;
	UINT				m_loop_count;
	UINT				m_base_time;
};
