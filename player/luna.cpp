//=============================================================================
// プレイヤーの全体コントロール
//=============================================================================

#include <windows.h>
#include <mmreg.h>
#include <shlwapi.h>
#include "lib/wx_misc.h"
#include "lib/wx_text_rw.h"
#include "lib/wx_command_line.h"
#include "config.h"
#include "player_window.h"
#include "driver_manager.h"
#include "media.h"
#include "media_manager.h"
#include "plugin.h"
#include "plugin_manager.h"
#include "cancel_dialog.h"
#include "luna.h"
#include "config.h"
#include "player_window.h"
#include "engine.h"
#include "lyrics.h"
#include "lyrics_loader.h"
#include "timer.h"

// アプリインスタンス
Luna* Luna::m_instance = NULL;

//-----------------------------------------------------------------------------
// 変数定義
//-----------------------------------------------------------------------------
namespace {
const wchar_t* MSG_NO_DEVICE  = L"使用可能なオーディオデバイスがありません。";
const wchar_t* MSG_NO_PLUGIN  = L"対応するプラグインがないため、再生できません。";
const wchar_t* MSG_PLUGIN_ERR = L"プラグインが処理できないため、再生できません。";
const wchar_t* MSG_FORMAT_ERR = L"未対応のオーディオ形式のため、再生できません。";

const wchar_t* MSG_ENGINE_ERR[Engine::ERR_MAX] =
{
	L"再生できません。",
	L"メモリー不足のため、再生できません。",
	L"オーディオデバイスを開けません。",
	L"再生に失敗しました。",
};

} //namespace

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
Luna::Luna()
	: m_root(NULL)
	, m_index(0)
	, m_plugin(NULL)
{
	m_instance = this;
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
Luna::~Luna()
{
	m_instance = NULL;
}

//-----------------------------------------------------------------------------
// アプリ初期化
//-----------------------------------------------------------------------------
bool Luna::Initialize(HINSTANCE app_inst)
{
	WX_TRACE("Lunaを起動します。\n");

	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) {
		WX_TRACE("[ERROR] COM init failed. hr=%x.\n", hr);
		return false;
	}

	LoadConfig(m_config);

	Timer::Initialize();

	m_root = m_player.Initialize(app_inst);
	if (!m_root) {
		WX_TRACE("[ERROR] Player init failed.\n");
		return false;
	}

	m_player.SetInfoText1(APP_NAME);
	m_player.SetInfoText2(TEXT(""));
	m_player.SetPlayInfo(TEXT(""));

	if (!DriverManager::Initialize(m_config.device_name, m_config.allow_ks)) {
		WX_TRACE("[ERROR] Driver Manager init failed.\n");
		return false;
	}

	if (!PluginManager::Initialize(m_root)) {
		WX_TRACE("[ERROR] Plugin Manager init failed.\n");
		return false;
	}

	if (!MediaManager::Initialize()) {
		WX_TRACE("[ERROR] Media Manager init failed.\n");
		return false;
	}

	if (!m_engine.Initialize()) {
		WX_TRACE("[ERROR] Engine init failed.\n");
		return false;
	}

	MediaManager::SetEnableRuby(m_config.use_ruby);
	MediaManager::LoadDefaultList();

	// コマンドラインで渡されたファイルを追加
	wx::CommandLine arg;
	for (int i = 0; i < arg.GetArgc(); ++i) {
		AddMedia(arg.GetArgv(i));
	}

	// 前回のソート状態を復元
	const MediaManager::SortKey sort_key = static_cast<MediaManager::SortKey>(m_config.sort_key);
	const MediaManager::SortType sort_type = static_cast<MediaManager::SortType>(m_config.sort_type);
	if (sort_key != MediaManager::SORT_KEY_NONE) {
		MediaManager::Sort(sort_key, sort_type);
	}

	m_player.UpdatePlayList();

	if (DriverManager::GetDeviceNum() > 0) {
		m_config.device_name = DriverManager::GetDeviceName();
	}
	else {
		MessageBox(m_root, MSG_NO_DEVICE, APP_NAME, MB_OK);
		m_config.device_name.Clear();
	}

	m_engine.SetDriver(DriverManager::GetDriver(), DriverManager::GetDeviceGuid());

	WX_TRACE("Luna started.\n");
	return true;
}

//-----------------------------------------------------------------------------
// アプリ終了
//-----------------------------------------------------------------------------
void Luna::Finalize()
{
	SaveConfig(GetConfig());
	MediaManager::SaveDefaultList();

	m_engine.Finalize();
	m_player.Finalize();
	m_root = NULL;

	MediaManager::Finalize();
	PluginManager::Finalize();
	DriverManager::Finalize();
	Timer::Finalize();

	CoUninitialize();

	WX_TRACE("Luna exit.\n");
}

