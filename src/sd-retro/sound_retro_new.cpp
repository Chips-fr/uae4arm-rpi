 /* 
  * libretro sound.c implementation
  * (c) 2021 Chips
  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory.h"
#include "audio.h"
#include "gensound.h"
#include "sd-retro/sound.h"
#include "custom.h"

#ifdef ANDROIDSDL
#include <android/log.h>
#endif

extern unsigned long next_sample_evtime;

uae_u16 sndbuffer[SOUND_BUFFERS_COUNT][(SNDBUFFER_LEN+32)*DEFAULT_SOUND_CHANNELS];
unsigned n_callback_sndbuff, n_render_sndbuff;
uae_u16 *sndbufpt = sndbuffer[0];
uae_u16 *render_sndbuff = sndbuffer[0];
uae_u16 *finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN*2;
static int rdcnt = 0;
static int wrcnt = 0;

uae_u16 cdaudio_buffer[CDAUDIO_BUFFERS][(CDAUDIO_BUFFER_LEN + 32) * 2];
uae_u16 *cdbufpt = cdaudio_buffer[0];
uae_u16 *render_cdbuff = cdaudio_buffer[0];
uae_u16 *finish_cdbuff = cdaudio_buffer[0] + CDAUDIO_BUFFER_LEN * 2;
bool cdaudio_active = false;
static int cdwrcnt = 0;
static int cdrdcnt = 0;

#ifdef NO_SOUND


void finish_sound_buffer (void) {  }

int setup_sound (void) { sound_available = 0; return 0; }

void close_sound (void) { }

void pandora_stop_sound (void) { }

int init_sound (void) { return 0; }

void pause_sound (void) { }

void resume_sound (void) { }

void update_sound (int) { }

void reset_sound (void) { }

void uae4all_init_sound(void) { }

void uae4all_play_click(void) { }

void uae4all_pause_music(void) { }

void uae4all_resume_music(void) { }

void restart_sound_buffer(void) { }

void finish_cdaudio_buffer (void) { }

bool cdaudio_catchup(void) { }

void sound_volume (int dir) { }
#else 

void update_sound (float clk)
{
  float evtime;
  
  evtime = clk * CYCLE_UNIT / (float)currprefs.sound_freq;
  scaled_sample_evtime = (int)evtime;
}


#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))


static void sound_copy_produced_block(void)
{

  if(currprefs.sound_stereo)
  {
    if(cdaudio_active && currprefs.sound_freq == 44100 && cdrdcnt < cdwrcnt)
    {
      for(int i=0; i<SNDBUFFER_LEN * 2 ; ++i)
        sndbuffer[rdcnt % SOUND_BUFFERS_COUNT][i] += cdaudio_buffer[cdrdcnt & (CDAUDIO_BUFFERS - 1)][i];
      cdrdcnt++;
    }
  }
  //rdcnt++;
}

static void init_soundbuffer_usage(void)
{
  sndbufpt = sndbuffer[0];
  render_sndbuff = sndbuffer[0];
  finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN * 2;
  rdcnt = 0;
  wrcnt = 0;
  
  cdbufpt = cdaudio_buffer[0];
  render_cdbuff = cdaudio_buffer[0];
  finish_cdbuff = cdaudio_buffer[0] + CDAUDIO_BUFFER_LEN * 2;
  cdrdcnt = 0;
  cdwrcnt = 0;
}

// this is meant to be called only once on exit
void pandora_stop_sound(void)
{

}

extern void retro_audiocb(signed short *sound_buffer,int sndbufsize);


void finish_sound_buffer (void)
{

#ifdef DEBUG_SOUND
  dbg("sound.c : finish_sound_buffer");
#endif
  // In order to mix with cd audio and synchrously send to retroarch...
  sound_copy_produced_block();

  retro_audiocb((short int*) sndbuffer[wrcnt%SOUND_BUFFERS_COUNT], SNDBUFFER_LEN);
  //wrcnt++;
  sndbufpt = render_sndbuff = sndbuffer[wrcnt%SOUND_BUFFERS_COUNT];


  if(currprefs.sound_stereo)
    finish_sndbuff = sndbufpt + SNDBUFFER_LEN * 2;
  else
    finish_sndbuff = sndbufpt + SNDBUFFER_LEN;

#ifdef DEBUG_SOUND
  dbg(" sound.c : ! finish_sound_buffer");
#endif
}

void flush_audio(void)
{

  // Flush audio buffer in order to render all audio samples for a given frame. It's better for some frontend

  // cdaudio buffer is handled asynchronously so we can't flush in case of cda...
  if(cdaudio_active)
    return;

  retro_audiocb((short int*) sndbuffer[wrcnt%SOUND_BUFFERS_COUNT], (sndbufpt - render_sndbuff)/2);

  init_soundbuffer_usage();
}


void pause_sound_buffer (void)
{
  reset_sound ();
}

void restart_sound_buffer(void)
{
  sndbufpt = render_sndbuff = sndbuffer[wrcnt % SOUND_BUFFERS_COUNT];
  if(currprefs.sound_stereo)
    finish_sndbuff = sndbufpt + SNDBUFFER_LEN * 2;
  else
    finish_sndbuff = sndbufpt + SNDBUFFER_LEN;

  cdbufpt = render_cdbuff = cdaudio_buffer[cdwrcnt & (CDAUDIO_BUFFERS - 1)];
  finish_cdbuff = cdbufpt + CDAUDIO_BUFFER_LEN * 2;  
}

void finish_cdaudio_buffer (void)
{
  cdwrcnt++;
  cdbufpt = render_cdbuff = cdaudio_buffer[cdwrcnt & (CDAUDIO_BUFFERS - 1)];
  finish_cdbuff = cdbufpt + CDAUDIO_BUFFER_LEN * 2;
  audio_activate();
}

bool cdaudio_catchup(void)
{
  while((cdwrcnt > cdrdcnt + CDAUDIO_BUFFERS - 30) && (quit_program == 0))
    sleep_millis(10);
  return 1;
}


/* Try to determine whether sound is available.  This is only for GUI purposes.  */
int setup_sound (void)
{
#ifdef DEBUG_SOUND
  dbg("sound.c : setup_sound");
#endif

  init_soundbuffer_usage();

  sound_available = 1;

#ifdef DEBUG_SOUND
  dbg(" sound.c : ! setup_sound");
#endif
  return 1;
}


void close_sound (void)
{
}

int init_sound (void)
{
#ifdef DEBUG_SOUND
  dbg("sound.c : init_sound");
#endif

  init_soundbuffer_usage();
  // init_sound_table16 ();

  sound_available = 1;

  if(currprefs.sound_stereo)
    sample_handler = sample16s_handler;
  else
    sample_handler = sample16_handler;

#ifdef DEBUG_SOUND
  dbg(" sound.c : ! init_sound");
#endif
  return 1;
}

void pause_sound (void)
{
}

void resume_sound (void)
{
}

void uae4all_init_sound(void)
{
}

void uae4all_pause_music(void)
{
}

void uae4all_resume_music(void)
{
}

void uae4all_play_click(void)
{
}

void reset_sound (void)
{
  init_soundbuffer_usage();

  clear_sound_buffers();
  clear_cdaudio_buffers();
}


void sound_volume (int dir)
{
}

#endif

