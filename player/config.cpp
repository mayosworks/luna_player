//=============================================================================
// 設定関係
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "lib/wx_misc.h"
#include "lib/wx_ini_file.h"
#include "config.h"

//-----------------------------------------------------------------------------
// 設定読み込み
//-----------------------------------------------------------------------------
void LoadConfig(Config& conf)
{
	wx::IniFile ini;

	ini.SetSection(TEXT("Display"));
	conf.always_top     = ini.GetBool(TEXT("AlwaysTop"), false);
	conf.use_task_tray  = ini.GetBool(TEXT("TaskTray"), false);
	conf.use_balloon    = ini.GetBool(TEXT("Balloon"), false);
	conf.use_tray_ctrl  = ini.GetBool(TEXT("TrayCtrl"), false);
	conf.hide_task_bar  = ini.GetBool(TEXT("HideTBar"), false);
	conf.title_width    = ini.GetUint(TEXT("TitleWidth"), UINT(-1));
	conf.artist_width   = ini.GetUint(TEXT("ArtistWidth"), UINT(-1));
	conf.album_width    = ini.GetUint(TEXT("AlbumWidth"), UINT(-1));
	conf.duration_width = ini.GetUint(TEXT("DurationWidth"), UINT(-1));

	ini.SetSection(TEXT("Output"));
	conf.device_name  = ini.GetText(TEXT("DeviceName"), TEXT(""));
	conf.out_volume   = ini.GetUint(TEXT("OutVolume"), 100);
	conf.use_mmcss    = ini.GetBool(TEXT("UseMmcss"), false);
	conf.allow_ks     = ini.GetBool(TEXT("AllowKs"), false);
	conf.fx_reverb    = ini.GetBool(TEXT("UseReverb"), false);
	conf.fx_compress  = ini.GetBool(TEXT("FxCompress"), false);
	conf.vc_cancel    = ini.GetBool(TEXT("VcCancel"), false);
	conf.vc_boost     = ini.GetBool(TEXT("VcBoost"), false);

	ini.SetSection(TEXT("Lyrics"));
	conf.show_lyrics  = ini.GetBool(TEXT("ShowLyrics"), false);
	conf.scan_sub_dir = ini.GetBool(TEXT("ScanSubDir"), false);
	conf.lyrcis_dir   = ini.GetText(TEXT("LyricsDir"), TEXT(""));

	ini.SetSection(TEXT("Control"));
	conf.play_mode = ini.GetUint(TEXT("PlayMode"), 0);
	conf.sort_key  = ini.GetUint(TEXT("SortKey"), 0);
	conf.sort_type = ini.GetUint(TEXT("SortType"), 0);
	conf.use_ruby  = ini.GetBool(TEXT("UseRuby"), false);

	// 不正な値の調整
	conf.out_volume = wx::Clamp(conf.out_volume, 0, 100);
}

//-----------------------------------------------------------------------------
// 設定保存
//-----------------------------------------------------------------------------
void SaveConfig(Config& conf)
{
	wx::IniFile ini;

	ini.SetSection(TEXT("Display"));
	ini.SetBool(TEXT("AlwaysTop"), conf.always_top);
	ini.SetBool(TEXT("TaskTray"),  conf.use_task_tray);
	ini.SetBool(TEXT("Balloon"),   conf.use_balloon);
	ini.SetBool(TEXT("TrayCtrl"),  conf.use_tray_ctrl);
	ini.SetBool(TEXT("HideTBar"),  conf.hide_task_bar);
	
	ini.SetUint(TEXT("TitleWidth"),    conf.title_width);
	ini.SetUint(TEXT("ArtistWidth"),   conf.artist_width);
	ini.SetUint(TEXT("AlbumWidth"),    conf.album_width);
	ini.SetUint(TEXT("DurationWidth"), conf.duration_width);

	ini.SetSection(TEXT("Output"));
	ini.SetText(TEXT("DeviceName"), conf.device_name);
	ini.SetUint(TEXT("OutVolume"),  conf.out_volume);
	ini.SetBool(TEXT("UseMmcss"),   conf.use_mmcss);
	ini.SetBool(TEXT("AllowKs"),    conf.allow_ks);
	ini.SetBool(TEXT("UseReverb"),  conf.fx_reverb);
	ini.SetBool(TEXT("FxCompress"), conf.fx_compress);
	ini.SetBool(TEXT("VcCancel"),   conf.vc_cancel);
	ini.SetBool(TEXT("VcBoost"),    conf.vc_boost);

	ini.SetSection(TEXT("Lyrics"));
	ini.SetBool(TEXT("ShowLyrics"), conf.show_lyrics);
	ini.SetBool(TEXT("ScanSubDir"), conf.scan_sub_dir);
	ini.SetText(TEXT("LyricsDir"),  conf.lyrcis_dir);

	ini.SetSection(TEXT("Control"));
	ini.SetUint(TEXT("PlayMode"), conf.play_mode);
	ini.SetUint(TEXT("SortKey"),  conf.sort_key);
	ini.SetUint(TEXT("SortType"), conf.sort_type);
	ini.SetBool(TEXT("UseRuby"),  conf.use_ruby);
}
