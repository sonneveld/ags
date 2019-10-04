/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      MacOS X Core Audio digital sound driver.
 *
 *      By Angelo Mottola.
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintosx.h"

#ifndef ALLEGRO_MACOSX
#error something is wrong with the makefile
#endif


static int ca_detect(int);
static int ca_init(int, int);
static void ca_exit(int);
static int ca_buffer_size();
static int ca_set_mixer_volume(int);


static AUGraph graph;
   
static AudioUnit converter_unit;
static AudioUnit mixer_unit;
static AudioUnit output_unit;

static int audio_buffer_size = 0;
static unsigned char *audio_buffer = NULL;
static int audio_buffer_offset = 0;

static char ca_desc[512] = EMPTY_STRING;


DIGI_DRIVER digi_core_audio =
{
   DIGI_CORE_AUDIO,
   empty_string,
   empty_string,
   "CoreAudio",
   0,
   0,
   MIXER_MAX_SFX,
   MIXER_DEF_SFX,

   ca_detect,
   ca_init,
   ca_exit,
   ca_set_mixer_volume,
   NULL,

   NULL,
   NULL,
   ca_buffer_size,
   _mixer_init_voice,
   _mixer_release_voice,
   _mixer_start_voice,
   _mixer_stop_voice,
   _mixer_loop_voice,

   _mixer_get_position,
   _mixer_set_position,

   _mixer_get_volume,
   _mixer_set_volume,
   _mixer_ramp_volume,
   _mixer_stop_volume_ramp,

   _mixer_get_frequency,
   _mixer_set_frequency,
   _mixer_sweep_frequency,
   _mixer_stop_frequency_sweep,

   _mixer_get_pan,
   _mixer_set_pan,
   _mixer_sweep_pan,
   _mixer_stop_pan_sweep,

   _mixer_set_echo,
   _mixer_set_tremolo,
   _mixer_set_vibrato,
   0, 0,
   0,
   0,
   0,
   0,
   0,
   0
};


/*
A frame is a set of samples for each channel at one point of time
A packet is a set of frames. PCM just has one frame per packet, but other formats may differ
*/

// https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/AudioUnitHostingGuide_iOS/AudioUnitHostingFundamentals/AudioUnitHostingFundamentals.html#//apple_ref/doc/uid/TP40009492-CH3-SW27
static OSStatus render_callback(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags,
   const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames, AudioBufferList *ioData)
{
   // printf("render numbuf=%d numframes=%d bufferSize=%d\n", ioData->mNumberBuffers, inNumFrames, ioData->mBuffers[0].mDataByteSize);
    
   unsigned char *mData = ioData->mBuffers[0].mData;
   UInt32 mDataByteRemaining = ioData->mBuffers[0].mDataByteSize;
   // UInt32 mDataByteRemaining = inNumFrames*2*sizeof(short);

   while (mDataByteRemaining > 0) {
      if (audio_buffer_offset >= audio_buffer_size) {
         _mix_some_samples(audio_buffer, 0, TRUE);
         audio_buffer_offset = 0;
      }

      UInt32 byteCopyCount = audio_buffer_size - audio_buffer_offset;
      if (byteCopyCount > mDataByteRemaining) {
         byteCopyCount = mDataByteRemaining;
      }

      memcpy(mData, audio_buffer+audio_buffer_offset, byteCopyCount);

      mData += byteCopyCount;
      mDataByteRemaining -= byteCopyCount;
      audio_buffer_offset += byteCopyCount;

      // printf("  - transferred %d bytes\n", byteCopyCount);
   }
   return 0;
}



static int ca_detect(int input)
{
   if (input) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
      return FALSE;
   }
   if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_1)
      return FALSE;
   return TRUE;
}



static int ca_init(int input, int voices)
{
   UInt32 property_size;

   if (input) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
      return -1;
   }
   if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_1) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("MacOS X.2 or newer required by this driver"));
      return -1;
   }
   
   NewAUGraph(&graph);

   AUNode converter_node;
   AudioComponentDescription converter_desc = {0};
   converter_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
   converter_desc.componentSubType = kAudioUnitSubType_AUConverter;
   converter_desc.componentType = kAudioUnitType_FormatConverter;
   AUGraphAddNode(graph, &converter_desc, &converter_node);

   AUNode mixer_node;
   AudioComponentDescription mixer_desc = {0};
   mixer_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
   mixer_desc.componentSubType = kAudioUnitSubType_StereoMixer;
   mixer_desc.componentType = kAudioUnitType_Mixer;
   AUGraphAddNode(graph, &mixer_desc, &mixer_node);

   AUNode output_node;
   AudioComponentDescription output_desc = {0};
   output_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
   output_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
   output_desc.componentType = kAudioUnitType_Output;
   AUGraphAddNode(graph, &output_desc, &output_node);

   AUGraphConnectNodeInput(graph, converter_node, 0, mixer_node, 0);
   AUGraphConnectNodeInput(graph, mixer_node, 0, output_node, 0);

   if (AUGraphOpen(graph)) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Error opening graph"));
      ca_exit(FALSE);
      return -1; 
   }
   
   AUGraphNodeInfo(graph, converter_node, &converter_desc, &converter_unit);
   AUGraphNodeInfo(graph, mixer_node, &mixer_desc, &mixer_unit);
   AUGraphNodeInfo(graph, output_node, &output_desc, &output_unit);

   UInt32 buscount = 1;
   if (AudioUnitSetProperty(mixer_unit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &buscount, sizeof(buscount))) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Error initialising mixer"));
      ca_exit(FALSE);
      return -1; 
   }

   AURenderCallbackStruct render_cb = {0};
   render_cb.inputProc = render_callback;
    if (AUGraphSetNodeInputCallback(graph, converter_node, 0, &render_cb)) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot set audio rendering callback"));
      ca_exit(FALSE);
      return -1;
   }


   AudioStreamBasicDescription output_format_desc = {0};
   property_size = sizeof(output_format_desc);
   if (AudioUnitGetProperty(output_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &output_format_desc, &property_size)) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot detect output audio format"));
      ca_exit(FALSE);
      return -1;
   }
   // printf("output sample rate: %f\n", output_format_desc.mSampleRate);
   

   AudioStreamBasicDescription input_format_desc = {0};
