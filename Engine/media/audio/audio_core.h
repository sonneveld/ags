
#include "ac/asset_helper.h"
#include "util/string.h"
#include "core/assetmanager.h"
#include "util/stream.h"

float audio_core_asset_length_ms(const AssetPath &assetPath, const AGS::Common::String &extension);
int audio_core_asset_sound_type(const AssetPath &assetPath, const AGS::Common::String &extension);
// (ms, pattern, beat)
//int position_ms_to_posititon();
//int position_to_position_ms();

// will start in background thread.
void audio_core_init(/*config, soundlib*/);
void audio_core_shutdown();

// controls
// if none free, add a new one.
int audio_core_slot_initialise(const AssetPath &assetPath, const AGS::Common::String &extension, AGS::Common::Stream *hackStream, size_t hacksize);
void audio_core_slot_play(int slot_handle);
void audio_core_slot_pause(int slot_handle);
void audio_core_slot_stop(int slot_handle);
void audio_core_slot_seek_ms(int slot_handle, float pos_ms);

// config
void audio_core_set_master_volume(float newvol);
void audio_core_slot_configure(int slot_handle, bool repeat, float volume, float speed, float panning);

// status
#define MUS_MIDI 1
#define MUS_MP3  2
#define MUS_WAVE 3
#define MUS_MOD  4
#define MUS_OGG  5
enum AudioCorePlayState { Initial, Playing, Paused, Stopped };

AudioCorePlayState audio_core_slot_get_play_state(int slot_handle);
float audio_core_slot_get_pos_ms(int slot_handle);  // ms
