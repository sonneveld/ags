
#include "ac/asset_helper.h"

extern void audio_core_init();
extern void audio_core_shutdown();


extern void audio_core_set_master_volume(float newvol);

extern int audio_core_sample_load(const AssetPath asset_name, const char *extension);
extern int audio_core_sample_get_length_ms(int core_sample);
extern int audio_core_sample_get_sound_type(int core_sample);


extern int audio_core_channel_allocate();
extern void audio_core_channel_delete(int core_channel);

extern int audio_core_channel_play_from(int core_channel, int core_sample, int position, bool repeat, float volume);
extern void audio_core_channel_seek(int core_channel, int pos);
extern void audio_core_channel_pause(int core_channel);
extern void audio_core_channel_resume(int core_channel);
extern void audio_core_channel_stop(int core_channel);

extern int audio_core_channel_get_pos(int core_channel);
extern int audio_core_channel_get_pos_ms(int core_channel);
extern bool audio_core_is_channel_active(int core_channel);

extern void audio_core_channel_set_volume(int core_channel, float volume);
extern void audio_core_channel_set_speed(int core_channel, float speed);
extern void audio_core_channel_set_panning(int core_channel, float panning);



extern void audio_core_set_midi_pos(int position);
extern int audio_core_get_midi_pos();