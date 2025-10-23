#ifndef CONVARDEFS_H
#define CONVARDEFS_H

// ########################################################################################################################
// # this first part is just to allow for proper code completion when editing this file
// ########################################################################################################################

// NOLINTBEGIN(misc-definitions-in-headers)

#if !defined(CONVAR_H) && \
    (defined(_CLANGD) || defined(Q_CREATOR_RUN) || defined(__INTELLISENSE__) || defined(__CDT_PARSER__))
#define DEFINE_CONVARS
#include "ConVar.h"

struct dummyEngine {
    inline void shutdown() { ; }
    inline void toggleFullscreen() { ; }
};
dummyEngine *engine;

struct dummyGraphics {
    inline void setVSync(bool /**/) { ; }
};
dummyGraphics *g;

struct dummyDatabase {
    inline void save() { ; }
};
dummyDatabase *db;

// struct dummyEnv {
//     inline void setFullscreenWindowedBorderless(bool /**/) { ; }
// };
// dummyEnv *env{};
namespace Environment {
extern void setThreadPriority(float /**/);
}  // namespace Environment

namespace ConVarHandler::ConVarBuiltins {
extern void find(std::string_view args);
extern void help(std::string_view args);
extern void listcommands(void);
extern void dumpcommands(void);
extern void echo(std::string_view args);
}  // namespace ConVarHandler::ConVarBuiltins

extern void _borderless();
extern void _center();
extern void _dpiinfo();
extern void _errortest();
extern void _focus();
extern void _maximize();
extern void _minimize();
extern void _printsize();
extern void _toggleresizable();
extern void _restart();
extern void _update();

extern void _osuOptionsSliderQualityWrapper(float);

#endif

// ########################################################################################################################
// # actual declarations/definitions below
// ########################################################################################################################

#define _CV(name) name
// helper to create an SA delegate from a freestanding/static function
#define CFUNC(func) SA::delegate<decltype(func)>::template create<func>()

// defined and included at the end of ConVar.cpp
#if defined(DEFINE_CONVARS)
#undef CONVAR
#define CONVAR(name, ...) ConVar _CV(name)(__VA_ARGS__)

#include "KeyBindings.h"
#include "BanchoNetworking.h"  // defines some things we need like OSU_VERSION_DATEONLY
#include "build_timestamp.h"
namespace SliderRenderer {
extern void onUniformConfigChanged();
}
namespace RichPresence {
extern void onRichPresenceChange(float, float);
}
namespace Profiling {
extern void vprofToggleCB(float);
}
namespace Spectating {
extern void start_by_username(std::string_view username);
}

#else
#define CONVAR(name, ...) extern ConVar _CV(name)
#endif

