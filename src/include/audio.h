 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Sound emulation stuff
  *
  * Copyright 1995, 1996, 1997 Bernd Schmidt
  */

#ifndef UAE_AUDIO_H
#define UAE_AUDIO_H

#include "uae/types.h"

#define PERIOD_MAX UINT_MAX
#define MAX_EV ~0u

void AUDxDAT (int nr, uae_u16 value);
void AUDxVOL (int nr, uae_u16 value);
void AUDxPER (int nr, uae_u16 value);
void AUDxLCH (int nr, uae_u16 value);
void AUDxLCL (int nr, uae_u16 value);
void AUDxLEN (int nr, uae_u16 value);

uae_u16 audio_dmal (void);
void audio_state_machine (void);
void audio_dmal_do (int nr, bool reset);

int init_audio (void);
void audio_reset (void);
void update_audio (void);
void audio_evhandler (void);
void audio_hsync (void);
void audio_update_adkmasks (void);
void update_sound (float clk);
void led_filter_audio (void);
void set_audio(void);
int audio_activate(void);
void audio_deactivate (void);

typedef void(*CDA_CALLBACK)(int, void*);
typedef bool(*SOUND_STREAM_CALLBACK)(int, void*);

struct cd_audio_state
{
	uae_s16 *cda_bufptr;
	int cda_length, cda_userdata;
	CDA_CALLBACK cda_next_cd_audio_buffer_callback;
	void *cb_data;
	int cda_volume[2];
	int cda_streamid = -1;
};

extern void audio_cda_new_buffer(struct cd_audio_state *cas, uae_s16 *buffer, int length, int userdata, CDA_CALLBACK next_cd_audio_buffer_callback, void *cb_data);
extern void audio_cda_volume(struct cd_audio_state *cas, int left, int right);

extern int sound_cd_volume[2];

#define AUDIO_CHANNELS_PAULA 4

enum {
  SND_MONO,
  SND_STEREO,
  SND_NONE
};

STATIC_INLINE int get_audio_ismono (int stereomode)
{
	return stereomode == 0;
}

#define SOUND_MAX_DELAY_BUFFER 1024
#define SOUND_MAX_LOG_DELAY 10
#define MIXED_STEREO_MAX 16
#define MIXED_STEREO_SCALE 32

#endif /* UAE_AUDIO_H */