//    input_format_desc.mSampleRate = 22050.0;
//    input_format_desc.mSampleRate = 44100.0;
   input_format_desc.mSampleRate = output_format_desc.mSampleRate;
   input_format_desc.mFormatID = kAudioFormatLinearPCM;
#ifdef ALLEGRO_BIG_ENDIAN
   input_format_desc.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
#else
   input_format_desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
#endif
   input_format_desc.mBytesPerPacket = 4;
   input_format_desc.mFramesPerPacket = 1;
   input_format_desc.mBytesPerFrame = 4;
   input_format_desc.mChannelsPerFrame = 2;
   input_format_desc.mBitsPerChannel = 16;
   
   if( AudioUnitSetProperty(converter_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &input_format_desc, sizeof(input_format_desc))) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot configure format converter audio unit"));
      ca_exit(FALSE);
      return -1;
   }

   _sound_bits = 16;
   _sound_stereo = TRUE;
   _sound_freq = (int)input_format_desc.mSampleRate;


   // essentially we were guessing what the buffer size would be, but size changes over time if the output
   // device changes.

   int audio_buffer_size_in_frames = 512;  // Seems to be initally 512 on average
   int audio_buffer_size_in_samples = audio_buffer_size_in_frames * 2;
   audio_buffer_size /* in bytes */ = audio_buffer_size_in_samples * sizeof(short);
   audio_buffer = _AL_MALLOC_ATOMIC(audio_buffer_size);
   audio_buffer_offset = audio_buffer_size; // set to end so it will fill immediately

   digi_core_audio.voices = voices;
   if (_mixer_init(audio_buffer_size_in_samples, _sound_freq, _sound_stereo, TRUE, &digi_core_audio.voices)) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Error initialising mixer"));
      ca_exit(FALSE);
      return -1;
   }


   AUGraphInitialize(graph);

   AUGraphStart(graph);


   AudioDeviceID audio_device;

   property_size = sizeof(audio_device);
   if (AudioUnitGetProperty(output_unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Output, 0, &audio_device, &property_size)) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot get CoreAudio device"));
      ca_exit(FALSE);
      return -1;
   }

   // describe audio device, write to digi_core_audio.desc
   char device_name[64];
   char manufacturer[64];
   UInt32 device_name_size = sizeof(device_name);
   UInt32 manufacturer_size = sizeof(manufacturer);
   AudioObjectPropertyAddress deviceNameAddr = {kAudioDevicePropertyDeviceName, kAudioDevicePropertyScopeOutput, 0};
   AudioObjectPropertyAddress deviceManuAddr = {kAudioDevicePropertyDeviceManufacturer, kAudioDevicePropertyScopeOutput, 0};
   if (!AudioObjectGetPropertyData(audio_device, &deviceNameAddr, 0, NULL, &device_name_size, device_name) &&
       !AudioObjectGetPropertyData(audio_device, &deviceManuAddr, 0, NULL, &manufacturer_size, manufacturer)) 
   {
      char tmp1[256];
      char tmp2[256];
      char tmp3[256];
      uszprintf(ca_desc, sizeof(ca_desc), get_config_text("%s (%s), 16 bits (%d real), %d bps, %s"),
         uconvert_ascii(device_name, tmp1), uconvert_ascii(manufacturer, tmp2), output_format_desc.mBitsPerChannel,
	 _sound_freq, uconvert_ascii(_sound_stereo ? "stereo" : "mono", tmp3));
   }
   else {
      char tmp3[256];
      uszprintf(ca_desc, sizeof(ca_desc), get_config_text("16 bits (%d real), %d bps, %s"), output_format_desc.mBitsPerChannel,
	 _sound_freq, uconvert_ascii(_sound_stereo ? "stereo" : "mono", tmp3));
   }
   digi_core_audio.desc = ca_desc;
   
   return 0;
}



static void ca_exit(int input)
{
   if (input)
      return;
   AUGraphStop(graph);
   AUGraphUninitialize(graph);
   AUGraphClose(graph);
   DisposeAUGraph(graph);
   _mixer_exit();
   _AL_FREE(audio_buffer);
   audio_buffer = NULL;
}



static int ca_buffer_size()
{
   return audio_buffer_size;
}



static int ca_set_mixer_volume(int volume)
{
   float value = (float)volume / 255.0;
   return AudioUnitSetParameter(output_unit, kAudioUnitParameterUnit_LinearGain, kAudioUnitScope_Output, 0, value, 0);
}