//-----------------------------------------------------------------------------
// メッセージ等のメイン処理
//-----------------------------------------------------------------------------
bool Luna::Execute(MSG* msg)
{
	if (!m_player.ProcessMessage(msg)) {
		TranslateMessage(msg);
		DispatchMessage(msg);
	}

	return true;
}

//-----------------------------------------------------------------------------
// 再生（再生中は一時停止）
//-----------------------------------------------------------------------------
bool Luna::Play()
{
	Player& player = GetInstance().m_player;
	Engine& engine = GetInstance().m_engine;
	if (engine.IsPlaying()) {
		if (engine.IsPausing()) {
			player.StartTimer();
		}
		else {
			player.StopTimer();
		}

		engine.Pause();
		return true;
	}

	// メディアなしの場合
	int num = MediaManager::GetNum();
	int index = GetInstance().m_index;
	if (num < 1 || index >= num) {
		return false;
	}

	Media* media = MediaManager::Get(index);
	if (!media) {
		return false;
	}

	player.SetInfoText1(TEXT("Now, Loading..."));
	player.SetInfoText2(TEXT(""));

	WX_TRACE("Source:%s\n", media->GetPath());

	Plugin* plugin = NULL;
	Metadata meta;
	Output out;

	ZeroMemory(&meta, sizeof(meta));
	ZeroMemory(&out, sizeof(out));

	int pnum = PluginManager::GetNum();
	bool found = false;
	for (int i = 0; i < pnum; ++i) {
		Plugin* plg = PluginManager::GetPlugin(i);
		if (plg && plg->IsSupported(media->GetPath())) {
			ZeroMemory(&meta, sizeof(meta));
			ZeroMemory(&out, sizeof(out));
			found = true;

			if (plg->Parse(media->GetPath(), meta)) {
				if (plg->Open(media->GetPath(), out)) {
					plugin = plg;
					break;
				}
			}
		}
	}

	if (!found) {
		player.SetInfoText1(MSG_NO_PLUGIN);
		WX_TRACE("[ERROR] Not Found Plugin.\n");
		return false;
	}

	if (!plugin) {
		player.SetInfoText1(MSG_PLUGIN_ERR);
		WX_TRACE("[ERROR] Plugin Error.\n");
		return false;
	}

	WAVEFORMATEX wfx;

	wfx.wFormatTag		= WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec	= out.sample_rate;
	wfx.wBitsPerSample	= static_cast<WORD>(out.sample_bits);
	wfx.nChannels		= static_cast<WORD>(out.num_channels);
	wfx.nBlockAlign		= wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec	= wfx.nBlockAlign * wfx.nSamplesPerSec;
	wfx.cbSize			= 0;

	media->SetDuration(meta.duration);
	media->SetSeekable(meta.seekable != FALSE);

	// タイトルがないなら、ファイル名から補完する
	if (lstrlen(meta.title) < 1) {
		lstrcpy(meta.title, PathFindFileName(media->GetPath()));
		PathRemoveExtension(meta.title);
	}

	media->SetTitle(meta.title);
	media->SetArtist(meta.artist);
	media->SetAlbum(meta.album);
	media->SetExtra(meta.extra);

	UINT fx_mode = FX_NONE;
	fx_mode |= GetConfig().fx_reverb?   FX_WAVES_REVERB : FX_NONE;
	fx_mode |= GetConfig().fx_compress? FX_COMPRESSOR   : FX_NONE;
	fx_mode |= GetConfig().vc_cancel?   FX_VOCAL_CANCEL : FX_NONE;
	fx_mode |= GetConfig().vc_boost?    FX_VOCAL_BOOST  : FX_NONE;

	engine.SetDriver(DriverManager::GetDriver(), DriverManager::GetDeviceGuid());
	engine.SetVolume(GetConfig().out_volume);
	engine.SetEffect(fx_mode);

	int ret = engine.Play(plugin, wfx, out.unit_length);
	if (ret != Engine::ERR_SUCCESS) {
		plugin->Close();
		player.SetInfoText1(MSG_ENGINE_ERR[ret]);
		WX_TRACE("[ERROR] Play() failed.\n");
		return false;
	}

	GetInstance().m_plugin = plugin;

	bool foundLyrics = false;
	if (Luna::GetConfig().show_lyrics) {
		LyricsLoader loader;

		loader.SetSearchDir(Luna::GetConfig().lyrcis_dir);
		loader.SetSearchNest(3);

		TCHAR title[META_MAXLEN + 1];
		NameSafeCopy(title, media->GetTitle(), WX_NUMBEROF(title));

		// 歌詞を見つけた？
		if (loader.SearchLyrics(media->GetPath(), title)) {
			Lyrics& lyrics = GetLyrics();

			if (lyrics.LoadFile(loader.GetFoundPath(), true)) {
				// 歌詞から情報を補完する（アーティスト名）
				if (media->GetArtistLength() == 0) {
					media->SetArtist(lyrics.GetArtist());
				}

				// 歌詞から情報を補完する（アルバム名）
				if (media->GetAlbumLength() == 0) {
					media->SetAlbum(lyrics.GetAlbum());
				}

				foundLyrics = true;
			}
		}
	}

	WAVEFORMATEX fmt = wfx;
	engine.GetOutputFormat(fmt);

	player.SetInfoText1(media->GetTitle());
	player.SetInfoText2(media->GetArtist());

	wx::String info;
	info += wx::String().Print(L"タイトル: %s\n", media->GetTitle());
	info += wx::String().Print(L"アーティスト: %s\n", media->GetArtist());
	info += wx::String().Print(L"アルバム: %s\n", media->GetAlbum());
	info += wx::String().Print(L"時間: %d.%03d秒\n", meta.duration / 1000, meta.duration % 1000);
	info += wx::String().Print(L"シーク: %s\n", meta.seekable? L"可能" : L"不可");
	info += wx::String().Print(L"その他: %s\n", media->GetExtra());
	info += wx::String().Print(L"パス: %s\n\n", media->GetPath());
	info += wx::String().Print(L"プラグイン: %s\n", plugin->GetName());
	info += wx::String().Print(L"出力先: %s\n", DriverManager::GetDeviceName());

	wchar_t tag[64];
	wsprintf(tag, L"%dHz, %dbit, %dch\n", out.sample_rate, out.sample_bits, out.num_channels);

	if (fmt.wBitsPerSample != wfx.wBitsPerSample) {
		info += L"オーディオ形式: ";
		info += tag;

		wsprintf(tag, L"%dHz, %dbit, %dch\n", fmt.nSamplesPerSec, fmt.wBitsPerSample, fmt.nChannels);
		info += L"出力形式: ";
		info += tag;
	}
	else {
		info += L"オーディオ形式: ";
		info += tag;
	}

	player.SetPlayInfo(info);

	wx::String balloon;
	if (media->GetArtistLength() > 0) {
		balloon = media->GetArtist();
		balloon += L" - ";
	}

	balloon += media->GetTitle();
	player.ShowBalloon(APP_NAME, balloon);

	player.SetSeekbar(media->GetDuration(), media->IsSeekable());
	player.StartTimer();
	return true;
}

