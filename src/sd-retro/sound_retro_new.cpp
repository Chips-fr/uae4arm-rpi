 /* 
  * Sdl sound.c implementation
  * (c) 2015
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
#if defined(__LIBRETRO__)
#include "sd-retro/sound.h"
#else
#include "sd-pandora/sound.h"
#endif
#include "custom.h"

#ifdef ANDROIDSDL
#include <android/log.h>
#endif

extern unsigned long next_sample_evtime;

int produce_sound=0;
int changed_produce_sound=0;

uae_u16 sndbuffer[SOUND_BUFFERS_COUNT][(SNDBUFFER_LEN+32)*DEFAULT_SOUND_CHANNELS];
unsigned n_callback_sndbuff, n_render_sndbuff;
uae_u16 *sndbufpt = sndbuffer[0];
uae_u16 *render_sndbuff = sndbuffer[0];
uae_u16 *finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN*2;

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


static int have_sound = 0;

void update_sound (float clk)
{
  float evtime;
  
  evtime = clk * CYCLE_UNIT / (float)currprefs.sound_freq;
	scaled_sample_evtime = (int)evtime;
}

static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;


#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static int rdcnt = 0;

static int wrcnt = 0;


static void sound_copy_produced_block(void *ud, unsigned char *stream, int len)
{

	//__android_log_print(ANDROID_LOG_INFO, "UAE4ALL2","Sound callback cnt %d buf %d\n", cnt, cnt%SOUND_BUFFERS_COUNT);
	if(currprefs.sound_stereo)
	{
		if(cdaudio_active && currprefs.sound_freq == 44100 && cdrdcnt < cdwrcnt)
		{
			for(int i=0; i<SNDBUFFER_LEN * 2 ; ++i)
				sndbuffer[rdcnt % SOUND_BUFFERS_COUNT][i] += cdaudio_buffer[cdrdcnt & (CDAUDIO_BUFFERS - 1)][i];
		}
	
		memcpy(stream, sndbuffer[rdcnt%SOUND_BUFFERS_COUNT], MIN(SNDBUFFER_LEN*4, len));
	}
	else
	  	memcpy(stream, sndbuffer[rdcnt%SOUND_BUFFERS_COUNT], MIN(SNDBUFFER_LEN * 2, len));


}

static void init_soundbuffer_usage(void)
{
  sndbufpt = sndbuffer[0];
  render_sndbuff = sndbuffer[0];
  finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN * 2;
  //output_cnt = 0;
  rdcnt = 0;
  wrcnt = 0;
  
  cdbufpt = cdaudio_buffer[0];
  render_cdbuff = cdaudio_buffer[0];
  finish_cdbuff = cdaudio_buffer[0] + CDAUDIO_BUFFER_LEN * 2;
  cdrdcnt = 0;
  cdwrcnt = 0;
}



static int pandora_start_sound(int rate, int bits, int stereo)
{
	int frag = 0, buffers, ret;
	unsigned int bsize;

	printf("starting sound thread..rate:%d bits:%d stereo:%d\n",rate,bits,stereo);

	// if no settings change, we don't need to do anything
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo)
	    return 0;

	init_soundbuffer_usage();

	s_oldrate = rate; 
	s_oldbits = bits; 
	s_oldstereo = stereo;

	return 0;
}


// this is meant to be called only once on exit
void pandora_stop_sound(void)
{

}

//static int wrcnt = 0;
void finish_sound_buffer (void)
{

#ifdef DEBUG_SOUND
	dbg("sound.c : finish_sound_buffer");
#endif



	extern void retro_audiocb(signed short int *sound_buffer,int sndbufsize);
	retro_audiocb(sndbuffer[wrcnt%SOUND_BUFFERS_COUNT], SNDBUFFER_LEN/2);
	wrcnt++;
	sndbufpt = render_sndbuff = sndbuffer[wrcnt%SOUND_BUFFERS_COUNT];
	//__android_log_print(ANDROID_LOG_INFO, "UAE4ALL2","Sound buffer write cnt %d buf %d\n", wrcnt, wrcnt%SOUND_BUFFERS_COUNT);


	if(currprefs.sound_stereo)
	  finish_sndbuff = sndbufpt + SNDBUFFER_LEN;
	else
	  finish_sndbuff = sndbufpt + SNDBUFFER_LEN/2;	

#ifdef DEBUG_SOUND
	dbg(" sound.c : ! finish_sound_buffer");
#endif
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
  return true;
#if 0
  while((cdwrcnt > cdrdcnt + CDAUDIO_BUFFERS - 30) && (sound_thread_active != 0) && (quit_program == 0)) {
    sleep_millis(10);
  }
  return (sound_thread_active != 0);
#endif
}


/* Try to determine whether sound is available.  This is only for GUI purposes.  */
int setup_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : setup_sound");
#endif

     // Android does not like opening sound device several times
     if (pandora_start_sound(currprefs.sound_freq, 16, currprefs.sound_stereo) != 0)
        return 0;

     sound_available = 1;

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! setup_sound");
#endif
    return 1;
}
/*
void update_sound (int freq)
{
  sound_default_evtime(freq);
}
*/

static int open_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : open_sound");
#endif

	// Android does not like opening sound device several times
    if (pandora_start_sound(currprefs.sound_freq, 16, currprefs.sound_stereo) != 0)
	    return 0;
   // init_sound_table16 ();

    have_sound = 1;
    sound_available = 1;

    if(currprefs.sound_stereo)
      sample_handler = sample16s_handler;
    else
      sample_handler = sample16_handler;
 

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! open_sound");
#endif
    return 1;
}

void close_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : close_sound");
#endif
    if (!have_sound)
	return;

    // testing shows that reopenning sound device is not a good idea (causes random sound driver crashes)
    // we will close it on real exit instead

    have_sound = 0;

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! close_sound");
#endif
}

int init_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : init_sound");
#endif

    have_sound=open_sound();

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! init_sound");
#endif
    return have_sound;
}

void pause_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : pause_sound");
#endif

	//SDL_PauseAudio (1);
    /* nothing to do */

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! pause_sound");
#endif
}

void resume_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : resume_sound");
#endif

	//SDL_PauseAudio (0);
    /* nothing to do */

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! resume_sound");
#endif
}

void uae4all_init_sound(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : uae4all_init_sound");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! uae4all_init_sound");
#endif
}

void uae4all_pause_music(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : pause_music");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! pause_music");
#endif
}

void uae4all_resume_music(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : resume_music");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! resume_music");
#endif
}

void uae4all_play_click(void)
{
}

void reset_sound (void)
{
  if (!have_sound)
  	return;


  init_soundbuffer_usage();

  clear_sound_buffers();
  clear_cdaudio_buffers();
}


void sound_volume (int dir)
{
}

#endif

