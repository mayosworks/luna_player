//=============================================================================
// 再生エンジン
//=============================================================================

#include <windows.h>
#include <avrt.h>
#include "lib/wx_misc.h"
#include "plugin.h"
#include "engine.h"
#include "luna.h"
#include "driver.h"

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
namespace {

// コマンドリスト
enum 
{
	CMD_NONE,
	CMD_INIT,
	CMD_TERM,
	CMD_PLAY,
	CMD_STOP,
	CMD_SEEK,
	CMD_PAUSE,

	CMD_NUM,

	CMD_NG = 0,
	CMD_OK = 1,
	CMD_FAILED = 0xFFFFFFFF,
};

// 処理状態
enum State
{
	STATE_NONE,		// 何もしてない
	STATE_RENDER,	// データ生成
	STATE_WRITE,	// 書き込み
	STATE_PAUSE,	// ポーズ
	STATE_FLUSH,	// 書き込み完了
	STATE_NOTIFY,	// 再生終了通知
	STATE_STOP,		// ユーザーの停止

	STATE_MAX,
};

// 定数
const UINT WAIT_TIME_L = 20;	// 待機時、この時間ごとに割り込み
const UINT WAIT_TIME_S =  3;	// 生成時、この時間ごとに割り込み
const UINT ALLOW_DIFF  = 10;	// 時間ずれの許容値

typedef HANDLE (WINAPI* fpAvSetMmThreadCharacteristics)(LPCWSTR TaskName, LPDWORD TaskIndex);
typedef BOOL   (WINAPI* fpAvRevertMmThreadCharacteristics)(HANDLE AvrtHandle);
typedef BOOL   (WINAPI* fpAvSetMmThreadPriority)(HANDLE AvrtHandle, AVRT_PRIORITY Priority);

} //namespace

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Engine::Engine()
	: m_thread(NULL)
	, m_req_ev(NULL)
	, m_res_ev(NULL)
	, m_cmd(0)
	, m_arg(0)
	, m_ret(0)
	, m_fx_mode(0)
	, m_driver(NULL)
	, m_volume(100)
	, m_plugin(NULL)
	, m_buffer(NULL)
	, m_block_size(0)
	, m_loop_count(0)
	, m_base_time(0)
{
	ZeroMemory(&m_wfx, sizeof(m_wfx));
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Engine::~Engine()
{
}

//-----------------------------------------------------------------------------
// レンダラを初期化
//-----------------------------------------------------------------------------
bool Engine::Initialize()
{
	m_req_ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_res_ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!m_req_ev || !m_res_ev) {
		Finalize();
		return false;
	}

	DWORD thread_id = 0;
	m_thread = CreateThread(NULL, 0, ThreadProc, this, 0, &thread_id);
	if (!m_thread) {
		Finalize();
		return false;
	}

	Request(CMD_INIT);
	return true;
}

//-----------------------------------------------------------------------------
// レンダラを終了
//-----------------------------------------------------------------------------
void Engine::Finalize()
{
	if (m_thread) {
		UINT ret = Request(CMD_TERM);
		if (ret != CMD_OK) {
			TerminateThread(m_thread, 1);
		}

		if (WaitForSingleObject(m_thread, 10 * 1000) != WAIT_OBJECT_0) {
			TerminateThread(m_thread, 1);
		}

		CloseHandle(m_thread);
		m_thread = NULL;
	}

	if (m_req_ev) {
		CloseHandle(m_req_ev);
		m_req_ev = NULL;
	}

	if (m_res_ev) {
		CloseHandle(m_res_ev);
		m_res_ev = NULL;
	}
}

//-----------------------------------------------------------------------------
// エフェクト設定
//-----------------------------------------------------------------------------
void Engine::SetEffect(UINT fx_mode)
{
	m_fx_mode = fx_mode;
}

//-----------------------------------------------------------------------------
// ドライバ設定
//-----------------------------------------------------------------------------
void Engine::SetDriver(Driver* driver, const wchar_t* guid)
{
	m_driver = driver;
	m_guid = guid;
}