//-----------------------------------------------------------------------------
// 停止
//-----------------------------------------------------------------------------
void Luna::Stop()
{
	GetInstance().m_engine.Stop();

	if (GetInstance().m_plugin) {
		GetInstance().m_plugin->Close();
		GetInstance().m_plugin = NULL;
	}

	Player& player = GetInstance().m_player;

	player.StopTimer();
	player.HideBalloon();
	player.SetInfoText1(L"");
	player.SetInfoText2(L"");
	player.SetPlayInfo(L"");
	player.SetSeekbar(0, false);

	GetLyrics().FreeData();
}

//-----------------------------------------------------------------------------
// 前の曲へ
//-----------------------------------------------------------------------------
void Luna::Prev()
{
	Stop();

	int num = MediaManager::GetNum();
	int index = GetInstance().m_index;

	if (num == 0) {
		SetNexIndex(0);
		return;
	}

	switch (Luna::GetConfig().play_mode) {
	case PLAY_NORMAL:
		index = (index > 0)? (index - 1) : 0;
		break;

	case PLAY_REPEAT:
		index = (index > 0)? (index - 1) : (num - 1);
		break;

	case PLAY_SINGLE:
		break;

	case PLAY_RANDOM:
		srand(GetTickCount());
		index = wx::Clamp(MulDiv(num, rand(), RAND_MAX), 0, num - 1);
		break;

	default:
		index = 0;
		break;
	}

	SetNexIndex(index);
	Play();
}

