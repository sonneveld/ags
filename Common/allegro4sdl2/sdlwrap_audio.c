//
//  sdlwrap.c
//  AGSKit
//
//  Created by Nick Sonneveld on 9/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include "sdlwrap.h"
#include <SDL2/SDL.h>
#include "allegro.h"
#include "allegro/internal/aintern.h"
#include <strings.h>
#include <ctype.h>





// --------------------------------------------------------------------------
// Audio
// --------------------------------------------------------------------------


/* notes
 
 
 
 
 good summary
 http://stackoverflow.com/a/90592/84262
 use fmod or openal
 
 
 sdl_mixer  https://www.libsdl.org/projects/SDL_mixer/ (limited)
 almixer http://playcontrol.net/opensource/ALmixer/ https://bitbucket.org/ewing/almixer/src
 openal http://kcat.strangesoft.net/openal.html
 sdl_sound : out of date?
 
 tracker http://mikmod.sourceforge.net/
 http://modplug-xmms.sourceforge.net/
 http://dumb.sourceforge.net/
 
 
 commercial
 http://www.fmod.org/
 bass http://www.un4seen.com/
 http://www.ambiera.com/irrklang/irrklang_pro.html
 
 
 fez and audio
 https://plus.google.com/+flibitijibibo/posts/UQ7pANF4ZsW
 http://theinstructionlimit.com/ogg-streaming-using-opentk-and-nvorbis
 
 
 
 
 // */

int digi_card = DIGI_AUTODETECT;

void reserve_voices(int digi_voices, int midi_voices) { printf("STUB: reserve_voices\n");       }
int install_sound() { printf("STUB: install_sound\n");      return -1; }
void remove_sound(){ printf("STUB: remove_sound\n");       }
void set_volume(int digi_volume, int midi_volume) { printf("STUB: set_volume\n");       }
void set_volume_per_voice(int scale) { printf("STUB: set_volume_per_voice\n");        }

//  allegro supports 256 virtual voices.
void voice_start(int voice) { printf("STUB: voice_start\n");      }
void voice_stop(int voice) { printf("STUB: voice_stop\n");      }

void voice_set_volume(int voice, int volume) { printf("STUB: voice_set_volume\n");       }
int voice_get_frequency(int voice) { printf("STUB: voice_get_frequency\n");      return 0; }
int voice_get_position(int voice) { printf("STUB: voice_get_position\n");      return 0; }
void voice_set_position(int voice, int position) { printf("STUB: voice_set_position\n");    }
void voice_set_pan(int voice, int pan) { printf("STUB: voice_set_pan\n");      }
void voice_set_priority(int voice, int priority) { printf("STUB: voice_set_priority\n");      }
void voice_set_frequency(int voice, int frequency) { printf("STUB: voice_set_frequency\n");        }

SAMPLE *create_sample(int bits, int stereo, int freq, int len) { printf("STUB: create_sample\n");      return 0; }
void destroy_sample(SAMPLE *spl) { printf("STUB: destroy_sample\n");      }
SAMPLE *load_wav_pf(PACKFILE *f) { printf("STUB: load_wav_pf\n");      return 0; }
int play_sample() { printf("STUB: play_sample\n");      return -1; }  //  returns VOICE
void stop_sample(const SAMPLE *spl) { printf("STUB: stop_sample\n");     }
void adjust_sample(const SAMPLE *spl, int vol, int pan, int freq, int loop) { printf("STUB: adjust_sample\n");       }

//// internally it uses a giant sample as buffer.
AUDIOSTREAM *play_audio_stream(int len, int bits, int stereo, int freq, int vol, int pan) { printf("STUB: play_audio_stream\n");      return 0; }
void free_audio_stream_buffer(AUDIOSTREAM *stream) { printf("STUB: free_audio_stream_buffer\n");       }
void *get_audio_stream_buffer(AUDIOSTREAM *stream) { printf("STUB: get_audio_stream_buffer\n");      return 0; }
void stop_audio_stream(AUDIOSTREAM *stream) { printf("STUB: stop_audio_stream\n");     }


//  midi
int midi_card;
volatile long midi_pos;

MIDI * load_midi (AL_CONST char *filename) { printf("STUB: load_midi\n");      return 0; }
void destroy_midi (MIDI *midi) { printf("STUB: destroy_midi\n");      }
int load_midi_patches (void) { printf("STUB: load_midi_patches\n");      return 0; }

int play_midi(MIDI *midi, int loop) { printf("STUB: play_midi\n");      return 0; }
void midi_pause(void) { printf("STUB: midi_pause\n");       }
void midi_resume(void) { printf("STUB: midi_resume\n");       }
void stop_midi(void) { printf("STUB: stop_midi\n");        }
int midi_seek(int target) { printf("STUB: midi_seek\n");      return 0; }

int get_midi_length (MIDI *midi) { printf("STUB: get_midi_length\n");      return 0; }