//-----------------------------------------------------------------------------
// 再生
//-----------------------------------------------------------------------------
int Engine::Play(Plugin* plugin, const WAVEFORMATEX& wfx, UINT block_size)
{
	WX_TRACE("OnPlay.\n");

	m_plugin = plugin;
	m_wfx = wfx;
	m_block_size = block_size;

	int ret = InitializeOutput();
	if (ret != ERR_SUCCESS) {
		return ret;
	}

	if (Request(CMD_PLAY) != CMD_OK) {
		FinalizeOutput();
		return ERR_PLAYFAIL;
	}

	return ERR_SUCCESS;
}

//-----------------------------------------------------------------------------
// 停止
//-----------------------------------------------------------------------------
void Engine::Stop()
{
	WX_TRACE("OnStop.\n");
	Request(CMD_STOP);
	FinalizeOutput();
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
bool Engine::Seek(UINT time)
{
	WX_TRACE("OnSeek. time=%d\n", time);
	return (Request(CMD_SEEK, time) == CMD_OK);
}

//-----------------------------------------------------------------------------
// 一時停止
//-----------------------------------------------------------------------------
void Engine::Pause()
{
	WX_TRACE("OnPause.\n");
	Request(CMD_PAUSE);
}

//-----------------------------------------------------------------------------
// 再生中かを取得する
//-----------------------------------------------------------------------------
bool Engine::IsPlaying() const
{
	return (m_state != STATE_NONE);
}

//-----------------------------------------------------------------------------
// 一時停止中かを取得する
//-----------------------------------------------------------------------------
bool Engine::IsPausing() const
{
	return (m_state == STATE_PAUSE);
}

//-----------------------------------------------------------------------------
// 再生時間を取得する
//-----------------------------------------------------------------------------
UINT Engine::GetPlayTime() const
{
	return m_timer.GetTime();
}

//-----------------------------------------------------------------------------
// 音量設定
//-----------------------------------------------------------------------------
void Engine::SetVolume(int volume)
{
	m_volume = volume;
	if (m_driver) {
		m_driver->SetVolume(volume);
	}
}

bool Engine::GetOutputFormat(WAVEFORMATEX& wfx) const
{
	if (m_driver) {
		return m_driver->GetFormat(wfx);
	}

	return false;
}

//-----------------------------------------------------------------------------
// スレッドスタートアップルーチン
//-----------------------------------------------------------------------------
DWORD WINAPI Engine::ThreadProc(void* param)
{
	WX_NULL_ASSERT(param);
	return static_cast<Engine*>(param)->Main();
}

//-----------------------------------------------------------------------------
// エンジンメイン処理
//-----------------------------------------------------------------------------
DWORD Engine::Main()
{
	// 優先度をあげて、再生を安定させる
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	bool use_mmcss = Luna::GetConfig().use_mmcss;

	// Vista以降用、マルチメディアクラススケジューリングサービス設定
	HANDLE mmcss = NULL;
	HMODULE avrt = use_mmcss? LoadLibrary(L"avrt.dll") : NULL;
	if (avrt) {
		fpAvSetMmThreadCharacteristics fAvSetMmThreadCharacteristics =
			(fpAvSetMmThreadCharacteristics)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW");
		if (fAvSetMmThreadCharacteristics) {
			DWORD task_index = 0;
			mmcss = fAvSetMmThreadCharacteristics(L"Playback", &task_index);
			if (mmcss) {
				fpAvSetMmThreadPriority fAvSetMmThreadPriority = 
					(fpAvSetMmThreadPriority)GetProcAddress(avrt, "AvSetMmThreadPriority");
				if (fAvSetMmThreadPriority) {
					fAvSetMmThreadPriority(mmcss, AVRT_PRIORITY_HIGH);
				}
			}
		}
	}

	bool exec = true;
	while (exec) {
		WaitForSingleObject(m_req_ev, INFINITE);

		switch (m_cmd) {
		case CMD_NONE:
			break;

		case CMD_INIT:
			Response(CMD_OK);
			break;

		case CMD_TERM:
			Response(CMD_OK);
			exec = false;
			break;

		case CMD_PLAY:
			Execute();
			break;

		default:
			Response(CMD_NG);
			break;
		}
	}

	// Vista以降用、マルチメディアクラススケジューリングサービス設定
	if (avrt) {
		fpAvRevertMmThreadCharacteristics fAvRevertMmThreadCharacteristics =
			(fpAvRevertMmThreadCharacteristics)GetProcAddress(avrt, "AvRevertMmThreadCharacteristics");
		if (fAvRevertMmThreadCharacteristics) {
			fAvRevertMmThreadCharacteristics(mmcss);
		}

		FreeLibrary(avrt);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// コマンド要求
//-----------------------------------------------------------------------------
UINT Engine::Request(UINT cmd, UINT arg)
{
	// スレッドなしの場合は送信しない
	if (!m_thread) {
		WX_TRACE("[ERROR] No thread.\n");
		return static_cast<UINT>(CMD_FAILED);
	}

	DWORD exitCode = 0;
	GetExitCodeThread(m_thread, &exitCode);

	// スレッド終了の場合は送信しない
	if (exitCode != STILL_ACTIVE) {
		WX_TRACE("[ERROR] No thread.\n");
		return static_cast<UINT>(CMD_FAILED);
	}

	m_cmd = cmd;
	m_arg = arg;
	m_ret = CMD_NG;

	SetEvent(m_req_ev);
	WX_TRACE("Request. Cmd:%d, Arg:%d\n", cmd, arg);

	WaitForSingleObject(m_res_ev, INFINITE);
	WX_TRACE("Response. Ret:%d\n", m_ret);

	m_cmd = 0;
	m_arg = 0;

	return m_ret;
}

//-----------------------------------------------------------------------------
// コマンド応答
//-----------------------------------------------------------------------------
void Engine::Response(UINT ret)
{
	m_ret = ret;
	SetEvent(m_res_ev);
}

//-----------------------------------------------------------------------------
// メイン：レンダリング実行
//-----------------------------------------------------------------------------
void Engine::Execute()
{
	m_driver->Start();
	m_timer.Start();

	Response(CMD_OK);

	UINT rendered = 0;
	UINT written = 0;

	m_state = STATE_RENDER;
	while (m_state < STATE_NOTIFY) {
		if (m_state == STATE_RENDER) {
			rendered = RenderBlock();
			written = 0;
		}

		if (m_state == STATE_WRITE) {
			written += m_driver->Write(m_buffer + written, rendered - written);
			if (rendered == written) {
				m_state.Pop();
			}
		}

		if (m_state == STATE_FLUSH) {
			if (m_driver->Flush()) {
				m_state = STATE_NOTIFY;
				break;
			}
		}

		UINT time = m_base_time + m_driver->GetPlayTime();
		if (wx::Diff(m_timer.GetTime(), time) > ALLOW_DIFF) {
			m_timer.SetTime(time);
		}

		DoCommand();
		Sleep(0);
	}

	m_driver->Stop();
	m_timer.Stop();

	int state = m_state;
	m_state = STATE_NONE;

	if (state == STATE_STOP) {
		Response(CMD_OK);
	}
	else {
		Luna::PlayEnd();
	}
}

//-----------------------------------------------------------------------------
// 出力初期化
//-----------------------------------------------------------------------------
int Engine::InitializeOutput()
{
	WX_TRACE("Initialize Output.\n");

	if (!m_driver) {
		WX_TRACE("[ERROR] No Driver.\n");
		return ERR_OPENFAIL;
	}

	if (m_wfx.wBitsPerSample % 8 > 0) {
		WX_TRACE("[ERROR] bits % 8 != 0. %dbit\n", m_wfx.wBitsPerSample);
		return ERR_OPENFAIL;
	}

	if (!m_driver->Open(m_guid, m_wfx)) {
		WX_TRACE("[ERROR] Can't open Driver. Tag%d, %dHz, %dbit, %dch\n",
			m_wfx.wFormatTag, m_wfx.nSamplesPerSec, m_wfx.wBitsPerSample, m_wfx.nChannels);
		return ERR_OPENFAIL;
	}

	m_driver->SetVolume(m_volume);

	UINT bufSize = wx::Max<UINT>(m_block_size, m_wfx.nAvgBytesPerSec / 10);
	bufSize -= bufSize % m_wfx.nBlockAlign;

	m_buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, bufSize * 4));
	if (!m_buffer) {
		WX_TRACE("[ERROR] Can't allocate buffer.\n");
		m_driver->Close();
		return ERR_NOMEMORY;
	}

	m_block_size = (m_block_size > 0)? m_block_size : bufSize;
	m_loop_count = bufSize / m_block_size;
	m_base_time = 0;

	m_effect.Initialize(FX_SETUP_ALL, m_wfx, m_block_size * m_loop_count);
	return ERR_SUCCESS;
}

//-----------------------------------------------------------------------------
// 出力終了
//-----------------------------------------------------------------------------
void Engine::FinalizeOutput()
{
	WX_TRACE("Finalize Output.\n");

	if (m_driver) {
		m_driver->Close();
	}

	m_effect.Finalize();

	if (m_buffer) {
		HeapFree(GetProcessHeap(), 0, m_buffer);
		m_buffer = NULL;
	}

	m_block_size = 0;
	m_loop_count = 0;
	m_base_time = 0;
}

//-----------------------------------------------------------------------------
// １回分レンダリング
//-----------------------------------------------------------------------------
UINT Engine::RenderBlock()
{
	UINT require = m_block_size * m_loop_count;
	ZeroMemory(m_buffer, require);

	UINT rendered = 0;
	UINT written = m_block_size;

	for (UINT i = 0; (i < m_loop_count) && (written == m_block_size); ++i) {
		written = m_plugin->Render(m_buffer + rendered, m_block_size);
		rendered += written;
	}

	rendered = m_effect.Process(m_buffer, rendered, m_fx_mode);

	m_state.SetState((rendered == require)? STATE_RENDER : STATE_FLUSH);
	m_state.Push(STATE_WRITE);
	return rendered;
}

//-----------------------------------------------------------------------------
// コマンド処理
//-----------------------------------------------------------------------------
void Engine::DoCommand()
{
	DWORD waitTime = (m_state == STATE_RENDER)? WAIT_TIME_S : WAIT_TIME_L;
	if (WaitForSingleObject(m_req_ev, waitTime) != WAIT_OBJECT_0) {
		return;
	}

	switch (m_cmd) {
	case CMD_STOP:
		m_state.Clear();
		m_state = STATE_STOP;
		break;

	case CMD_SEEK:
		ExecSeek();
		break;

	case CMD_PAUSE:
		ExecPause();
		break;

	default:
		Response(CMD_NG);
		break;
	}
}

//-----------------------------------------------------------------------------
// シーク処理
//-----------------------------------------------------------------------------
void Engine::ExecSeek()
{
	if (m_state == STATE_PAUSE) {
		m_state.Pop();
	}

	m_driver->Stop();
	m_driver->Reset();
	m_timer.Suspend();

	UINT time = m_plugin->Seek(m_arg);
	Sleep(0);

	m_driver->Start();
	m_timer.Resume();
	m_timer.SetTime(time);

	m_base_time = time;

	m_state.Clear();
	m_state = STATE_RENDER;

	Response(CMD_OK);

	WX_TRACE("Seeked. time = %d\n", time);
}

//-----------------------------------------------------------------------------
// 一時停止処理
//-----------------------------------------------------------------------------
void Engine::ExecPause()
{
	if (m_state == STATE_PAUSE) {
		WX_TRACE("UnPause\n");
		m_driver->Start();
		m_timer.Resume();
		m_state.Pop();
	}
	else {
		WX_TRACE("Pause\n");
		m_driver->Stop();
		m_timer.Suspend();
		m_state.Push(STATE_PAUSE);
	}

	Response(CMD_OK);
}