//-----------------------------------------------------------------------------
// 次の曲へ
//-----------------------------------------------------------------------------
void Luna::Next()
{
	Stop();

	int num = MediaManager::GetNum();
	int index = GetInstance().m_index;

	if (num == 0) {
		SetNexIndex(0);
		return;
	}

	switch (Luna::GetConfig().play_mode) {
	case PLAY_NORMAL:
		index = (index < (num - 1))? (index + 1) : -1;
		break;

	case PLAY_REPEAT:
		index = (index < (num - 1))? (index + 1) : 0;
		break;

	case PLAY_SINGLE:
		break;

	case PLAY_RANDOM:
		srand(GetTickCount());
		index = wx::Clamp(MulDiv(num, rand(), RAND_MAX), 0, num - 1);
		break;

	default:
		index = 0;
		break;
	}

	if (index >= 0) {
		SetNexIndex(index);
		Play();
	}
	else {
		// 通常再生時は、全部終わったら何もしない
		SetNexIndex(index);
	}
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
bool Luna::Seek(UINT time)
{
	bool is_pausing = GetInstance().m_engine.IsPausing();
	bool ret = GetInstance().m_engine.Seek(time);
	GetLyrics().SeekTime(time);

	if (is_pausing) {
		GetInstance().m_player.StartTimer();
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 音量設定
//-----------------------------------------------------------------------------
void Luna::SetVolume(int volume)
{
	GetEngine().SetVolume(volume);
	GetConfig().out_volume = volume;
}

//-----------------------------------------------------------------------------
// ファイル・フォルダを判定してメディアを追加する
//-----------------------------------------------------------------------------
bool Luna::Add(const wchar_t* path)
{
	if (PathIsDirectory(path)) {
		return ParseDir(path);
	}

	return AddMedia(path);
}

//-----------------------------------------------------------------------------
// 再生終了通知
//-----------------------------------------------------------------------------
void Luna::PlayEnd()
{
	PostMessage(GetInstance().m_root, MSG_PLAYEND, 0, 0);
}


//-----------------------------------------------------------------------------
// 再生エラー通知
//-----------------------------------------------------------------------------
void Luna::PlayErr()
{
	PostMessage(GetInstance().m_root, MSG_PLAYERR, 0, 0);
}


//-----------------------------------------------------------------------------
// メッセージを処理する
//-----------------------------------------------------------------------------
void Luna::ProcessMessage()
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}


//-----------------------------------------------------------------------------
// 次に再生するインデックスを指定
//-----------------------------------------------------------------------------
void Luna::SetNexIndex(int index)
{
	GetInstance().m_index = index;
}


//-----------------------------------------------------------------------------
// ディレクトリを解析し、メディアを追加する
//-----------------------------------------------------------------------------
bool Luna::ParseDir(const wchar_t* dir)
{
	TCHAR scan_path[MAX_PATH];
	bool canceled = false;

	PathCombine(scan_path, dir, TEXT("*"));

	HANDLE find;
	WIN32_FIND_DATA wfd;

	find = FindFirstFile(scan_path, &wfd);
	if (find == INVALID_HANDLE_VALUE) {
		return canceled;
	}

	do {
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (wfd.cFileName[0] != TEXT('.')) {
				PathCombine(scan_path, dir, wfd.cFileName);
				canceled = ParseDir(scan_path);
			}
		}
		else {
			PathCombine(scan_path, dir, wfd.cFileName);
			AddMedia(scan_path);
		}
	}
	while (FindNextFile(find, &wfd) && !canceled);
	FindClose(find);

	ProcessMessage();
	canceled |= CancelDialog::IsCanceled();

	return canceled;
}


//-----------------------------------------------------------------------------
// メディアを追加する
//-----------------------------------------------------------------------------
bool Luna::AddMedia(const wchar_t* path)
{
	CancelDialog::SetPath(path);

	int num = PluginManager::GetNum();
	for (int i = 0; i < num; ++i) {
		Plugin* plugin = PluginManager::GetPlugin(i);
		if (plugin->IsSupported(path)) {
			Metadata meta;
			ZeroMemory(&meta, sizeof(meta));

			if (plugin->Parse(path, meta)) {
				Media media(path, meta);
				MediaManager::UpdateRuby(media);
				MediaManager::Insert(media, -1);
				return true;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// ファイルシステムセーフな名前でコピー
//-----------------------------------------------------------------------------
void Luna::NameSafeCopy(wchar_t* dest, const wchar_t* src, int length)
{
	const int maxlen = (length - 1);
	dest[maxlen] = L'\0';

	for (int i = 0; i < maxlen; ++i) {
		const wchar_t c = src[i];
		dest[i] = c;

		if (c == L'\0') {
			break;
		}

		// ファイルシステムで使えない文字を'_'に置き換え
		if (c == L'\\' || c == L'/' || c == L':' || c == L'*'
		 || c == L'?'  || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
			dest[i] = L'_';
		}
	}
}
