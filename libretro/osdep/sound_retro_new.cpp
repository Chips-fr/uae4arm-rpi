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
#include "debug.h"
#include "audio.h"
#include "gensound.h"
#if defined(__LIBRETRO__)
#include "osdep/sound.h"
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

#else 


static int have_sound = 0;
static int lastfreq;

extern uae_u16 new_beamcon0;

void sound_default_evtime(int freq)
{
	int pal = new_beamcon0 & 0x20;

	if (freq < 0)
		freq = lastfreq;
	lastfreq = freq;

#if 0
                // 41114
		if (pal)
			scaled_sample_evtime = (MAXHPOS_PAL * MAXVPOS_PAL * freq * CYCLE_UNIT + currprefs.sound_freq - 1) / currprefs.sound_freq;
		else
			scaled_sample_evtime = (MAXHPOS_NTSC * MAXVPOS_NTSC * freq * CYCLE_UNIT + currprefs.sound_freq - 1) / currprefs.sound_freq;
#else
                // 41245
		if (pal)
			scaled_sample_evtime=(MAXHPOS_PAL*313*VBLANK_HZ_PAL*CYCLE_UNIT)/currprefs.sound_freq;
		else
			scaled_sample_evtime=(MAXHPOS_NTSC*MAXVPOS_NTSC*VBLANK_HZ_NTSC*CYCLE_UNIT)/currprefs.sound_freq + 1;
#endif
}


static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
static int cnt = 0;

void sound_thread_mixer(void *ud, unsigned char *stream, int len)
{

	cnt++;
	//__android_log_print(ANDROID_LOG_INFO, "UAE4ALL2","Sound callback cnt %d buf %d\n", cnt, cnt%SOUND_BUFFERS_COUNT);
	if(currprefs.sound_stereo)
		memcpy(stream, sndbuffer[cnt%SOUND_BUFFERS_COUNT], MIN(SNDBUFFER_LEN*2, len));
	else
	  	memcpy(stream, sndbuffer[cnt%SOUND_BUFFERS_COUNT], MIN(SNDBUFFER_LEN, len));

}

static int pandora_start_sound(int rate, int bits, int stereo)
{
	int frag = 0, buffers, ret;
	unsigned int bsize;

	s_oldrate = rate; 
	s_oldbits = bits; 
	s_oldstereo = stereo;

	return 0;
}


// this is meant to be called only once on exit
void pandora_stop_sound(void)
{

}

static int wrcnt = 0;
void finish_sound_buffer (void)
{

#ifdef DEBUG_SOUND
	dbg("sound.c : finish_sound_buffer");
#endif

	extern void retro_audiocb(signed short int *sound_buffer,int sndbufsize);
	retro_audiocb(sndbuffer[wrcnt%SOUND_BUFFERS_COUNT], SNDBUFFER_LEN);


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


void restart_sound_buffer(void)
{
	sndbufpt = render_sndbuff = sndbuffer[wrcnt%SOUND_BUFFERS_COUNT];
	if(currprefs.sound_stereo)
	  finish_sndbuff = sndbufpt + SNDBUFFER_LEN;
	else
	  finish_sndbuff = sndbufpt + SNDBUFFER_LEN/2;	  
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

void update_sound (int freq)
{
  sound_default_evtime(freq);
}


static int open_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : open_sound");
#endif

	// Android does not like opening sound device several times
    if (pandora_start_sound(currprefs.sound_freq, 16, currprefs.sound_stereo) != 0)
	    return 0;

    init_sound_table16 ();

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

  memset(sndbuffer, 0, sizeof(sndbuffer));
}


void sound_volume (int dir)
{
}

#endif