class ConVar;
namespace cv {
namespace cmd {

// Generic commands
CONVAR(borderless, "borderless"sv, CLIENT, CFUNC(_borderless));
CONVAR(center, "center"sv, CLIENT, CFUNC(_center));
CONVAR(clear, "clear"sv);
CONVAR(dpiinfo, "dpiinfo"sv, CLIENT, CFUNC(_dpiinfo));
CONVAR(dumpcommands, "dumpcommands"sv, CLIENT, CFUNC(ConVarHandler::ConVarBuiltins::dumpcommands));
CONVAR(errortest, "errortest"sv, CLIENT, CFUNC(_errortest));
CONVAR(exec, "exec"sv, CLIENT);  // set in ConsoleBox
CONVAR(find, "find"sv, CLIENT, CFUNC(ConVarHandler::ConVarBuiltins::find));
CONVAR(focus, "focus"sv, CLIENT, CFUNC(_focus));
CONVAR(help, "help"sv, CLIENT, CFUNC(ConVarHandler::ConVarBuiltins::help));
CONVAR(listcommands, "listcommands"sv, CLIENT, CFUNC(ConVarHandler::ConVarBuiltins::listcommands));
CONVAR(maximize, "maximize"sv, CLIENT, CFUNC(_maximize));
CONVAR(minimize, "minimize"sv, CLIENT, CFUNC(_minimize));
CONVAR(printsize, "printsize"sv, CLIENT, CFUNC(_printsize));
CONVAR(resizable_toggle, "resizable_toggle"sv, CLIENT, CFUNC(_toggleresizable));
CONVAR(restart, "restart"sv, CLIENT, CFUNC(_restart));
CONVAR(save, "save"sv, CLIENT);  // database save, set in Database
CONVAR(showconsolebox, "showconsolebox"sv);
CONVAR(snd_restart, "snd_restart"sv);
CONVAR(update, "update"sv, CLIENT);
CONVAR(complete_oauth, "complete_oauth"sv, CLIENT, CFUNC(BANCHO::Net::complete_oauth));

// Server-callable commands
CONVAR(exit, "exit"sv, CLIENT | SERVER, []() -> void { engine ? engine->shutdown() : (void)0; });
CONVAR(shutdown, "shutdown"sv, CLIENT | SERVER, []() -> void { engine ? engine->shutdown() : (void)0; });
CONVAR(spectate, "spectate"sv, CLIENT | SERVER, CFUNC(Spectating::start_by_username));

// Server and skin-callable commands
CONVAR(echo, "echo"sv, CLIENT | SKINS | SERVER, CFUNC(ConVarHandler::ConVarBuiltins::echo));

}  // namespace cmd

// Audio
CONVAR(asio_buffer_size, "asio_buffer_size"sv, -1, CLIENT,
       "buffer size in samples (usually 44100 samples per second)"sv);
CONVAR(loudness_calc_threads, "loudness_calc_threads"sv, 0.f, CLIENT,
       "0 = autodetect. do not use too many threads or your PC will explode"sv);
CONVAR(loudness_fallback, "loudness_fallback"sv, -12.f, CLIENT);
CONVAR(loudness_target, "loudness_target"sv, -14.f, CLIENT);
CONVAR(sound_panning, "sound_panning"sv, true, CLIENT | SKINS | SERVER,
       "positional hitsound audio depending on the playfield position"sv);
CONVAR(sound_panning_multiplier, "sound_panning_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER,
       "the final panning value is multiplied with this, e.g. if you want to reduce or "
       "increase the effect strength by a percentage"sv);
CONVAR(snd_async_buffer, "snd_async_buffer"sv, 65536, CLIENT,
       "BASS_CONFIG_ASYNCFILE_BUFFER length in bytes. Set to 0 to disable."sv);
CONVAR(snd_change_check_interval, "snd_change_check_interval"sv, 0.5f, CLIENT,
       "check for output device changes every this many seconds. 0 = disabled"sv);
CONVAR(snd_dev_buffer, "snd_dev_buffer"sv, 30, CLIENT, "BASS_CONFIG_DEV_BUFFER length in milliseconds"sv);
CONVAR(snd_dev_period, "snd_dev_period"sv, 10, CLIENT,
       "BASS_CONFIG_DEV_PERIOD length in milliseconds, or if negative then in samples"sv);
CONVAR(snd_output_device, "snd_output_device"sv, "Default"sv, CLIENT);
CONVAR(snd_ready_delay, "snd_ready_delay"sv, 0.f, CLIENT,
       "after a sound engine restart, wait this many seconds before marking it as ready"sv);
CONVAR(snd_restrict_play_frame, "snd_restrict_play_fr ame"sv, true, CLIENT,
       "only allow one new channel per frame for overlayable sounds (prevents lag and earrape)"sv);
CONVAR(snd_updateperiod, "snd_updateperiod"sv, 10, CLIENT, "BASS_CONFIG_UPDATEPERIOD length in milliseconds"sv);
CONVAR(snd_file_min_size, "snd_file_min_size"sv, 51, CLIENT,
       "minimum file size in bytes for WAV files to be considered valid (everything below will "
       "fail to load), this is a workaround for BASS crashes"sv);
CONVAR(snd_force_load_unknown, "snd_force_load_unknown"sv, false, CLIENT,
       "force loading of assumed invalid audio files"sv);
CONVAR(snd_freq, "snd_freq"sv, 44100, CLIENT | NOSAVE, "output sampling rate in Hz"sv);
CONVAR(snd_soloud_buffer, "snd_soloud_buffer"sv, 0, CLIENT | NOSAVE,
       "SoLoud audio device buffer size (recommended to leave this on 0/auto)"sv);
CONVAR(snd_soloud_backend, "snd_soloud_backend"sv, "MiniAudio"sv, CLIENT,
       R"(SoLoud backend, "MiniAudio" or "SDL3" (MiniAudio is default))"sv);
CONVAR(snd_sanity_simultaneous_limit, "snd_sanity_simultaneous_limit"sv, 128, CLIENT | NOSAVE,
       "The maximum number of overlayable sounds that are allowed to be active at once"sv);
CONVAR(snd_soloud_hardcoded_offset, "snd_soloud_hardcoded_offset"sv, -18, CLIENT);  // should hopefully be "temporary"
CONVAR(snd_soloud_prefer_ffmpeg, "snd_soloud_prefer_ffmpeg"sv, 0, CLIENT,
       "(0=no, 1=streams, 2=streams+samples) prioritize using ffmpeg as a decoder (if available) over other decoder "
       "backends"sv);
CONVAR(snd_soloud_prefer_exclusive, "snd_soloud_prefer_exclusive"sv, false, CLIENT,
       "try initializing in exclusive mode first for MiniAudio on Windows"sv);
CONVAR(snd_disable_exclusive_unfocused, "snd_disable_exclusive_unfocused"sv, true, CLIENT,
       "disable WASAPI exclusive mode when losing focus (currently SoLoud+MiniAudio only)"sv);
CONVAR(volume_change_interval, "volume_change_interval"sv, 0.05f, CLIENT | SKINS | SERVER);
CONVAR(volume_effects, "volume_effects"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(volume_master, "volume_master"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(volume_master_inactive, "volume_master_inactive"sv, 0.25f, CLIENT | SKINS | SERVER);
CONVAR(volume_music, "volume_music"sv, 0.4f, CLIENT | SKINS | SERVER);
CONVAR(win_snd_wasapi_buffer_size, "win_snd_wasapi_buffer_size"sv, 0.011f, CLIENT,
       "buffer size/length in seconds (e.g. 0.011 = 11 ms), directly responsible for audio delay and crackling"sv);
CONVAR(win_snd_wasapi_exclusive, "win_snd_wasapi_exclusive"sv, true, CLIENT);
CONVAR(win_snd_wasapi_period_size, "win_snd_wasapi_period_size"sv, 0.0f, CLIENT,
       "interval between OutputWasapiProc calls in seconds (e.g. 0.016 = 16 ms) (0 = use default)"sv);

// Audio (mods)
CONVAR(snd_pitch_hitsounds, "snd_pitch_hitsounds"sv, false, CLIENT | SKINS | SERVER,
       "change hitsound pitch based on accuracy"sv);
CONVAR(snd_pitch_hitsounds_factor, "snd_pitch_hitsounds_factor"sv, -0.5f, CLIENT | SKINS | SERVER,
       "how much to change the pitch"sv);

// Debug
CONVAR(debug_osu, "debug_osu"sv, false, CLIENT);
CONVAR(debug_db, "debug_db"sv, false, CLIENT);
CONVAR(debug_async_db, "debug_async_db"sv, false, CLIENT);
CONVAR(debug_anim, "debug_anim"sv, false, CLIENT);
CONVAR(debug_box_shadows, "debug_box_shadows"sv, false, CLIENT);
CONVAR(debug_draw_timingpoints, "debug_draw_timingpoints"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(debug_engine, "debug_engine"sv, false, CLIENT);
CONVAR(debug_ui, "debug_ui"sv, false, CLIENT);
CONVAR(debug_env, "debug_env"sv, false, CLIENT);
CONVAR(debug_font, "debug_font"sv, false, CLIENT);
CONVAR(debug_file, "debug_file"sv, false, CLIENT);
CONVAR(debug_hiterrorbar_misaims, "debug_hiterrorbar_misaims"sv, false, CLIENT);
CONVAR(debug_image, "debug_image"sv, false, CLIENT | NOLOAD | NOSAVE);
CONVAR(debug_mouse, "debug_mouse"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(debug_pp, "debug_pp"sv, false, CLIENT);
CONVAR(debug_rm, "debug_rm"sv, false, CLIENT);
CONVAR(debug_bg_loader, "debug_bg_loader"sv, false, CLIENT);
CONVAR(debug_rt, "debug_rt"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "draws all rendertargets with a translucent green background"sv);
CONVAR(debug_shaders, "debug_shaders"sv, false, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(debug_vprof, "debug_vprof"sv, false, CLIENT | SERVER);
CONVAR(debug_opengl, "debug_opengl"sv, false, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(debug_snd, "debug_snd"sv, false, CLIENT | NOSAVE);
CONVAR(r_3dscene_zf, "r_3dscene_zf"sv, 5000.0f, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_3dscene_zn, "r_3dscene_zn"sv, 5.0f, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_debug_disable_3dscene, "r_debug_disable_3dscene"sv, false, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_debug_disable_cliprect, "r_debug_disable_cliprect"sv, false, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_debug_drawimage, "r_debug_drawimage"sv, false, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_debug_flush_drawstring, "r_debug_flush_drawstring"sv, false, CLIENT);
CONVAR(r_debug_drawstring_unbind, "r_debug_drawstring_unbind"sv, false, CLIENT);
CONVAR(r_debug_font_unicode, "r_debug_font_unicode"sv, false, CLIENT,
       "debug messages for unicode/fallback font related stuff"sv);
CONVAR(r_sync_timeout, "r_sync_timeout"sv, 5000000, CLIENT,
       "timeout in microseconds for GPU synchronization operations"sv);
CONVAR(r_sync_enabled, "r_sync_enabled"sv, true, CLIENT, "enable explicit GPU synchronization for OpenGL"sv);
CONVAR(r_opengl_legacy_vao_use_vertex_array, "r_opengl_legacy_vao_use_vertex_array"sv,
       Env::cfg(REND::GLES32) ? true : false, CLIENT,
       "dramatically reduces per-vao draw calls, but completely breaks legacy ffp draw calls (vertices work, but "
       "texcoords/normals/etc. are NOT in gl_MultiTexCoord0 -> requiring a shader with attributes)"sv);
CONVAR(font_load_system, "font_load_system"sv, true, CLIENT,
       "try to load a similar system font if a glyph is missing in the bundled fonts"sv);
CONVAR(r_image_unbind_after_drawimage, "r_image_unbind_after_drawimage"sv, true, CLIENT);
CONVAR(r_globaloffset_x, "r_globaloffset_x"sv, 0.0f, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_globaloffset_y, "r_globaloffset_y"sv, 0.0f, CLIENT | PROTECTED | GAMEPLAY);
CONVAR(r_sync_debug, "r_sync_debug"sv, false, CLIENT | HIDDEN, "print debug information about sync objects"sv);
CONVAR(r_gpuupload_debug, "r_gpuupload_debug", false, CLIENT, "more verbose logging for async GPU uploads");
CONVAR(r_async_gpu, "r_async_gpu", true, CLIENT,
       "enable asynchronous GPU texture uploads for fewer lag spikes when loading images");
CONVAR(slider_debug_draw, "slider_debug_draw"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "draw hitcircle at every curve point and nothing else (no vao, no rt, no shader, nothing) "
       "(requires enabling legacy slider renderer)"sv);
CONVAR(slider_debug_draw_square_vao, "slider_debug_draw_square_vao"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "generate square vaos and nothing else (no rt, no shader) (requires disabling legacy slider renderer)"sv);
CONVAR(slider_debug_wireframe, "slider_debug_wireframe"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY, "unused"sv);
CONVAR(vprof, "vprof"sv, false, CLIENT | SERVER, "enables/disables the visual profiler"sv,
       CFUNC(Profiling::vprofToggleCB));
CONVAR(vprof_display_mode, "vprof_display_mode"sv, 0, CLIENT | SERVER,
       "which info blade to show on the top right (gpu/engine/app/etc. info), use CTRL + TAB to "
       "cycle through, 0 = disabled"sv);
CONVAR(vprof_graph, "vprof_graph"sv, true, CLIENT | SERVER, "whether to draw the graph when the overlay is enabled"sv);
CONVAR(vprof_graph_alpha, "vprof_graph_alpha"sv, 0.9f, CLIENT | SERVER, "line opacity"sv);
CONVAR(vprof_graph_draw_overhead, "vprof_graph_draw_overhead"sv, false, CLIENT | SERVER,
       "whether to draw the profiling overhead time in white (usually negligible)"sv);
CONVAR(vprof_graph_height, "vprof_graph_height"sv, 250.0f, CLIENT | SERVER);
CONVAR(vprof_graph_margin, "vprof_graph_margin"sv, 40.0f, CLIENT | SERVER);
CONVAR(vprof_graph_range_max, "vprof_graph_range_max"sv, 16.666666f, CLIENT | SERVER,
       "max value of the y-axis in milliseconds"sv);
CONVAR(vprof_graph_width, "vprof_graph_width"sv, 800.0f, CLIENT | SERVER);
CONVAR(vprof_spike, "vprof_spike"sv, 0, CLIENT | SERVER,
       "measure and display largest spike details (1 = small info, 2 = extended info)"sv);

// Keybinds
CONVAR(BOSS_KEY, "key_boss"sv, (int)KEY_INSERT, CLIENT);
CONVAR(DECREASE_LOCAL_OFFSET, "key_decrease_local_offset"sv, (int)KEY_SUBTRACT, CLIENT);
CONVAR(DECREASE_VOLUME, "key_decrease_volume"sv, (int)KEY_DOWN, CLIENT);
CONVAR(DISABLE_MOUSE_BUTTONS, "key_disable_mouse_buttons"sv, (int)KEY_F10, CLIENT);
CONVAR(FPOSU_ZOOM, "key_fposu_zoom"sv, 0, CLIENT);
CONVAR(GAME_PAUSE, "key_game_pause"sv, (int)KEY_ESCAPE, CLIENT);
CONVAR(INCREASE_LOCAL_OFFSET, "key_increase_local_offset"sv, (int)KEY_ADD, CLIENT);
CONVAR(INCREASE_VOLUME, "key_increase_volume"sv, (int)KEY_UP, CLIENT);
CONVAR(INSTANT_REPLAY, "key_instant_replay"sv, (int)KEY_F2, CLIENT);
CONVAR(LEFT_CLICK, "key_left_click"sv, (int)KEY_Z, CLIENT);
CONVAR(LEFT_CLICK_2, "key_left_click_2"sv, 0, CLIENT);
CONVAR(MOD_AUTO, "key_mod_auto"sv, (int)KEY_V, CLIENT);
CONVAR(MOD_AUTOPILOT, "key_mod_autopilot"sv, (int)KEY_X, CLIENT);
CONVAR(MOD_DOUBLETIME, "key_mod_doubletime"sv, (int)KEY_D, CLIENT);
CONVAR(MOD_EASY, "key_mod_easy"sv, (int)KEY_Q, CLIENT);
CONVAR(MOD_FLASHLIGHT, "key_mod_flashlight"sv, (int)KEY_G, CLIENT);
CONVAR(MOD_HALFTIME, "key_mod_halftime"sv, (int)KEY_E, CLIENT);
CONVAR(MOD_HARDROCK, "key_mod_hardrock"sv, (int)KEY_A, CLIENT);
CONVAR(MOD_HIDDEN, "key_mod_hidden"sv, (int)KEY_F, CLIENT);
CONVAR(MOD_NOFAIL, "key_mod_nofail"sv, (int)KEY_W, CLIENT);
CONVAR(MOD_RELAX, "key_mod_relax"sv, (int)KEY_Z, CLIENT);
CONVAR(MOD_SCOREV2, "key_mod_scorev2"sv, (int)KEY_B, CLIENT);
CONVAR(MOD_SPUNOUT, "key_mod_spunout"sv, (int)KEY_C, CLIENT);
CONVAR(MOD_SUDDENDEATH, "key_mod_suddendeath"sv, (int)KEY_S, CLIENT);
CONVAR(OPEN_SKIN_SELECT_MENU, "key_open_skin_select_menu"sv, 0, CLIENT);
CONVAR(QUICK_LOAD, "key_quick_load"sv, (int)KEY_F7, CLIENT);
CONVAR(QUICK_RETRY, "key_quick_retry"sv, (int)KEY_BACKSPACE, CLIENT);
CONVAR(QUICK_SAVE, "key_quick_save"sv, (int)KEY_F6, CLIENT);
CONVAR(RANDOM_BEATMAP, "key_random_beatmap"sv, (int)KEY_F2, CLIENT);
CONVAR(RIGHT_CLICK, "key_right_click"sv, (int)KEY_X, CLIENT);
CONVAR(RIGHT_CLICK_2, "key_right_click_2"sv, 0, CLIENT);
CONVAR(SAVE_SCREENSHOT, "key_save_screenshot"sv, (int)KEY_F12, CLIENT);
CONVAR(SEEK_TIME, "key_seek_time"sv, (int)KEY_LSHIFT, CLIENT);
CONVAR(SEEK_TIME_BACKWARD, "key_seek_time_backward"sv, (int)KEY_LEFT, CLIENT);
CONVAR(SEEK_TIME_FORWARD, "key_seek_time_forward"sv, (int)KEY_RIGHT, CLIENT);
CONVAR(SMOKE, "key_smoke"sv, 0, CLIENT);
CONVAR(SKIP_CUTSCENE, "key_skip_cutscene"sv, (int)KEY_SPACE, CLIENT);
CONVAR(TOGGLE_CHAT, "key_toggle_chat"sv, (int)KEY_F8, CLIENT);
CONVAR(TOGGLE_EXTENDED_CHAT, "key_toggle_extended_chat"sv, (int)KEY_F9, CLIENT);
CONVAR(TOGGLE_MAP_BACKGROUND, "key_toggle_map_background"sv, 0, CLIENT);
CONVAR(TOGGLE_MODSELECT, "key_toggle_modselect"sv, (int)KEY_F1, CLIENT);
CONVAR(TOGGLE_SCOREBOARD, "key_toggle_scoreboard"sv, (int)KEY_TAB, CLIENT);

// Input behavior
CONVAR(alt_f4_quits_even_while_playing, "alt_f4_quits_even_while_playing"sv, true, CLIENT);
CONVAR(auto_and_relax_block_user_input, "auto_and_relax_block_user_input"sv, true, CLIENT);
CONVAR(mod_suddendeath_restart, "mod_suddendeath_restart"sv, false, CLIENT,
       "osu! has this set to false (i.e. you fail after missing). if set to true, then "
       "behave like SS/PF, instantly restarting the map"sv);
CONVAR(hud_shift_tab_toggles_everything, "hud_shift_tab_toggles_everything"sv, true, CLIENT);
CONVAR(win_disable_windows_key_while_playing, "win_disable_windows_key_while_playing"sv, true, CLIENT);

// Files
CONVAR(database_enabled, "database_enabled"sv, true, CLIENT);
CONVAR(database_ignore_version, "database_ignore_version"sv, true, CLIENT,
       "ignore upper version limit and force load the db file (may crash)"sv);
CONVAR(database_version, "database_version"sv, OSU_VERSION_DATESTR, CLIENT | NOLOAD | NOSAVE,
       "maximum supported osu!.db version, above this will use fallback loader"sv);
CONVAR(osu_folder, "osu_folder"sv, ""sv, CLIENT);
CONVAR(osu_folder_sub_skins, "osu_folder_sub_skins"sv, "Skins/"sv, CLIENT);
CONVAR(songs_folder, "songs_folder"sv, "Songs/"sv, CLIENT);

// Looks
CONVAR(always_render_cursor_trail, "always_render_cursor_trail"sv, true, CLIENT | SKINS,
       "always render the cursor trail, even when not moving the cursor"sv);
CONVAR(automatic_cursor_size, "automatic_cursor_size"sv, false, CLIENT | SKINS);
CONVAR(mod_hd_circle_fadeout_end_percent, "mod_hd_circle_fadeout_end_percent"sv, 0.3f,
       CLIENT | SKINS | SERVER | GAMEPLAY,
       "hiddenFadeOutEndTime = circleTime - approachTime * mod_hd_circle_fadeout_end_percent"sv);
CONVAR(mod_hd_circle_fadeout_start_percent, "mod_hd_circle_fadeout_start_percent"sv, 0.6f,
       CLIENT | SKINS | SERVER | GAMEPLAY,
       "hiddenFadeOutStartTime = circleTime - approachTime * mod_hd_circle_fadeout_start_percent"sv);
CONVAR(mod_hd_slider_fade_percent, "mod_hd_slider_fade_percent"sv, 1.0f, CLIENT | SKINS | SERVER | GAMEPLAY);
CONVAR(mod_hd_slider_fast_fade, "mod_hd_slider_fast_fade"sv, false, CLIENT | SKINS | SERVER | GAMEPLAY);

// Song browser
CONVAR(draw_songbrowser_background_image, "draw_songbrowser_background_image"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_songbrowser_menu_background_image, "draw_songbrowser_menu_background_image"sv, true,
       CLIENT | SKINS | SERVER);
CONVAR(draw_songbrowser_strain_graph, "draw_songbrowser_strain_graph"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_songbrowser_thumbnails, "draw_songbrowser_thumbnails"sv, true, CLIENT | SKINS | SERVER);
CONVAR(songbrowser_background_fade_in_duration, "songbrowser_background_fade_in_duration"sv, 0.1f, CLIENT | SKINS);
CONVAR(songbrowser_button_active_color_a, "songbrowser_button_active_color_a"sv, 230, CLIENT | SKINS);
CONVAR(songbrowser_button_active_color_b, "songbrowser_button_active_color_b"sv, 255, CLIENT | SKINS);
CONVAR(songbrowser_button_active_color_g, "songbrowser_button_active_color_g"sv, 255, CLIENT | SKINS);
CONVAR(songbrowser_button_active_color_r, "songbrowser_button_active_color_r"sv, 255, CLIENT | SKINS);
CONVAR(songbrowser_button_collection_active_color_a, "songbrowser_button_collection_active_color_a"sv, 255,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_active_color_b, "songbrowser_button_collection_active_color_b"sv, 44,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_active_color_g, "songbrowser_button_collection_active_color_g"sv, 240,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_active_color_r, "songbrowser_button_collection_active_color_r"sv, 163,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_inactive_color_a, "songbrowser_button_collection_inactive_color_a"sv, 255,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_inactive_color_b, "songbrowser_button_collection_inactive_color_b"sv, 143,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_inactive_color_g, "songbrowser_button_collection_inactive_color_g"sv, 50,
       CLIENT | SKINS);
CONVAR(songbrowser_button_collection_inactive_color_r, "songbrowser_button_collection_inactive_color_r"sv, 35,
       CLIENT | SKINS);
CONVAR(songbrowser_button_difficulty_inactive_color_a, "songbrowser_button_difficulty_inactive_color_a"sv, 255,
       CLIENT | SKINS);
CONVAR(songbrowser_button_difficulty_inactive_color_b, "songbrowser_button_difficulty_inactive_color_b"sv, 236,
       CLIENT | SKINS);
CONVAR(songbrowser_button_difficulty_inactive_color_g, "songbrowser_button_difficulty_inactive_color_g"sv, 150,
       CLIENT | SKINS);
CONVAR(songbrowser_button_difficulty_inactive_color_r, "songbrowser_button_difficulty_inactive_color_r"sv, 0,
       CLIENT | SKINS);
CONVAR(songbrowser_button_inactive_color_a, "songbrowser_button_inactive_color_a"sv, 240, CLIENT | SKINS);
CONVAR(songbrowser_button_inactive_color_b, "songbrowser_button_inactive_color_b"sv, 153, CLIENT | SKINS);
CONVAR(songbrowser_button_inactive_color_g, "songbrowser_button_inactive_color_g"sv, 73, CLIENT | SKINS);
CONVAR(songbrowser_button_inactive_color_r, "songbrowser_button_inactive_color_r"sv, 235, CLIENT | SKINS);
CONVAR(songbrowser_thumbnail_delay, "songbrowser_thumbnail_delay"sv, 0.1f, CLIENT | SKINS);
CONVAR(songbrowser_thumbnail_fade_in_duration, "songbrowser_thumbnail_fade_in_duration"sv, 0.1f, CLIENT | SKINS);

// Song browser (client only)
CONVAR(prefer_cjk, "prefer_cjk"sv, false, CLIENT, "prefer metadata in original language"sv);
CONVAR(songbrowser_search_delay, "songbrowser_search_delay"sv, 0.2f, CLIENT,
       "delay until search update when entering text"sv);
CONVAR(songbrowser_search_hardcoded_filter, "songbrowser_search_hardcoded_filter"sv, ""sv, CLIENT,
       "allows forcing the specified search filter to be active all the time"sv);

// Song browser (maybe useful to servers)
CONVAR(songbrowser_scorebrowser_enabled, "songbrowser_scorebrowser_enabled"sv, true, CLIENT | SKINS | SERVER);
CONVAR(songbrowser_scores_filteringtype, "songbrowser_scores_filteringtype"sv, "Local"sv, CLIENT | SKINS | SERVER);
CONVAR(songbrowser_scores_filteringtype_manual, "songbrowser_scores_filteringtype_manual"sv, "unset"sv,
       CLIENT | SKINS | SERVER);
CONVAR(songbrowser_scores_sortingtype, "songbrowser_scores_sortingtype"sv, "By pp"sv, CLIENT | SKINS | SERVER);
CONVAR(songbrowser_sortingtype, "songbrowser_sortingtype"sv, "By Date Added"sv, CLIENT | SKINS | SERVER);

// Playfield
CONVAR(background_alpha, "background_alpha"sv, 1.0f, CLIENT | SKINS | SERVER,
       "transparency of all background layers at once, only useful for FPoSu"sv);
CONVAR(background_brightness, "background_brightness"sv, 0.0f, CLIENT | SKINS | SERVER,
       "0 to 1, if this is larger than 0 then it will replace the entire beatmap background "
       "image with a solid color (see background_color_r/g/b)"sv);
CONVAR(background_color_b, "background_color_b"sv, 255.0f, CLIENT | SKINS | SERVER,
       "0 to 255, only relevant if background_brightness is larger than 0"sv);
CONVAR(background_color_g, "background_color_g"sv, 255.0f, CLIENT | SKINS | SERVER,
       "0 to 255, only relevant if background_brightness is larger than 0"sv);
CONVAR(background_color_r, "background_color_r"sv, 255.0f, CLIENT | SKINS | SERVER,
       "0 to 255, only relevant if background_brightness is larger than 0"sv);
CONVAR(background_dim, "background_dim"sv, 0.9f, CLIENT | SKINS | SERVER);
CONVAR(background_dont_fade_during_breaks, "background_dont_fade_during_breaks"sv, false, CLIENT | SKINS | SERVER);
CONVAR(background_fade_after_load, "background_fade_after_load"sv, true, CLIENT | SKINS | SERVER);
CONVAR(background_fade_in_duration, "background_fade_in_duration"sv, 0.85f, CLIENT | SKINS | SERVER);
CONVAR(background_fade_min_duration, "background_fade_min_duration"sv, 1.4f, CLIENT | SKINS | SERVER,
       "Only fade if the break is longer than this (in seconds)"sv);
CONVAR(background_fade_out_duration, "background_fade_out_duration"sv, 0.25f, CLIENT | SKINS | SERVER);
CONVAR(draw_accuracy, "draw_accuracy"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_approach_circles, "draw_approach_circles"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_beatmap_background_image, "draw_beatmap_background_image"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_circles, "draw_circles"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_combo, "draw_combo"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_continue, "draw_continue"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_cursor_ripples, "draw_cursor_ripples"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_cursor_trail, "draw_cursor_trail"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_followpoints, "draw_followpoints"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_fps, "draw_fps"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar, "draw_hiterrorbar"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar_bottom, "draw_hiterrorbar_bottom"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar_left, "draw_hiterrorbar_left"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar_right, "draw_hiterrorbar_right"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar_top, "draw_hiterrorbar_top"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_hiterrorbar_ur, "draw_hiterrorbar_ur"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_hitobjects, "draw_hitobjects"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_hud, "draw_hud"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_inputoverlay, "draw_inputoverlay"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_numbers, "draw_numbers"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_playfield_border, "draw_playfield_border"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_progressbar, "draw_progressbar"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_rankingscreen_background_image, "draw_rankingscreen_background_image"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_score, "draw_score"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scorebar, "draw_scorebar"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scorebarbg, "draw_scorebarbg"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scoreboard, "draw_scoreboard"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scoreboard_mp, "draw_scoreboard_mp"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scrubbing_timeline, "draw_scrubbing_timeline"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scrubbing_timeline_breaks, "draw_scrubbing_timeline_breaks"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_scrubbing_timeline_strain_graph, "draw_scrubbing_timeline_strain_graph"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_smoke, "draw_smoke"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_spectator_background_image, "draw_spectator_background_image"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_spectator_list, "draw_spectator_list"sv, true, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_ar, "draw_statistics_ar"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_bpm, "draw_statistics_bpm"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_cs, "draw_statistics_cs"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_hitdelta, "draw_statistics_hitdelta"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_hitwindow300, "draw_statistics_hitwindow300"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_hp, "draw_statistics_hp"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_livestars, "draw_statistics_livestars"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_maxpossiblecombo, "draw_statistics_maxpossiblecombo"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_misses, "draw_statistics_misses"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_nd, "draw_statistics_nd"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_nps, "draw_statistics_nps"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_od, "draw_statistics_od"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_perfectpp, "draw_statistics_perfectpp"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_pp, "draw_statistics_pp"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_sliderbreaks, "draw_statistics_sliderbreaks"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_totalstars, "draw_statistics_totalstars"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_statistics_ur, "draw_statistics_ur"sv, false, CLIENT | SKINS | SERVER);
CONVAR(draw_target_heatmap, "draw_target_heatmap"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hud_accuracy_scale, "hud_accuracy_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_combo_scale, "hud_combo_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_fps_smoothing, "hud_fps_smoothing"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_alpha, "hud_hiterrorbar_alpha"sv, 1.0f, CLIENT | SKINS | SERVER,
       "opacity multiplier for entire hiterrorbar"sv);
CONVAR(hud_hiterrorbar_bar_alpha, "hud_hiterrorbar_bar_alpha"sv, 1.0f, CLIENT | SKINS | SERVER,
       "opacity multiplier for background color bar"sv);
CONVAR(hud_hiterrorbar_bar_height_scale, "hud_hiterrorbar_bar_height_scale"sv, 3.4f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_bar_width_scale, "hud_hiterrorbar_bar_width_scale"sv, 0.6f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_centerline_alpha, "hud_hiterrorbar_centerline_alpha"sv, 1.0f, CLIENT | SKINS | SERVER,
       "opacity multiplier for center line"sv);
CONVAR(hud_hiterrorbar_centerline_b, "hud_hiterrorbar_centerline_b"sv, 255, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_centerline_g, "hud_hiterrorbar_centerline_g"sv, 255, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_centerline_r, "hud_hiterrorbar_centerline_r"sv, 255, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_100_b, "hud_hiterrorbar_entry_100_b"sv, 19, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_100_g, "hud_hiterrorbar_entry_100_g"sv, 227, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_100_r, "hud_hiterrorbar_entry_100_r"sv, 87, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_300_b, "hud_hiterrorbar_entry_300_b"sv, 231, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_300_g, "hud_hiterrorbar_entry_300_g"sv, 188, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_300_r, "hud_hiterrorbar_entry_300_r"sv, 50, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_50_b, "hud_hiterrorbar_entry_50_b"sv, 70, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_50_g, "hud_hiterrorbar_entry_50_g"sv, 174, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_50_r, "hud_hiterrorbar_entry_50_r"sv, 218, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_additive, "hud_hiterrorbar_entry_additive"sv, true, CLIENT | SKINS | SERVER,
       "whether to use additive blending for all hit error entries/lines"sv);
CONVAR(hud_hiterrorbar_entry_alpha, "hud_hiterrorbar_entry_alpha"sv, 0.75f, CLIENT | SKINS | SERVER,
       "opacity multiplier for all hit error entries/lines"sv);
CONVAR(hud_hiterrorbar_entry_hit_fade_time, "hud_hiterrorbar_entry_hit_fade_time"sv, 6.0f, CLIENT | SKINS | SERVER,
       "fade duration of 50/100/300 hit entries/lines in seconds"sv);
CONVAR(hud_hiterrorbar_entry_miss_b, "hud_hiterrorbar_entry_miss_b"sv, 0, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_miss_fade_time, "hud_hiterrorbar_entry_miss_fade_time"sv, 4.0f, CLIENT | SKINS | SERVER,
       "fade duration of miss entries/lines in seconds"sv);
CONVAR(hud_hiterrorbar_entry_miss_g, "hud_hiterrorbar_entry_miss_g"sv, 0, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_entry_miss_r, "hud_hiterrorbar_entry_miss_r"sv, 205, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_height_percent, "hud_hiterrorbar_height_percent"sv, 0.007f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_hide_during_spinner, "hud_hiterrorbar_hide_during_spinner"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_max_entries, "hud_hiterrorbar_max_entries"sv, 32, CLIENT | SKINS | SERVER,
       "maximum number of entries/lines"sv);
CONVAR(hud_hiterrorbar_offset_bottom_percent, "hud_hiterrorbar_offset_bottom_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_offset_left_percent, "hud_hiterrorbar_offset_left_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_offset_percent, "hud_hiterrorbar_offset_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_offset_right_percent, "hud_hiterrorbar_offset_right_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_offset_top_percent, "hud_hiterrorbar_offset_top_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_scale, "hud_hiterrorbar_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_showmisswindow, "hud_hiterrorbar_showmisswindow"sv, false, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_ur_alpha, "hud_hiterrorbar_ur_alpha"sv, 0.5f, CLIENT | SKINS | SERVER,
       "opacity multiplier for unstable rate text above hiterrorbar"sv);
CONVAR(hud_hiterrorbar_ur_offset_x_percent, "hud_hiterrorbar_ur_offset_x_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_ur_offset_y_percent, "hud_hiterrorbar_ur_offset_y_percent"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_ur_scale, "hud_hiterrorbar_ur_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_width_percent, "hud_hiterrorbar_width_percent"sv, 0.15f, CLIENT | SKINS | SERVER);
CONVAR(hud_hiterrorbar_width_percent_with_misswindow, "hud_hiterrorbar_width_percent_with_misswindow"sv, 0.4f,
       CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_anim_color_duration, "hud_inputoverlay_anim_color_duration"sv, 0.1f, CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_anim_scale_duration, "hud_inputoverlay_anim_scale_duration"sv, 0.16f, CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_anim_scale_multiplier, "hud_inputoverlay_anim_scale_multiplier"sv, 0.8f,
       CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_offset_x, "hud_inputoverlay_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_offset_y, "hud_inputoverlay_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_inputoverlay_scale, "hud_inputoverlay_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_playfield_border_size, "hud_playfield_border_size"sv, 5.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_progressbar_scale, "hud_progressbar_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_scale, "hud_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_score_scale, "hud_score_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_scorebar_hide_anim_duration, "hud_scorebar_hide_anim_duration"sv, 0.5f, CLIENT | SKINS | SERVER);
CONVAR(hud_scorebar_hide_during_breaks, "hud_scorebar_hide_during_breaks"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hud_scorebar_scale, "hud_scorebar_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_scoreboard_offset_y_percent, "hud_scoreboard_offset_y_percent"sv, 0.11f, CLIENT | SKINS | SERVER);
CONVAR(hud_scoreboard_scale, "hud_scoreboard_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_scoreboard_use_menubuttonbackground, "hud_scoreboard_use_menubuttonbackground"sv, true,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_hover_tooltip_offset_multiplier,
       "hud_scrubbing_timeline_hover_tooltip_offset_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_aim_color_b, "hud_scrubbing_timeline_strains_aim_color_b"sv, 0,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_aim_color_g, "hud_scrubbing_timeline_strains_aim_color_g"sv, 255,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_aim_color_r, "hud_scrubbing_timeline_strains_aim_color_r"sv, 0,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_alpha, "hud_scrubbing_timeline_strains_alpha"sv, 0.4f, CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_height, "hud_scrubbing_timeline_strains_height"sv, 200.0f,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_speed_color_b, "hud_scrubbing_timeline_strains_speed_color_b"sv, 0,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_speed_color_g, "hud_scrubbing_timeline_strains_speed_color_g"sv, 0,
       CLIENT | SKINS | SERVER);
CONVAR(hud_scrubbing_timeline_strains_speed_color_r, "hud_scrubbing_timeline_strains_speed_color_r"sv, 255,
       CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_ar_offset_x, "hud_statistics_ar_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_ar_offset_y, "hud_statistics_ar_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_bpm_offset_x, "hud_statistics_bpm_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_bpm_offset_y, "hud_statistics_bpm_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_cs_offset_x, "hud_statistics_cs_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_cs_offset_y, "hud_statistics_cs_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hitdelta_chunksize, "hud_statistics_hitdelta_chunksize"sv, 30, CLIENT | SKINS | SERVER,
       "how many recent hit deltas to average (-1 = all)"sv);
CONVAR(hud_statistics_hitdelta_offset_x, "hud_statistics_hitdelta_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hitdelta_offset_y, "hud_statistics_hitdelta_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hitwindow300_offset_x, "hud_statistics_hitwindow300_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hitwindow300_offset_y, "hud_statistics_hitwindow300_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hp_offset_x, "hud_statistics_hp_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_hp_offset_y, "hud_statistics_hp_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_livestars_offset_x, "hud_statistics_livestars_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_livestars_offset_y, "hud_statistics_livestars_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_maxpossiblecombo_offset_x, "hud_statistics_maxpossiblecombo_offset_x"sv, 0.0f,
       CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_maxpossiblecombo_offset_y, "hud_statistics_maxpossiblecombo_offset_y"sv, 0.0f,
       CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_misses_offset_x, "hud_statistics_misses_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_misses_offset_y, "hud_statistics_misses_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_nd_offset_x, "hud_statistics_nd_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_nd_offset_y, "hud_statistics_nd_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_nps_offset_x, "hud_statistics_nps_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_nps_offset_y, "hud_statistics_nps_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_od_offset_x, "hud_statistics_od_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_od_offset_y, "hud_statistics_od_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_offset_x, "hud_statistics_offset_x"sv, 5.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_offset_y, "hud_statistics_offset_y"sv, 50.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_perfectpp_offset_x, "hud_statistics_perfectpp_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_perfectpp_offset_y, "hud_statistics_perfectpp_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_pp_decimal_places, "hud_statistics_pp_decimal_places"sv, 0, CLIENT | SKINS | SERVER,
       "number of decimal places for the live pp counter (min = 0, max = 2)"sv);
CONVAR(hud_statistics_pp_offset_x, "hud_statistics_pp_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_pp_offset_y, "hud_statistics_pp_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_scale, "hud_statistics_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_sliderbreaks_offset_x, "hud_statistics_sliderbreaks_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_sliderbreaks_offset_y, "hud_statistics_sliderbreaks_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_spacing_scale, "hud_statistics_spacing_scale"sv, 1.1f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_totalstars_offset_x, "hud_statistics_totalstars_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_totalstars_offset_y, "hud_statistics_totalstars_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_ur_offset_x, "hud_statistics_ur_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_statistics_ur_offset_y, "hud_statistics_ur_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_volume_duration, "hud_volume_duration"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(hud_volume_size_multiplier, "hud_volume_size_multiplier"sv, 1.5f, CLIENT | SKINS | SERVER);
CONVAR(playfield_border_bottom_percent, "playfield_border_bottom_percent"sv, 0.0834f, CLIENT | SERVER | GAMEPLAY);
CONVAR(playfield_border_top_percent, "playfield_border_top_percent"sv, 0.117f, CLIENT | SERVER | GAMEPLAY);
CONVAR(playfield_mirror_horizontal, "playfield_mirror_horizontal"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(playfield_mirror_vertical, "playfield_mirror_vertical"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(playfield_rotation, "playfield_rotation"sv, 0.f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "rotates the entire playfield by this many degrees"sv);
CONVAR(smoke_scale, "smoke_scale"sv, 1.f, CLIENT | SKINS | SERVER);
CONVAR(smoke_trail_duration, "smoke_trail_duration"sv, 10.f, CLIENT | SKINS | SERVER,
       "how long smoke trails should last before being completely gone, in seconds"sv);
CONVAR(smoke_trail_max_size, "smoke_trail_max_size"sv, 2048, CLIENT | SKINS | SERVER,
       "maximum number of rendered smoke trail images, array size limit"sv);
CONVAR(smoke_trail_opaque_duration, "smoke_trail_opaque_duration"sv, 7.f, CLIENT | SKINS | SERVER,
       "how long smoke trails should last before starting to fade out, in seconds"sv);
CONVAR(smoke_trail_spacing, "smoke_trail_spacing"sv, 5.f, CLIENT | SKINS | SERVER,
       "how big the gap between smoke particles should be, in milliseconds"sv);

// Hitobjects
CONVAR(approach_circle_alpha_multiplier, "approach_circle_alpha_multiplier"sv, 0.9f,
       CLIENT | SKINS | SERVER | GAMEPLAY);
CONVAR(approach_scale_multiplier, "approach_scale_multiplier"sv, 3.0f, CLIENT | SKINS | SERVER | GAMEPLAY);

// Vanilla mods
CONVAR(mod_hidden, "mod_hidden"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_autoplay, "mod_autoplay"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_autopilot, "mod_autopilot"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_relax, "mod_relax"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_spunout, "mod_spunout"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_target, "mod_target"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_scorev2, "mod_scorev2"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_flashlight, "mod_flashlight"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_doubletime, "mod_doubletime"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_nofail, "mod_nofail"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_hardrock, "mod_hardrock"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_easy, "mod_easy"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_suddendeath, "mod_suddendeath"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_perfect, "mod_perfect"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_touchdevice, "mod_touchdevice"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_touchdevice_always, "mod_touchdevice_always"sv, false, CLIENT | SERVER | GAMEPLAY,
       "always enable touchdevice mod"sv);
// speed_override: Even though it isn't PROTECTED, only (0.75, 1.0, 1.5) are allowed on bancho servers.
CONVAR(speed_override, "speed_override"sv, -1.0f, CLIENT | SERVER | GAMEPLAY);
// mod_*time_dummy: These don't affect gameplay, but edit speed_override.
CONVAR(mod_doubletime_dummy, "mod_doubletime_dummy"sv, false, CLIENT | SKINS | SERVER);
CONVAR(mod_halftime_dummy, "mod_halftime_dummy"sv, false, CLIENT | SKINS | SERVER);

// Non-vanilla mods
CONVAR(ar_override, "ar_override"sv, -1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "use this to override between AR 0 and AR 12.5+. active if value is more than or equal to 0."sv);
CONVAR(ar_override_lock, "ar_override_lock"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "always force constant approach time even through speed changes"sv);
CONVAR(ar_overridenegative, "ar_overridenegative"sv, 0.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "use this to override below AR 0. active if value is less than 0, disabled otherwise. "
       "this override always overrides the other override."sv);
CONVAR(autopilot_lenience, "autopilot_lenience"sv, 0.75f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(autopilot_snapping_strength, "autopilot_snapping_strength"sv, 2.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "How many iterations of quadratic interpolation to use, more = snappier, 0 = linear"sv);
CONVAR(cs_override, "cs_override"sv, -1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "use this to override between CS 0 and CS 12.1429. active if value is more than or equal to 0."sv);
CONVAR(cs_overridenegative, "cs_overridenegative"sv, 0.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "use this to override below CS 0. active if value is less than 0, disabled otherwise. "
       "this override always overrides the other override."sv);
CONVAR(hp_override, "hp_override"sv, -1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_actual_flashlight, "mod_actual_flashlight"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_approach_different, "mod_approach_different"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "replicates osu!lazer's \"Approach Different\" mod"sv);
CONVAR(mod_approach_different_initial_size, "mod_approach_different_initial_size"sv, 4.0f, CLIENT | SERVER | GAMEPLAY,
       "initial size of the approach circles, relative to hit circles (as a multiplier)"sv);
CONVAR(mod_approach_different_style, "mod_approach_different_style"sv, 1, CLIENT | SERVER | GAMEPLAY,
       "0 = linear, 1 = gravity, 2 = InOut1, 3 = InOut2, 4 = Accelerate1, 5 = Accelerate2, 6 = Accelerate3, 7 = "
       "Decelerate1, 8 = Decelerate2, 9 = Decelerate3"sv);
CONVAR(mod_artimewarp, "mod_artimewarp"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_artimewarp_multiplier, "mod_artimewarp_multiplier"sv, 0.5f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_arwobble, "mod_arwobble"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_arwobble_interval, "mod_arwobble_interval"sv, 7.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_arwobble_strength, "mod_arwobble_strength"sv, 1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_endless, "mod_endless"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_fadingcursor, "mod_fadingcursor"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_fadingcursor_combo, "mod_fadingcursor_combo"sv, 50.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_fposu, "mod_fposu"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_fposu_sound_panning, "mod_fposu_sound_panning"sv, false, CLIENT, "see sound_panning"sv);
CONVAR(mod_fps, "mod_fps"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_fps_sound_panning, "mod_fps_sound_panning"sv, false, CLIENT, "see sound_panning"sv);
CONVAR(mod_fullalternate, "mod_fullalternate"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_halfwindow, "mod_halfwindow"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_halfwindow_allow_300s, "mod_halfwindow_allow_300s"sv, true, CLIENT | SERVER | GAMEPLAY,
       "should positive hit deltas be allowed within 300 range"sv);
CONVAR(mod_hd_circle_fadein_end_percent, "mod_hd_circle_fadein_end_percent"sv, 0.6f,
       CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "hiddenFadeInEndTime = circleTime - approachTime * mod_hd_circle_fadein_end_percent"sv);
CONVAR(mod_hd_circle_fadein_start_percent, "mod_hd_circle_fadein_start_percent"sv, 1.0f,
       CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "hiddenFadeInStartTime = circleTime - approachTime * mod_hd_circle_fadein_start_percent"sv);
CONVAR(mod_jigsaw1, "mod_jigsaw1"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_jigsaw2, "mod_jigsaw2"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_jigsaw_followcircle_radius_factor, "mod_jigsaw_followcircle_radius_factor"sv, 0.0f,
       CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_mafham, "mod_mafham"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_mafham_ignore_hittable_dim, "mod_mafham_ignore_hittable_dim"sv, true, CLIENT | SERVER | GAMEPLAY,
       "having hittable dim enabled makes it possible to \"read\" the beatmap by "
       "looking at the un-dim animations (thus making it a lot easier)"sv);
CONVAR(mod_mafham_render_chunksize, "mod_mafham_render_chunksize"sv, 15, CLIENT | SERVER | GAMEPLAY,
       "render this many hitobjects per frame chunk into the scene buffer (spreads "
       "rendering across many frames to minimize lag)"sv);
CONVAR(mod_mafham_render_livesize, "mod_mafham_render_livesize"sv, 25, CLIENT | SERVER | GAMEPLAY,
       "render this many hitobjects without any scene buffering, higher = more lag but more up-to-date scene"sv);
CONVAR(mod_millhioref, "mod_millhioref"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_millhioref_multiplier, "mod_millhioref_multiplier"sv, 2.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_ming3012, "mod_ming3012"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_minimize, "mod_minimize"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_minimize_multiplier, "mod_minimize_multiplier"sv, 0.5f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_no_keylock, "mod_no_keylock"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_no100s, "mod_no100s"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_no50s, "mod_no50s"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_nightmare, "mod_nightmare"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_reverse_sliders, "mod_reverse_sliders"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_shirone, "mod_shirone"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_shirone_combo, "mod_shirone_combo"sv, 20.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_singletap, "mod_singletap"sv, false, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_strict_tracking, "mod_strict_tracking"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_strict_tracking_remove_slider_ticks, "mod_strict_tracking_remove_slider_ticks"sv, false,
       CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "whether the strict tracking mod should remove slider ticks or not, "
       "this changed after its initial implementation in lazer"sv);
CONVAR(mod_target_100_percent, "mod_target_100_percent"sv, 0.7f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_target_300_percent, "mod_target_300_percent"sv, 0.5f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_target_50_percent, "mod_target_50_percent"sv, 0.95f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_timewarp, "mod_timewarp"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_timewarp_multiplier, "mod_timewarp_multiplier"sv, 1.5f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_wobble, "mod_wobble"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_wobble2, "mod_wobble2"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(mod_wobble_frequency, "mod_wobble_frequency"sv, 1.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_wobble_rotation_speed, "mod_wobble_rotation_speed"sv, 1.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(mod_wobble_strength, "mod_wobble_strength"sv, 25.0f, CLIENT | SERVER | GAMEPLAY);

// Important gameplay values
CONVAR(animation_speed_override, "animation_speed_override"sv, -1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(approachtime_max, "approachtime_max"sv, 450, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(approachtime_mid, "approachtime_mid"sv, 1200, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(approachtime_min, "approachtime_min"sv, 1800, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(cs_cap_sanity, "cs_cap_sanity"sv, true, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(skip_time, "skip_time"sv, 5000.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "Timeframe in ms within a beatmap which allows skipping if it doesn't contain any hitobjects"sv);

// Accessibility
CONVAR(avoid_flashes, "avoid_flashes"sv, false, CLIENT, "disable flashing elements (like FL dimming on sliders)"sv);

// Auto-updater
CONVAR(auto_update, "auto_update"sv, true, CLIENT);
CONVAR(bleedingedge, "bleedingedge"sv, false, CLIENT);
CONVAR(is_bleedingedge, "is_bleedingedge"sv, false, CLIENT | HIDDEN,
       "used by the updater to tell if it should nag the user to 'update' to the correct release stream"sv);

// Privacy settings
CONVAR(beatmap_mirror_override, "beatmap_mirror_override"sv, ""sv, CLIENT, "URL of custom beatmap download mirror"sv);
CONVAR(chat_auto_hide, "chat_auto_hide"sv, true, CLIENT, "automatically hide chat during gameplay"sv);
CONVAR(chat_highlight_words, "chat_highlight_words"sv, ""sv, CLIENT,
       "space-separated list of words to treat as a mention"sv);
CONVAR(chat_ignore_list, "chat_ignore_list"sv, ""sv, CLIENT, "space-separated list of words to ignore"sv);
CONVAR(chat_notify_on_dm, "chat_notify_on_dm"sv, true, CLIENT);
CONVAR(chat_notify_on_mention, "chat_notify_on_mention"sv, true, CLIENT, "get notified when someone says your name"sv);
CONVAR(chat_ping_on_mention, "chat_ping_on_mention"sv, true, CLIENT, "play a sound when someone says your name"sv);
CONVAR(chat_ticker, "chat_ticker"sv, true, CLIENT);

// Memes
CONVAR(auto_cursordance, "auto_cursordance"sv, false, CLIENT | SERVER);
CONVAR(auto_snapping_strength, "auto_snapping_strength"sv, 1.0f, CLIENT | SERVER,
       "How many iterations of quadratic interpolation to use, more = snappier, 0 = linear"sv);

// Performance tweaks
CONVAR(background_image_cache_size, "background_image_cache_size"sv, 32, CLIENT,
       "how many images can stay loaded in parallel"sv);
CONVAR(background_image_eviction_delay_frames, "background_image_eviction_delay_frames"sv, 60, CLIENT,
       "how many vsync frames to keep stale background images in the cache before deleting them"sv);
CONVAR(background_image_loading_delay, "background_image_loading_delay"sv, 0.1f, CLIENT,
       "how many seconds to wait until loading background images for visible beatmaps starts"sv);

// Display settings
CONVAR(fps_max, "fps_max"sv, 1000.0f, CLIENT, "framerate limiter, gameplay"sv);
CONVAR(fps_max_menu, "fps_max_menu"sv, 420.f, CLIENT, "framerate limiter, menus"sv);
CONVAR(fps_max_background, "fps_max_background"sv, 30.0f, CLIENT, "framerate limiter, background"sv);
CONVAR(fps_max_yield, "fps_max_yield"sv, false, CLIENT,
       "always release rest of timeslice once per frame (call scheduler via sleep(0))"sv);
// fps_unlimited: Unused since v39.01. Instead we just check if fps_max <= 0 (see MainMenu.cpp for migration)
CONVAR(fps_unlimited, "fps_unlimited"sv, false, CLIENT | HIDDEN | NOSAVE);
CONVAR(
    fps_unlimited_yield, "fps_unlimited_yield"sv, true, CLIENT,
    "always release rest of timeslice once per frame (call scheduler via sleep(0)), even if unlimited fps are enabled"sv);
CONVAR(fullscreen_windowed_borderless, "fullscreen_windowed_borderless"sv, false, CLIENT);
CONVAR(fullscreen, "fullscreen"sv, false, CLIENT,
       [](float /*newValue*/) -> void { engine ? engine->toggleFullscreen() : (void)0; });
CONVAR(monitor, "monitor"sv, 0, CLIENT, "monitor/display device to switch to, 0 = primary monitor"sv);
CONVAR(r_sync_max_frames, "r_sync_max_frames"sv, 1, CLIENT,
       "maximum pre-rendered frames allowed in rendering pipeline"sv);  // (a la "Max Prerendered Frames")
CONVAR(alt_sleep, "alt_sleep"sv, 1, CLIENT,
       "use an alternative sleep implementation (on Windows) for potentially more accurate frame limiting"sv);

// Constants (TODO: remove these)
CONVAR(
    beatmap_max_num_hitobjects, "beatmap_max_num_hitobjects"sv, 40000, CONSTANT,
    "maximum number of total allowed hitobjects per beatmap (prevent crashing on deliberate game-breaking beatmaps)"sv);
CONVAR(beatmap_max_num_slider_scoringtimes, "beatmap_max_num_slider_scoringtimes"sv, 32768, CONSTANT,
       "maximum number of slider score increase events allowed per slider "
       "(prevent crashing on deliberate game-breaking beatmaps)"sv);
CONVAR(build_timestamp, "build_timestamp"sv, BUILD_TIMESTAMP, CONSTANT);
CONVAR(debug_network, "debug_network"sv, false, CONSTANT);
CONVAR(slider_curve_max_length, "slider_curve_max_length"sv, 65536 / 2, CONSTANT,
       "maximum slider length in osu!pixels (i.e. pixelLength). also used to clamp all "
       "(control-)point coordinates to sane values."sv);
CONVAR(slider_curve_max_points, "slider_curve_max_points"sv, 9999.0f, CONSTANT,
       "maximum number of allowed interpolated curve points. quality will be forced to go "
       "down if a slider has more steps than this"sv);
CONVAR(slider_curve_points_separation, "slider_curve_points_separation"sv, 2.5f, CONSTANT,
       "slider body curve approximation step width in osu!pixels, don't set this lower than around 1.5"sv);
CONVAR(slider_end_inside_check_offset, "slider_end_inside_check_offset"sv, 36, CONSTANT,
       "offset in milliseconds going backwards from the end point, at which \"being "
       "inside the slider\" is checked. (osu bullshit behavior)"sv);
CONVAR(slider_max_repeats, "slider_max_repeats"sv, 9000, CONSTANT,
       "maximum number of repeats allowed per slider (clamp range)"sv);
CONVAR(slider_max_ticks, "slider_max_ticks"sv, 2048, CONSTANT,
       "maximum number of ticks allowed per slider (clamp range)"sv);
CONVAR(version, "version"sv, NEOSU_VERSION, CONSTANT);

// Sanity checks
CONVAR(beatmap_version, "beatmap_version"sv, 128, CLIENT,
       "maximum supported .osu file version, above this will simply not load (this was 14 but got "
       "bumped to 128 due to lazer backports)"sv);
CONVAR(r_drawstring_max_string_length, "r_drawstring_max_string_length"sv, 65536, CLIENT,
       "maximum number of characters per call, sanity/memory buffer limit"sv);

// Online
CONVAR(mp_autologin, "mp_autologin"sv, false, CLIENT);
CONVAR(mp_oauth_token, "mp_oauth_token"sv, ""sv, CLIENT | HIDDEN);
CONVAR(mp_password, "mp_password"sv, ""sv, CLIENT | HIDDEN | NOSAVE);
CONVAR(mp_password_md5, "mp_password_md5"sv, ""sv, CLIENT | HIDDEN);
CONVAR(mp_server, "mp_server"sv, "neosu.net"sv, CLIENT);
CONVAR(name, "name"sv, "Guest"sv, CLIENT);

// Server settings
CONVAR(sv_allow_speed_override, "sv_allow_speed_override"sv, false, SERVER,
       "let clients submit scores with non-vanilla speeds (e.g. not only HT/DT speed)"sv);

// Main menu
CONVAR(draw_menu_background, "draw_menu_background"sv, true, CLIENT | SKINS | SERVER);
CONVAR(main_menu_alpha, "main_menu_alpha"sv, 0.8f, CLIENT | SKINS | SERVER);
CONVAR(main_menu_friend, "main_menu_friend"sv, true, CLIENT | SKINS | SERVER);
CONVAR(main_menu_background_fade_duration, "main_menu_background_fade_duration"sv, 0.25f, CLIENT | SKINS | SERVER);
CONVAR(main_menu_startup_anim_duration, "main_menu_startup_anim_duration"sv, 0.25f, CLIENT | SKINS | SERVER);
CONVAR(main_menu_use_server_logo, "main_menu_use_server_logo"sv, true, CLIENT | SKINS | SERVER);

// Not sorted
CONVAR(beatmap_preview_mods_live, "beatmap_preview_mods_live"sv, false, CLIENT | SKINS | SERVER,
       "whether to immediately apply all currently selected mods while browsing beatmaps (e.g. speed/pitch)"sv);
CONVAR(beatmap_preview_music_loop, "beatmap_preview_music_loop"sv, true, CLIENT | SKINS | SERVER);
CONVAR(bug_flicker_log, "bug_flicker_log"sv, false, CLIENT | SKINS | SERVER);
CONVAR(notify_during_gameplay, "notify_during_gameplay"sv, false, CLIENT,
       "show notification popups instantly during gameplay"sv);
CONVAR(circle_color_saturation, "circle_color_saturation"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(circle_fade_out_scale, "circle_fade_out_scale"sv, 0.4f, CLIENT | SKINS | SERVER);
CONVAR(circle_number_rainbow, "circle_number_rainbow"sv, false, CLIENT | SKINS | SERVER);
CONVAR(circle_rainbow, "circle_rainbow"sv, false, CLIENT | SKINS | SERVER);
CONVAR(circle_shake_duration, "circle_shake_duration"sv, 0.120f, CLIENT | SKINS | SERVER);
CONVAR(circle_shake_strength, "circle_shake_strength"sv, 8.0f, CLIENT | SKINS | SERVER);
CONVAR(collections_custom_enabled, "collections_custom_enabled"sv, true, CLIENT | SKINS | SERVER,
       "load custom collections.db"sv);
CONVAR(collections_custom_version, "collections_custom_version"sv, 20220110, CLIENT | SKINS | SERVER,
       "maximum supported custom collections.db version"sv);
CONVAR(collections_legacy_enabled, "collections_legacy_enabled"sv, true, CLIENT | SKINS | SERVER,
       "load osu!'s collection.db"sv);
CONVAR(collections_save_immediately, "collections_save_immediately"sv, true, CLIENT | SKINS | SERVER,
       "write collections.db as soon as anything is changed"sv);
CONVAR(combo_anim1_duration, "combo_anim1_duration"sv, 0.15f, CLIENT | SKINS | SERVER);
CONVAR(combo_anim1_size, "combo_anim1_size"sv, 0.15f, CLIENT | SKINS | SERVER);
CONVAR(combo_anim2_duration, "combo_anim2_duration"sv, 0.4f, CLIENT | SKINS | SERVER);
CONVAR(combo_anim2_size, "combo_anim2_size"sv, 0.5f, CLIENT | SKINS | SERVER);
CONVAR(combobreak_sound_combo, "combobreak_sound_combo"sv, 20, CLIENT | SKINS | SERVER,
       "Only play the combobreak sound if the combo is higher than this"sv);
CONVAR(compensate_music_speed, "compensate_music_speed"sv, true, CLIENT | SKINS | SERVER,
       "compensates speeds slower than 1x a little bit, by adding an offset depending on the slowness"sv);
CONVAR(confine_cursor_fullscreen, "confine_cursor_fullscreen"sv, true, CLIENT | SKINS | SERVER);
CONVAR(confine_cursor_windowed, "confine_cursor_windowed"sv, false, CLIENT | SKINS | SERVER);
CONVAR(confine_cursor_never, "confine_cursor_never"sv, false, CLIENT | SKINS | SERVER);
CONVAR(console_logging, "console_logging"sv, true, CLIENT | SKINS | SERVER);
CONVAR(console_overlay, "console_overlay"sv, false, CLIENT | SKINS | SERVER,
       "should the log overlay always be visible (or only if the console is out)"sv);
CONVAR(console_overlay_lines, "console_overlay_lines"sv, 12, CLIENT | SKINS | SERVER, "max number of lines of text"sv);
CONVAR(console_overlay_scale, "console_overlay_scale"sv, 1.0f, CLIENT | SKINS | SERVER, "log text size multiplier"sv);
CONVAR(consolebox_animspeed, "consolebox_animspeed"sv, 12.0f, CLIENT | SKINS | SERVER);
CONVAR(consolebox_draw_helptext, "consolebox_draw_helptext"sv, true, CLIENT | SKINS | SERVER,
       "whether convar suggestions also draw their helptext"sv);
CONVAR(consolebox_draw_preview, "consolebox_draw_preview"sv, true, CLIENT | SKINS | SERVER,
       "whether the textbox shows the topmost suggestion while typing"sv);
CONVAR(crop_screenshots, "crop_screenshots"sv, true, CLIENT,
       "whether to crop screenshots to the letterboxed resolution"sv);
CONVAR(cursor_alpha, "cursor_alpha"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(cursor_expand_duration, "cursor_expand_duration"sv, 0.1f, CLIENT | SKINS | SERVER);
CONVAR(cursor_expand_scale_multiplier, "cursor_expand_scale_multiplier"sv, 1.3f, CLIENT | SKINS | SERVER);
CONVAR(cursor_ripple_additive, "cursor_ripple_additive"sv, true, CLIENT | SKINS | SERVER, "use additive blending"sv);
CONVAR(cursor_ripple_alpha, "cursor_ripple_alpha"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(cursor_ripple_anim_end_scale, "cursor_ripple_anim_end_scale"sv, 0.5f, CLIENT | SKINS | SERVER,
       "end size multiplier"sv);
CONVAR(cursor_ripple_anim_start_fadeout_delay, "cursor_ripple_anim_start_fadeout_delay"sv, 0.0f,
       CLIENT | SKINS | SERVER,
       "delay in seconds after which to start fading out (limited by cursor_ripple_duration of course)"sv);
CONVAR(cursor_ripple_anim_start_scale, "cursor_ripple_anim_start_scale"sv, 0.05f, CLIENT | SKINS | SERVER,
       "start size multiplier"sv);
CONVAR(cursor_ripple_duration, "cursor_ripple_duration"sv, 0.7f, CLIENT | SKINS | SERVER,
       "time in seconds each cursor ripple is visible"sv);
CONVAR(cursor_ripple_tint_b, "cursor_ripple_tint_b"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(cursor_ripple_tint_g, "cursor_ripple_tint_g"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(cursor_ripple_tint_r, "cursor_ripple_tint_r"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(cursor_scale, "cursor_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(cursor_trail_alpha, "cursor_trail_alpha"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(cursor_trail_expand, "cursor_trail_expand"sv, true, CLIENT | SKINS | SERVER,
       "if \"CursorExpand: 1\" in your skin.ini, whether the trail should then also expand or not"sv);
CONVAR(cursor_trail_length, "cursor_trail_length"sv, 0.17f, CLIENT | SKINS | SERVER,
       "how long unsmooth cursortrails should be, in seconds"sv);
CONVAR(cursor_trail_max_size, "cursor_trail_max_size"sv, 2048, CLIENT | SKINS | SERVER,
       "maximum number of rendered trail images, array size limit"sv);
CONVAR(cursor_trail_scale, "cursor_trail_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(cursor_trail_smooth_div, "cursor_trail_smooth_div"sv, 4.0f, CLIENT | SKINS | SERVER,
       "divide the cursortrail.png image size by this much, for determining the distance to the next trail image"sv);
CONVAR(cursor_trail_smooth_force, "cursor_trail_smooth_force"sv, false, CLIENT | SKINS | SERVER);
CONVAR(cursor_trail_smooth_length, "cursor_trail_smooth_length"sv, 0.5f, CLIENT | SKINS | SERVER,
       "how long smooth cursortrails should be, in seconds"sv);
CONVAR(cursor_trail_spacing, "cursor_trail_spacing"sv, 15.f, CLIENT | SKINS | SERVER,
       "how big the gap between consecutive unsmooth cursortrail images should be, in milliseconds"sv);
CONVAR(disable_mousebuttons, "disable_mousebuttons"sv, true, CLIENT | SKINS | SERVER);
CONVAR(disable_mousewheel, "disable_mousewheel"sv, true, CLIENT | SKINS | SERVER);
CONVAR(drain_kill, "drain_kill"sv, true, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "whether to kill the player upon failing"sv);
CONVAR(drain_disabled, "drain_disabled"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "determines if HP drain should be disabled entirely"sv);
CONVAR(drain_kill_notification_duration, "drain_kill_notification_duration"sv, 1.0f, CLIENT | SKINS | SERVER,
       "how long to display the \"You have failed, but you can keep playing!\" notification (0 = disabled)"sv);
CONVAR(early_note_time, "early_note_time"sv, 1500.0f, CLIENT | SKINS | SERVER | GAMEPLAY,
       "Timeframe in ms at the beginning of a beatmap which triggers a starting delay for easier reading"sv);
CONVAR(end_delay_time, "end_delay_time"sv, 750.0f, CLIENT | SKINS | SERVER,
       "Duration in ms which is added at the end of a beatmap after the last hitobject is finished "
       "but before the ranking screen is automatically shown"sv);
CONVAR(end_skip, "end_skip"sv, true, CLIENT | SKINS | SERVER,
       "whether the beatmap jumps to the ranking screen as soon as the last hitobject plus lenience has passed"sv);
CONVAR(end_skip_time, "end_skip_time"sv, 400.0f, CLIENT | SKINS | SERVER,
       "Duration in ms which is added to the endTime of the last hitobject, after which pausing the "
       "game will immediately jump to the ranking screen"sv);
CONVAR(engine_throttle, "engine_throttle"sv, true, CLIENT | SKINS | SERVER,
       "limit some engine component updates to improve performance (non-gameplay-related, only turn this off if you "
       "like lower performance for no reason)"sv);
CONVAR(fail_time, "fail_time"sv, 2.25f, CLIENT | SKINS | SERVER,
       "Timeframe in s for the slowdown effect after failing, before the pause menu is shown"sv);
CONVAR(file_size_max, "file_size_max"sv, 1024, CLIENT | SKINS | SERVER,
       "maximum filesize sanity limit in MB, all files bigger than this are not allowed to load"sv);
CONVAR(flashlight_always_hard, "flashlight_always_hard"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "always use 200+ combo flashlight radius"sv);
CONVAR(flashlight_follow_delay, "flashlight_follow_delay"sv, 0.120f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(flashlight_radius, "flashlight_radius"sv, 100.f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(followpoints_anim, "followpoints_anim"sv, false, CLIENT | SKINS | SERVER,
       "scale + move animation while fading in followpoints (osu only does this when its "
       "internal default skin is being used)"sv);
CONVAR(followpoints_approachtime, "followpoints_approachtime"sv, 800.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(followpoints_clamp, "followpoints_clamp"sv, false, CLIENT | SERVER | GAMEPLAY,
       "clamp followpoint approach time to current circle approach time (instead of using the "
       "hardcoded default 800 ms raw)"sv);
CONVAR(followpoints_connect_combos, "followpoints_connect_combos"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "connect followpoints even if a new combo has started"sv);
CONVAR(followpoints_connect_spinners, "followpoints_connect_spinners"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "connect followpoints even through spinners"sv);
CONVAR(followpoints_prevfadetime, "followpoints_prevfadetime"sv, 400.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(followpoints_scale_multiplier, "followpoints_scale_multiplier"sv, 1.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(followpoints_separation_multiplier, "followpoints_separation_multiplier"sv, 1.0f, CLIENT | SERVER | GAMEPLAY);
CONVAR(force_oauth, "force_oauth"sv, false, CLIENT, "always display oauth login button instead of password field"sv);
CONVAR(force_legacy_slider_renderer, "force_legacy_slider_renderer"sv, false, CLIENT | SKINS | SERVER,
       "on some older machines, this may be faster than vertexbuffers"sv);
CONVAR(fposu_3d_skybox, "fposu_3d_skybox"sv, true, CLIENT | SKINS | SERVER);
CONVAR(fposu_3d_skybox_size, "fposu_3d_skybox_size"sv, 450.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_absolute_mode, "fposu_absolute_mode"sv, false, CLIENT | SKINS | SERVER);
CONVAR(fposu_cube, "fposu_cube"sv, true, CLIENT | SKINS | SERVER);
CONVAR(fposu_cube_size, "fposu_cube_size"sv, 500.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_cube_tint_b, "fposu_cube_tint_b"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(fposu_cube_tint_g, "fposu_cube_tint_g"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(fposu_cube_tint_r, "fposu_cube_tint_r"sv, 255, CLIENT | SKINS | SERVER, "from 0 to 255"sv);
CONVAR(fposu_curved, "fposu_curved"sv, true, CLIENT | SKINS | SERVER);
CONVAR(fposu_distance, "fposu_distance"sv, 0.5f, CLIENT | SKINS | SERVER);
CONVAR(fposu_draw_cursor_trail, "fposu_draw_cursor_trail"sv, true, CLIENT | SKINS | SERVER);
CONVAR(fposu_draw_scorebarbg_on_top, "fposu_draw_scorebarbg_on_top"sv, false, CLIENT | SKINS | SERVER);
CONVAR(fposu_fov, "fposu_fov"sv, 103.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_invert_horizontal, "fposu_invert_horizontal"sv, false, CLIENT | SKINS | SERVER);
CONVAR(fposu_invert_vertical, "fposu_invert_vertical"sv, false, CLIENT | SKINS | SERVER);
CONVAR(fposu_mod_strafing, "fposu_mod_strafing"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(fposu_mod_strafing_frequency_x, "fposu_mod_strafing_frequency_x"sv, 0.1f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mod_strafing_frequency_y, "fposu_mod_strafing_frequency_y"sv, 0.2f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mod_strafing_frequency_z, "fposu_mod_strafing_frequency_z"sv, 0.15f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mod_strafing_strength_x, "fposu_mod_strafing_strength_x"sv, 0.3f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mod_strafing_strength_y, "fposu_mod_strafing_strength_y"sv, 0.1f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mod_strafing_strength_z, "fposu_mod_strafing_strength_z"sv, 0.15f, CLIENT | SERVER | GAMEPLAY);
CONVAR(fposu_mouse_cm_360, "fposu_mouse_cm_360"sv, 30.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_mouse_dpi, "fposu_mouse_dpi"sv, 400, CLIENT | SKINS | SERVER);
CONVAR(fposu_noclip, "fposu_noclip"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(fposu_noclipaccelerate, "fposu_noclipaccelerate"sv, 20.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_noclipfriction, "fposu_noclipfriction"sv, 10.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_noclipspeed, "fposu_noclipspeed"sv, 2.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_position_x, "fposu_playfield_position_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_position_y, "fposu_playfield_position_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_position_z, "fposu_playfield_position_z"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_rotation_x, "fposu_playfield_rotation_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_rotation_y, "fposu_playfield_rotation_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_playfield_rotation_z, "fposu_playfield_rotation_z"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_skybox, "fposu_skybox"sv, true, CLIENT | SKINS | SERVER);
CONVAR(fposu_transparent_playfield, "fposu_transparent_playfield"sv, false, CLIENT | SKINS | SERVER,
       "only works if background dim is 100% and background brightness is 0%"sv);
CONVAR(fposu_vertical_fov, "fposu_vertical_fov"sv, false, CLIENT | SKINS | SERVER);
CONVAR(fposu_zoom_anim_duration, "fposu_zoom_anim_duration"sv, 0.065f, CLIENT | SKINS | SERVER,
       "time in seconds for the zoom/unzoom animation"sv);
CONVAR(fposu_zoom_fov, "fposu_zoom_fov"sv, 45.0f, CLIENT | SKINS | SERVER);
CONVAR(fposu_zoom_sensitivity_ratio, "fposu_zoom_sensitivity_ratio"sv, 1.0f, CLIENT | SKINS | SERVER,
       "replicates zoom_sensitivity_ratio behavior on css/csgo/tf2/etc."sv);
CONVAR(fposu_zoom_toggle, "fposu_zoom_toggle"sv, false, CLIENT | SKINS | SERVER,
       "whether the zoom key acts as a toggle"sv);
CONVAR(hiterrorbar_misaims, "hiterrorbar_misaims"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hiterrorbar_misses, "hiterrorbar_misses"sv, true, CLIENT | SKINS | SERVER);
CONVAR(hitobject_fade_in_time, "hitobject_fade_in_time"sv, 400, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "in milliseconds (!)"sv);
CONVAR(hitobject_fade_out_time, "hitobject_fade_out_time"sv, 0.293f, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "in seconds (!)"sv);
CONVAR(hitobject_fade_out_time_speed_multiplier_min, "hitobject_fade_out_time_speed_multiplier_min"sv, 0.5f,
       CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "The minimum multiplication factor allowed for the speed multiplier influencing the fadeout duration"sv);
CONVAR(hitobject_hittable_dim, "hitobject_hittable_dim"sv, true, CLIENT | SKINS | SERVER,
       "whether to dim objects not yet within the miss-range (when they can't even be missed yet)"sv);
CONVAR(hitobject_hittable_dim_duration, "hitobject_hittable_dim_duration"sv, 100, CLIENT | SKINS | SERVER,
       "in milliseconds (!)"sv);
CONVAR(hitobject_hittable_dim_start_percent, "hitobject_hittable_dim_start_percent"sv, 0.7647f, CLIENT | SKINS | SERVER,
       "dimmed objects start at this brightness value before becoming fullbright (only RGB, this does not affect "
       "alpha/transparency)"sv);
CONVAR(hitresult_animated, "hitresult_animated"sv, true, CLIENT | SKINS | SERVER,
       "whether to animate hitresult scales (depending on particle<SCORE>.png, either scale wobble or smooth scale)"sv);
CONVAR(hitresult_delta_colorize, "hitresult_delta_colorize"sv, false, CLIENT | SKINS | SERVER,
       "whether to colorize hitresults depending on how early/late the hit (delta) was"sv);
CONVAR(hitresult_delta_colorize_early_b, "hitresult_delta_colorize_early_b"sv, 0, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(hitresult_delta_colorize_early_g, "hitresult_delta_colorize_early_g"sv, 0, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(hitresult_delta_colorize_early_r, "hitresult_delta_colorize_early_r"sv, 255, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(hitresult_delta_colorize_interpolate, "hitresult_delta_colorize_interpolate"sv, true, CLIENT | SKINS | SERVER,
       "whether colorized hitresults should smoothly interpolate between "
       "early/late colors depending on the hit delta amount"sv);
CONVAR(hitresult_delta_colorize_late_b, "hitresult_delta_colorize_late_b"sv, 255, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(hitresult_delta_colorize_late_g, "hitresult_delta_colorize_late_g"sv, 0, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(hitresult_delta_colorize_late_r, "hitresult_delta_colorize_late_r"sv, 0, CLIENT | SKINS | SERVER,
       "from 0 to 255"sv);
CONVAR(
    hitresult_delta_colorize_multiplier, "hitresult_delta_colorize_multiplier"sv, 2.0f, CLIENT | SKINS | SERVER,
    "early/late colors are multiplied by this (assuming interpolation is enabled, increasing this will make early/late "
    "colors appear fully earlier)"sv);
CONVAR(hitresult_draw_300s, "hitresult_draw_300s"sv, false, CLIENT | SKINS | SERVER);
CONVAR(hitresult_duration, "hitresult_duration"sv, 1.100f, CLIENT | SKINS | SERVER,
       "max duration of the entire hitresult in seconds (this limits all other values, except for animated skins!)"sv);
CONVAR(hitresult_duration_max, "hitresult_duration_max"sv, 5.0f, CLIENT | SKINS | SERVER,
       "absolute hard limit in seconds, even for animated skins"sv);
CONVAR(hitresult_fadein_duration, "hitresult_fadein_duration"sv, 0.120f, CLIENT | SKINS | SERVER);
CONVAR(hitresult_fadeout_duration, "hitresult_fadeout_duration"sv, 0.600f, CLIENT | SKINS | SERVER);
CONVAR(hitresult_fadeout_start_time, "hitresult_fadeout_start_time"sv, 0.500f, CLIENT | SKINS | SERVER);
CONVAR(hitresult_miss_fadein_scale, "hitresult_miss_fadein_scale"sv, 2.0f, CLIENT | SKINS | SERVER);
CONVAR(hitresult_scale, "hitresult_scale"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(ignore_beatmap_combo_colors, "ignore_beatmap_combo_colors"sv, true, CLIENT | SKINS | SERVER);
CONVAR(ignore_beatmap_combo_numbers, "ignore_beatmap_combo_numbers"sv, false, CLIENT | SKINS | SERVER,
       "may be used in conjunction with number_max"sv);
CONVAR(ignore_beatmap_sample_volume, "ignore_beatmap_sample_volume"sv, false, CLIENT | SKINS | SERVER);
CONVAR(instafade, "instafade"sv, false, CLIENT | SKINS | SERVER, "don't draw hitcircle fadeout animations"sv);
CONVAR(instafade_sliders, "instafade_sliders"sv, false, CLIENT | SKINS | SERVER,
       "don't draw slider fadeout animations"sv);
CONVAR(instant_replay_duration, "instant_replay_duration"sv, 15.f, CLIENT | SKINS | SERVER,
       "instant replay (F2) duration, in seconds"sv);
CONVAR(interpolate_music_pos, "interpolate_music_pos"sv, true, CLIENT | SKINS | SERVER,
       "interpolate song position with engine time"sv);
CONVAR(letterboxing, "letterboxing"sv, true, CLIENT | SKINS | SERVER);
CONVAR(letterboxing_offset_x, "letterboxing_offset_x"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(letterboxing_offset_y, "letterboxing_offset_y"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(load_beatmap_background_images, "load_beatmap_background_images"sv, true, CLIENT | SKINS | SERVER);
CONVAR(minimize_on_focus_lost_if_borderless_windowed_fullscreen,
       "minimize_on_focus_lost_if_borderless_windowed_fullscreen"sv, false, CLIENT | SKINS | SERVER);
CONVAR(minimize_on_focus_lost_if_fullscreen, "minimize_on_focus_lost_if_fullscreen"sv, true, CLIENT | SKINS | SERVER);
CONVAR(mouse_raw_input, "mouse_raw_input"sv, false, CLIENT | SKINS | SERVER);
CONVAR(mouse_sensitivity, "mouse_sensitivity"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(pen_input, "pen_input"sv, true, CLIENT | SKINS | SERVER,
       "support OTD Artist Mode and native tablet drivers' pen events"sv);
CONVAR(nightcore_enjoyer, "nightcore_enjoyer"sv, false, CLIENT | SKINS | SERVER);
CONVAR(normalize_loudness, "normalize_loudness"sv, true, CLIENT | SKINS | SERVER, "normalize loudness across songs"sv);
CONVAR(notelock_stable_tolerance2b, "notelock_stable_tolerance2b"sv, 3, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "time tolerance in milliseconds to allow hitting simultaneous objects close "
       "together (e.g. circle at end of slider)"sv);
CONVAR(notelock_type, "notelock_type"sv, 2, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "which notelock algorithm to use (0 = None, 1 = neosu, 2 = osu!stable, 3 = osu!lazer 2020)"sv);
CONVAR(notification_duration, "notification_duration"sv, 1.25f, CLIENT | SKINS | SERVER);
CONVAR(notify_friend_status_change, "notify_friend_status_change"sv, true, CLIENT,
       "notify when friends change status"sv);
CONVAR(number_max, "number_max"sv, 0, CLIENT | SKINS | SERVER,
       "0 = disabled, 1/2/3/4/etc. limits visual circle numbers to this number"sv);
CONVAR(number_scale_multiplier, "number_scale_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(od_override, "od_override"sv, -1.0f, CLIENT | SERVER | PROTECTED | GAMEPLAY);
CONVAR(od_override_lock, "od_override_lock"sv, false, CLIENT | SERVER | PROTECTED | GAMEPLAY,
       "always force constant 300 hit window even through speed changes"sv);
CONVAR(old_beatmap_offset, "old_beatmap_offset"sv, 24.0f, CLIENT | SKINS | SERVER,
       "offset in ms which is added to beatmap versions < 5 (default value is hardcoded 24 ms in stable)"sv);
CONVAR(options_high_quality_sliders, "options_high_quality_sliders"sv, false, CLIENT | SKINS | SERVER);
CONVAR(options_save_on_back, "options_save_on_back"sv, true, CLIENT | SKINS | SERVER);
CONVAR(options_slider_preview_use_legacy_renderer, "options_slider_preview_use_legacy_renderer"sv, false, CLIENT,
       "apparently newer AMD drivers with old gpus are crashing here with the legacy renderer? was just me being lazy "
       "anyway, so now there is a vao render path as it should be"sv);
CONVAR(options_slider_quality, "options_slider_quality"sv, 0.0f, CLIENT | SKINS | SERVER);
CONVAR(pause_anim_duration, "pause_anim_duration"sv, 0.15f, CLIENT | SKINS | SERVER);
CONVAR(pause_dim_alpha, "pause_dim_alpha"sv, 0.58f, CLIENT | SKINS | SERVER);
CONVAR(pause_dim_background, "pause_dim_background"sv, true, CLIENT | SKINS | SERVER);
CONVAR(pause_on_focus_loss, "pause_on_focus_loss"sv, true, CLIENT | SKINS | SERVER);
CONVAR(pvs, "pvs"sv, true, CLIENT | SKINS | SERVER,
       "optimizes all loops over all hitobjects by clamping the range to the Potentially Visible Set"sv);
CONVAR(quick_retry_delay, "quick_retry_delay"sv, 0.27f, CLIENT | SKINS | SERVER);
CONVAR(quick_retry_time, "quick_retry_time"sv, 2000.0f, CLIENT | SKINS | SERVER,
       "Timeframe in ms subtracted from the first hitobject when quick retrying (not regular retry)"sv);
CONVAR(rankingscreen_pp, "rankingscreen_pp"sv, true, CLIENT | SKINS | SERVER);
CONVAR(rankingscreen_topbar_height_percent, "rankingscreen_topbar_height_percent"sv, 0.785f, CLIENT | SKINS | SERVER);
CONVAR(
    relax_offset, "relax_offset"sv, -12, CLIENT | SERVER | PROTECTED | GAMEPLAY,
    "osu!relax always hits -12 ms too early, so set this to -12 (note the negative) if you want it to be the same"sv);
CONVAR(resolution, "resolution"sv, "1280x720"sv, CLIENT | SKINS | SERVER);
CONVAR(windowed_resolution, "windowed_resolution"sv, "1280x720"sv, CLIENT | SKINS | SERVER);
CONVAR(resolution_keep_aspect_ratio, "resolution_keep_aspect_ratio"sv, false, CLIENT | SKINS | SERVER);
CONVAR(restart_sound_engine_before_playing, "restart_sound_engine_before_playing"sv, false, CLIENT | SKINS | SERVER,
       "jank fix for users who experience sound issues after playing for a while"sv);
CONVAR(rich_presence, "rich_presence"sv, true, CLIENT | SKINS | SERVER, CFUNC(RichPresence::onRichPresenceChange));
CONVAR(scoreboard_animations, "scoreboard_animations"sv, true, CLIENT | SKINS | SERVER, "animate in-game scoreboard"sv);
CONVAR(scores_bonus_pp, "scores_bonus_pp"sv, true, CLIENT | SKINS | SERVER,
       "whether to add bonus pp to total (real) pp or not"sv);
CONVAR(scores_enabled, "scores_enabled"sv, true, CLIENT | SKINS | SERVER);
CONVAR(scores_save_immediately, "scores_save_immediately"sv, true, CLIENT | SKINS | SERVER,
       "write scores.db as soon as a new score is added"sv);
CONVAR(scores_sort_by_pp, "scores_sort_by_pp"sv, true, CLIENT | SKINS | SERVER,
       "display pp in score browser instead of score"sv);
CONVAR(scrubbing_smooth, "scrubbing_smooth"sv, true, CLIENT | SKINS | SERVER);
CONVAR(seek_delta, "seek_delta"sv, 5, CLIENT | SKINS | SERVER,
       "how many seconds to skip backward/forward when quick seeking"sv);
CONVAR(show_approach_circle_on_first_hidden_object, "show_approach_circle_on_first_hidden_object"sv, true,
       CLIENT | SKINS | SERVER);
CONVAR(simulate_replays, "simulate_replays"sv, false, CLIENT | SKINS | SERVER,
       "experimental \"improved\" replay playback"sv);
CONVAR(skin, "skin"sv, "default"sv, CLIENT | SKINS | SERVER);
CONVAR(skin_animation_force, "skin_animation_force"sv, false, CLIENT | SKINS | SERVER);
CONVAR(skin_animation_fps_override, "skin_animation_fps_override"sv, -1.0f, CLIENT | SKINS | SERVER);
CONVAR(skin_async, "skin_async"sv, true, CLIENT | SKINS | SERVER, "load in background without blocking"sv);
CONVAR(skin_color_index_add, "skin_color_index_add"sv, 0, CLIENT | SKINS | SERVER);
CONVAR(skin_force_hitsound_sample_set, "skin_force_hitsound_sample_set"sv, 0, CLIENT | SKINS | SERVER,
       "force a specific hitsound sample set to always be used regardless of what "
       "the beatmap says. 0 = disabled, 1 = normal, 2 = soft, 3 = drum."sv);
CONVAR(skin_hd, "skin_hd"sv, true, CLIENT | SKINS | SERVER, "load and use @2x versions of skin images, if available"sv);
CONVAR(skin_mipmaps, "skin_mipmaps"sv, false, CLIENT | SKINS | SERVER,
       "generate mipmaps for every skin image (only useful on lower game resolutions, requires more vram)"sv);
CONVAR(skin_random, "skin_random"sv, false, CLIENT | SKINS | SERVER,
       "select random skin from list on every skin load/reload"sv);
CONVAR(skin_random_elements, "skin_random_elements"sv, false, CLIENT | SKINS | SERVER,
       "sElECt RanDOM sKIn eLemENTs FRoM ranDom SkINs"sv);
CONVAR(skin_reload, "skin_reload"sv);
CONVAR(skin_use_skin_hitsounds, "skin_use_skin_hitsounds"sv, true, CLIENT | SKINS | SERVER,
       "If enabled: Use skin's sound samples. If disabled: Use default skin's sound samples. For hitsounds only."sv);
CONVAR(skip_breaks_enabled, "skip_breaks_enabled"sv, true, CLIENT | SKINS | SERVER,
       "enables/disables skip button for breaks in the middle of beatmaps"sv);
CONVAR(skip_intro_enabled, "skip_intro_enabled"sv, true, CLIENT | SKINS | SERVER,
       "enables/disables skip button for intro until first hitobject"sv);
CONVAR(slider_alpha_multiplier, "slider_alpha_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(slider_ball_tint_combo_color, "slider_ball_tint_combo_color"sv, true, CLIENT | SKINS | SERVER);
CONVAR(slider_body_alpha_multiplier, "slider_body_alpha_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER,
       CFUNC(SliderRenderer::onUniformConfigChanged));
CONVAR(slider_body_color_saturation, "slider_body_color_saturation"sv, 1.0f, CLIENT | SKINS | SERVER,
       CFUNC(SliderRenderer::onUniformConfigChanged));
CONVAR(slider_body_fade_out_time_multiplier, "slider_body_fade_out_time_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER,
       "multiplies hitobject_fade_out_time"sv);
CONVAR(slider_body_lazer_fadeout_style, "slider_body_lazer_fadeout_style"sv, true, CLIENT | SKINS | SERVER,
       "if snaking out sliders are enabled (aka shrinking sliders), smoothly fade "
       "out the last remaining part of the body (instead of vanishing instantly)"sv);
CONVAR(slider_body_smoothsnake, "slider_body_smoothsnake"sv, true, CLIENT | SKINS | SERVER,
       "draw 1 extra interpolated circle mesh at the start & end of every slider for extra smooth snaking/shrinking"sv);
CONVAR(slider_body_unit_circle_subdivisions, "slider_body_unit_circle_subdivisions"sv, 42, CLIENT | SKINS | SERVER);
CONVAR(slider_border_feather, "slider_border_feather"sv, 0.0f, CLIENT | SKINS | SERVER,
       CFUNC(SliderRenderer::onUniformConfigChanged));
CONVAR(slider_border_size_multiplier, "slider_border_size_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER,
       CFUNC(SliderRenderer::onUniformConfigChanged));
CONVAR(slider_border_tint_combo_color, "slider_border_tint_combo_color"sv, false, CLIENT | SKINS | SERVER);
CONVAR(slider_draw_body, "slider_draw_body"sv, true, CLIENT | SKINS | SERVER);
CONVAR(slider_draw_endcircle, "slider_draw_endcircle"sv, true, CLIENT | SKINS | SERVER);
CONVAR(slider_end_miss_breaks_combo, "slider_end_miss_breaks_combo"sv, false, CLIENT | SKINS | SERVER,
       "should a missed sliderend break combo (aka cause a regular sliderbreak)"sv);
CONVAR(slider_followcircle_fadein_fade_time, "slider_followcircle_fadein_fade_time"sv, 0.06f, CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_fadein_scale, "slider_followcircle_fadein_scale"sv, 0.5f, CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_fadein_scale_time, "slider_followcircle_fadein_scale_time"sv, 0.18f,
       CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_fadeout_fade_time, "slider_followcircle_fadeout_fade_time"sv, 0.25f,
       CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_fadeout_scale, "slider_followcircle_fadeout_scale"sv, 0.8f, CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_fadeout_scale_time, "slider_followcircle_fadeout_scale_time"sv, 0.25f,
       CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_tick_pulse_scale, "slider_followcircle_tick_pulse_scale"sv, 0.1f, CLIENT | SKINS | SERVER);
CONVAR(slider_followcircle_tick_pulse_time, "slider_followcircle_tick_pulse_time"sv, 0.2f, CLIENT | SKINS | SERVER);
CONVAR(
    slider_legacy_use_baked_vao, "slider_legacy_use_baked_vao"sv, false, CLIENT | SKINS | SERVER,
    "use baked cone mesh instead of raw mesh for legacy slider renderer (disabled by default because usually slower on "
    "very old gpus even though it should not be)"sv);
CONVAR(slider_osu_next_style, "slider_osu_next_style"sv, false, CLIENT | SKINS | SERVER,
       CFUNC(SliderRenderer::onUniformConfigChanged));
CONVAR(slider_rainbow, "slider_rainbow"sv, false, CLIENT | SKINS | SERVER);
CONVAR(slider_reverse_arrow_alpha_multiplier, "slider_reverse_arrow_alpha_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER);
CONVAR(slider_reverse_arrow_animated, "slider_reverse_arrow_animated"sv, true, CLIENT | SKINS | SERVER,
       "pulse animation on reverse arrows"sv);
CONVAR(slider_reverse_arrow_black_threshold, "slider_reverse_arrow_black_threshold"sv, 1.0f, CLIENT | SKINS | SERVER,
       "Blacken reverse arrows if the average color brightness percentage is above this value"sv);
CONVAR(slider_reverse_arrow_fadein_duration, "slider_reverse_arrow_fadein_duration"sv, 150, CLIENT | SKINS | SERVER,
       "duration in ms of the reverse arrow fadein animation after it starts"sv);
CONVAR(slider_shrink, "slider_shrink"sv, false, CLIENT | SKINS | SERVER);
CONVAR(slider_sliderhead_fadeout, "slider_sliderhead_fadeout"sv, true, CLIENT | SKINS | SERVER);
CONVAR(slider_snake_duration_multiplier, "slider_snake_duration_multiplier"sv, 1.0f, CLIENT | SKINS | SERVER,
       "the default snaking duration is multiplied with this (max sensible value "
       "is 3, anything above that will take longer than the approachtime)"sv);
CONVAR(slider_use_gradient_image, "slider_use_gradient_image"sv, false, CLIENT | SKINS | SERVER);
CONVAR(snaking_sliders, "snaking_sliders"sv, true, CLIENT | SKINS | SERVER);
CONVAR(sort_skins_cleaned, "sort_skins_cleaned"sv, false, CLIENT | SKINS | SERVER,
       "set to true to sort skins alphabetically, ignoring special characters at the start (not like stable)"sv);

CONVAR(spec_buffer, "spec_buffer"sv, 2500, CLIENT, "size of spectator buffer in milliseconds"sv);
CONVAR(spec_share_map, "spec_share_map"sv, true, CLIENT | SKINS | SERVER,
       "automatically send currently-playing beatmap to #spectator"sv);

CONVAR(spinner_fade_out_time_multiplier, "spinner_fade_out_time_multiplier"sv, 0.7f, CLIENT | SKINS | SERVER);
CONVAR(
    spinner_use_ar_fadein, "spinner_use_ar_fadein"sv, false, CLIENT | SKINS | SERVER,
    "whether spinners should fade in with AR (same as circles), or with hardcoded 400 ms fadein time (osu!default)"sv);
CONVAR(ssl_verify, "ssl_verify"sv, true, CLIENT);
CONVAR(stars_ignore_clamped_sliders, "stars_ignore_clamped_sliders"sv, true, CLIENT | SKINS | SERVER,
       "skips processing sliders limited by slider_curve_max_length"sv);
CONVAR(stars_slider_curve_points_separation, "stars_slider_curve_points_separation"sv, 20.0f, CLIENT | SKINS | SERVER,
       "massively reduce curve accuracy for star calculations to save memory/performance"sv);
CONVAR(stars_stacking, "stars_stacking"sv, true, CLIENT | SKINS | SERVER,
       "respect hitobject stacking before calculating stars/pp"sv);
CONVAR(start_first_main_menu_song_at_preview_point, "start_first_main_menu_song_at_preview_point"sv, false, CLIENT);
CONVAR(submit_after_pause, "submit_after_pause"sv, true, CLIENT | SERVER);
CONVAR(submit_scores, "submit_scores"sv, false, CLIENT | SERVER);
CONVAR(tooltip_anim_duration, "tooltip_anim_duration"sv, 0.4f, CLIENT | SKINS | SERVER);
CONVAR(ui_scale, "ui_scale"sv, 1.0f, CLIENT | SKINS | SERVER, "multiplier"sv);
CONVAR(ui_scale_to_dpi, "ui_scale_to_dpi"sv, true, CLIENT | SKINS | SERVER,
       "whether the game should scale its UI based on the DPI reported by your operating system"sv);
CONVAR(ui_scale_to_dpi_minimum_height, "ui_scale_to_dpi_minimum_height"sv, 1300, CLIENT | SKINS | SERVER,
       "any in-game resolutions below this will have ui_scale_to_dpi force disabled"sv);
CONVAR(ui_scale_to_dpi_minimum_width, "ui_scale_to_dpi_minimum_width"sv, 2200, CLIENT | SKINS | SERVER,
       "any in-game resolutions below this will have ui_scale_to_dpi force disabled"sv);
CONVAR(ui_scrollview_kinetic_approach_time, "ui_scrollview_kinetic_approach_time"sv, 0.075f, CLIENT | SKINS | SERVER,
       "approach target afterscroll delta over this duration"sv);
CONVAR(ui_scrollview_kinetic_energy_multiplier, "ui_scrollview_kinetic_energy_multiplier"sv, 24.0f, CLIENT,
       "afterscroll delta multiplier"sv);
CONVAR(ui_scrollview_mousewheel_multiplier, "ui_scrollview_mousewheel_multiplier"sv, 3.5f, CLIENT | SKINS | SERVER);
CONVAR(ui_scrollview_mousewheel_overscrollbounce, "ui_scrollview_mousewheel_overscrollbounce"sv, true, CLIENT);
CONVAR(ui_scrollview_resistance, "ui_scrollview_resistance"sv, 5.0f, CLIENT | SKINS | SERVER,
       "how many pixels you have to pull before you start scrolling"sv);
CONVAR(ui_scrollview_scrollbarwidth, "ui_scrollview_scrollbarwidth"sv, 15.0f, CLIENT | SKINS | SERVER);
CONVAR(ui_textbox_caret_blink_time, "ui_textbox_caret_blink_time"sv, 0.5f, CLIENT | SKINS | SERVER);
CONVAR(ui_textbox_text_offset_x, "ui_textbox_text_offset_x"sv, 3, CLIENT | SKINS | SERVER);
CONVAR(ui_top_ranks_max, "ui_top_ranks_max"sv, 200, CLIENT | SKINS | SERVER,
       "maximum number of displayed scores, to keep the ui/scrollbar manageable"sv);
CONVAR(ui_window_animspeed, "ui_window_animspeed"sv, 0.29f, CLIENT | SKINS | SERVER);
CONVAR(ui_window_shadow_radius, "ui_window_shadow_radius"sv, 13.0f, CLIENT | SKINS | SERVER);
CONVAR(universal_offset, "universal_offset"sv, 0.0f, CLIENT);
CONVAR(use_https, "use_https"sv, true, CLIENT);
CONVAR(use_ppv3, "use_ppv3"sv, false, CLIENT | SKINS | SERVER, "use ppv3 instead of ppv2 (experimental)"sv);
CONVAR(user_draw_accuracy, "user_draw_accuracy"sv, true, CLIENT | SKINS | SERVER);
CONVAR(user_draw_level, "user_draw_level"sv, true, CLIENT | SKINS | SERVER);
CONVAR(user_draw_level_bar, "user_draw_level_bar"sv, true, CLIENT | SKINS | SERVER);
CONVAR(user_draw_pp, "user_draw_pp"sv, true, CLIENT | SKINS | SERVER);
CONVAR(user_include_relax_and_autopilot_for_stats, "user_include_relax_and_autopilot_for_stats"sv, false,
       CLIENT | SKINS | SERVER);
CONVAR(vsync, "vsync"sv, false, CLIENT, [](float on) -> void { g ? g->setVSync(!!static_cast<int>(on)) : (void)0; });
// this is not windows-only anymore, just keeping it with the "win_" prefix to not break old configs
CONVAR(win_processpriority, "win_processpriority"sv, 1, CLIENT,
       "sets the main process priority (0 = normal, 1 = high)"sv, CFUNC(Environment::setThreadPriority));

// Unfinished features
CONVAR(adblock, "adblock"sv, true, CLIENT | SKINS | SERVER);
CONVAR(prefer_websockets, "prefer_websockets"sv, false, CLIENT, "prefer websocket connections over http polling");
CONVAR(load_db_immediately, "load_db_immediately"sv, false, CLIENT);
CONVAR(cbf, "cbf"sv, false, CLIENT, "click between frames"sv);
CONVAR(enable_spectating, "enable_spectating"sv, false, CLIENT);
CONVAR(allow_mp_invites, "allow_mp_invites"sv, true, CLIENT, "allow multiplayer game invites from all users"sv);
CONVAR(allow_stranger_dms, "allow_stranger_dms"sv, true, CLIENT, "allow private messages from non-friends"sv);
CONVAR(ignore_beatmap_samples, "ignore_beatmap_samples"sv, false, CLIENT | SERVER, "ignore beatmap hitsounds"sv);
CONVAR(ignore_beatmap_skins, "ignore_beatmap_skins"sv, false, CLIENT | SERVER, "ignore beatmap skins"sv);
CONVAR(language, "language"sv, "en"sv, CLIENT | SERVER);
CONVAR(draw_storyboard, "draw_storyboard"sv, true, CLIENT | SERVER);
CONVAR(draw_video, "draw_video"sv, true, CLIENT | SERVER);
CONVAR(save_failed_scores, "save_failed_scores"sv, false, CLIENT | HIDDEN,
       "save scores locally, even if there was a fail"sv);

// NOLINTEND(misc-definitions-in-headers)

}  // namespace cv

#endif
