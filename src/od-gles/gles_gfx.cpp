#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "gui.h"
#include "memory.h"
#include "newcpu.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "savestate.h"
#include "picasso96.h"

#include <png.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>
#include "gl.h"
#include <SDL_syswm.h>
#include "td-sdl/thread.h"

#ifdef USE_RENDER_THREAD
#define DISPLAY_SIGNAL_SETUP 				1
#define DISPLAY_SIGNAL_SUBSHUTDOWN 			2
#define DISPLAY_SIGNAL_OPEN 				3
#define DISPLAY_SIGNAL_SHOW 				4
#define DISPLAY_SIGNAL_QUIT 				5
static uae_thread_id display_tid = 0;
static smp_comm_pipe *volatile display_pipe = 0;
static uae_sem_t display_sem = 0;
static bool volatile display_thread_busy = false;
#endif

static int display_width;
static int display_height;

const SDL_VideoInfo* videoInfo;

/* SDL surface variable for output of emulation */
SDL_Surface *prSDLScreen = NULL;
static unsigned int current_vsync_frame = 0;
/* Dummy SDL variable for screen init */
SDL_Surface *Dummy_prSDLScreen = NULL;

unsigned long time_per_frame = 20000; // Default for PAL (50 Hz): 20000 microsecs
static unsigned long last_synctime;
static int vsync_modulo = 1;
static int host_hz = 50;

/* Possible screen modes (x and y resolutions) */
#define MAX_SCREEN_MODES 14
static int x_size_table[MAX_SCREEN_MODES] = {640, 640, 720, 800, 800, 960, 1024, 1280, 1280, 1280, 1360, 1366, 1680, 1920};
static int y_size_table[MAX_SCREEN_MODES] = {400, 480, 400, 480, 600, 540, 768, 720, 800, 1024, 768, 768, 1050, 1080};

#ifdef PICASSO96
#define TEXTURE_WIDTH  2048
#define TEXTURE_HEIGHT 2048
#else
#define TEXTURE_WIDTH  1024
#define TEXTURE_HEIGHT 1024

#endif


static int red_bits, green_bits, blue_bits;
static int red_shift, green_shift, blue_shift;

struct PicassoResolution *DisplayModes;
struct MultiDisplay Displays[MAX_DISPLAYS];

int screen_is_picasso = 0;

static SDL_Surface *current_screenshot = NULL;
static int curr_layer_width = 0;

static char vid_drv_name[32];
static void *display, *window;
static int gl_quirks;

static char screenshot_filename_default[MAX_DPATH]={
	'/', 't', 'm', 'p', '/', 'n', 'u', 'l', 'l', '.', 'p', 'n', 'g', '\0'
};
char *screenshot_filename=(char *)&screenshot_filename_default[0];
FILE *screenshot_file=NULL;
static void CreateScreenshot(void);
static int save_thumb(char *path);
int delay_savestate_frame = 0;

static long next_synctime = 0;

long start;

static volatile uae_atomic vsync_counter = 0;

SDL_SysWMinfo wminfo;
uint32_t vc_image_ptr;


void signal_open(void)
{
	const SDL_VideoInfo* videoInfo = SDL_GetVideoInfo ();
	printf("Current resolution: %d x %d %d bpp\n",videoInfo->current_w, videoInfo->current_h, videoInfo->vfmt->BitsPerPixel);
	//Dummy_prSDLScreen = SDL_SetVideoMode(videoInfo->current_w,videoInfo->current_h,16,SDL_SWSURFACE |SDL_FULLSCREEN);
	Dummy_prSDLScreen = SDL_SetVideoMode(800,480,16,SDL_SWSURFACE );

	printf("Emulation resolution: Width %i Height: %i\n",display_width,display_height);
	prSDLScreen = SDL_CreateRGBSurface(SDL_SWSURFACE,display_width,display_height,16,
		Dummy_prSDLScreen->format->Rmask,
		Dummy_prSDLScreen->format->Gmask,
		Dummy_prSDLScreen->format->Bmask,
		Dummy_prSDLScreen->format->Amask);


	// get x11 display/window for GL
	SDL_VideoDriverName(vid_drv_name, sizeof(vid_drv_name));
	#ifdef SDL_VIDEO_DRIVER_X11
	if (strcmp(vid_drv_name, "x11") == 0) {
	  SDL_VERSION(&wminfo.version);
	  if (SDL_GetWMInfo(&wminfo)> 0) {
	    display = wminfo.info.x11.display;
	    window = (void *)wminfo.info.x11.window;
	  } else
	  printf("Error getting SDL info\n");
	}
	#else
	  (void)wminfo;
	#endif
	if (gl_init(display, window, &gl_quirks, TEXTURE_WIDTH, TEXTURE_HEIGHT)== 0) {
	} else
	printf("Error: Can't init gl subsystem.\n");
}


#ifdef USE_RENDER_THREAD
static int display_thread (void *unused)
{

  for(;;) {
    display_thread_busy = false;
    uae_u32 signal = read_comm_pipe_u32_blocking(display_pipe);
    display_thread_busy = true;
    switch(signal) {
			case DISPLAY_SIGNAL_SETUP:
			  break;

			case DISPLAY_SIGNAL_SUBSHUTDOWN:
				gl_finish();
				if(prSDLScreen != NULL) {
					SDL_FreeSurface(prSDLScreen);
					prSDLScreen = NULL;
				}
				uae_sem_post (&display_sem);
				break;
				
			case DISPLAY_SIGNAL_OPEN:
				signal_open();

				uae_sem_post (&display_sem);
				break;

			case DISPLAY_SIGNAL_SHOW:
				gl_flip(gfxvidinfo.drawbuffer.bufmem, gfxvidinfo.drawbuffer.outwidth, gfxvidinfo.drawbuffer.outheight);
				atomic_inc(&vsync_counter);
				break;
								
			case DISPLAY_SIGNAL_QUIT:
				SDL_VideoQuit();
				display_tid = 0;
				return 0;
    }
  }
}
#endif

int graphics_setup(void)
{
#ifdef PICASSO96
  picasso_InitResolutions();
  InitPicasso96();
#endif
#if 0
  VCHI_INSTANCE_T vchi_instance;
  VCHI_CONNECTION_T *vchi_connection;
  TV_DISPLAY_STATE_T tvstate;

  if(vchi_initialise(&vchi_instance) == 0) {
    if(vchi_connect(NULL, 0, vchi_instance) == 0) {
      vc_vchi_tv_init(vchi_instance, &vchi_connection, 1);
      if(vc_tv_get_display_state(&tvstate) == 0) {
        HDMI_PROPERTY_PARAM_T property;
        property.property = HDMI_PROPERTY_PIXEL_CLOCK_TYPE;
        vc_tv_hdmi_get_property(&property);
        float frame_rate = property.param1 == HDMI_PIXEL_CLOCK_TYPE_NTSC ? tvstate.display.hdmi.frame_rate * (1000.0f/1001.0f) : tvstate.display.hdmi.frame_rate;
        host_hz = (int)frame_rate;
      }
      vc_vchi_tv_stop();
      vchi_disconnect(vchi_instance);
    }
  }
#endif

#ifdef USE_RENDER_THREAD
  if(display_pipe == 0) {
    display_pipe = xmalloc (smp_comm_pipe, 1);
    init_comm_pipe(display_pipe, 20, 1);
  }
  if(display_sem == 0) {
    uae_sem_init (&display_sem, 0, 0);
  }
  if(display_tid == 0 && display_pipe != 0 && display_sem != 0) {
    uae_start_thread(_T("render"), display_thread, NULL, &display_tid);
  }
  videoInfo = SDL_GetVideoInfo ();
  write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SETUP, 1);
#endif	
	return 1;
}

static void wait_for_display_thread(void)
{
#ifdef USE_RENDER_THREAD
	while(display_thread_busy)
		usleep(10);
#endif
}



#ifdef WITH_LOGGING

SDL_Surface *liveInfo = NULL;
TTF_Font *liveFont = NULL;
int liveInfoCounter = 0;
void ShowLiveInfo(char *msg)
{
  if(liveFont == NULL)
  {
    TTF_Init();
    liveFont = TTF_OpenFont("data/FreeSans.ttf", 12);
  }
  if(liveInfo != NULL) {
    SDL_FreeSurface(liveInfo);
    liveInfo = NULL;
  }
  SDL_Color col;
  col.r = 0xbf;
  col.g = 0xbf;
  col.b = 0xbf;
  liveInfo = TTF_RenderText_Solid(liveFont, msg, col);
  liveInfoCounter = 50 * 5;
}

void RefreshLiveInfo()
{
  if(liveInfoCounter > 0)
  {
    SDL_Rect dst, src;
    
    dst.x = 0;
    dst.y = 2;
    src.w = liveInfo->w;
    src.h = liveInfo->h;
    src.x = 0;
    src.y = 0;
    if(liveInfo != NULL)
      SDL_BlitSurface(liveInfo, &src, prSDLScreen, &dst);
    liveInfoCounter--;
    if(liveInfoCounter == 0)
    {
      if(liveInfo != NULL) {
        SDL_FreeSurface(liveInfo);
        liveInfo = NULL;
      }
    }
  }
}

#endif

void InitAmigaVidMode(struct uae_prefs *p)
{
  /* Initialize structure for Amiga video modes */
  gfxvidinfo.drawbuffer.pixbytes = 2;
  gfxvidinfo.drawbuffer.bufmem = (uae_u8 *)prSDLScreen->pixels;
  gfxvidinfo.drawbuffer.outwidth = p->gfx_size.width;
  gfxvidinfo.drawbuffer.outheight = p->gfx_size.height << p->gfx_vresolution;
#ifdef PICASSO96
  if(screen_is_picasso)
  {
    gfxvidinfo.drawbuffer.outwidth  = picasso_vidinfo.width;
    gfxvidinfo.drawbuffer.outheight = picasso_vidinfo.height;
  }
#endif
  gfxvidinfo.drawbuffer.rowbytes = prSDLScreen->pitch;
}


void graphics_subshutdown(void)
{

#ifdef USE_RENDER_THREAD
  if(display_tid != 0) {
    wait_for_display_thread();
    write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SUBSHUTDOWN, 1);
    uae_sem_wait (&display_sem);
  }
#else
  gl_finish();
  if(prSDLScreen != NULL) {
    SDL_FreeSurface(prSDLScreen);
    prSDLScreen = NULL;
  }
#endif


  if (Dummy_prSDLScreen != NULL)
  { 
    // y.f. 2016-10-13 : free the previous screen surface every time, 
    // so we can have fullscreen while running and windowed while in config window.
    // Apparently, something somewhere is resetting the screen.
    // Chips: confirmed with sanitize gcc option... looks like each SDL_SetVideoMode free previous surface...
    SDL_FreeSurface(Dummy_prSDLScreen);
    Dummy_prSDLScreen = NULL;
  }

}



static void open_screen(struct uae_prefs *p)
{
  graphics_subshutdown();


#ifdef PICASSO96
  if (screen_is_picasso)
  {
    display_width  = picasso_vidinfo.width;
    display_height = picasso_vidinfo.height;
  } else
#endif
  {
    p->gfx_resolution = p->gfx_size.width > 600 ? 1 : 0;
    display_width  = p->gfx_size.width;
    display_height = p->gfx_size.height << p->gfx_vresolution;
  }

#ifdef USE_RENDER_THREAD
  write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_OPEN, 1);
  uae_sem_wait (&display_sem);
#else
  signal_open();
#endif

  vsync_counter = 0;
  current_vsync_frame = 2;

  if(prSDLScreen != NULL)
  {
    InitAmigaVidMode(p);
    init_row_map();
  }    
  //framecnt = 1; // Don't draw frame before reset done
}


void update_display(struct uae_prefs *p)
{
  open_screen(p);
    
  SDL_ShowCursor(SDL_DISABLE);

  framecnt = 1; // Don't draw frame before reset done
}


int check_prefs_changed_gfx (void)
{
  int changed = 0;
  
  if(currprefs.gfx_size.height != changed_prefs.gfx_size.height ||
     currprefs.gfx_size.width != changed_prefs.gfx_size.width ||
     currprefs.gfx_resolution != changed_prefs.gfx_resolution ||
     currprefs.gfx_vresolution != changed_prefs.gfx_vresolution)
  {
  	cfgfile_configuration_change(1);
    currprefs.gfx_size.height = changed_prefs.gfx_size.height;
    currprefs.gfx_size.width = changed_prefs.gfx_size.width;
    currprefs.gfx_resolution = changed_prefs.gfx_resolution;
    currprefs.gfx_vresolution = changed_prefs.gfx_vresolution;
    update_display(&currprefs);
    changed = 1;
  }
  if (currprefs.leds_on_screen != changed_prefs.leds_on_screen ||
      currprefs.pandora_hide_idle_led != changed_prefs.pandora_hide_idle_led ||
      currprefs.pandora_vertical_offset != changed_prefs.pandora_vertical_offset)	
  {
    currprefs.leds_on_screen = changed_prefs.leds_on_screen;
    currprefs.pandora_hide_idle_led = changed_prefs.pandora_hide_idle_led;
    currprefs.pandora_vertical_offset = changed_prefs.pandora_vertical_offset;
    changed = 1;
  }
  if (currprefs.chipset_refreshrate != changed_prefs.chipset_refreshrate) 
  {
  	currprefs.chipset_refreshrate = changed_prefs.chipset_refreshrate;
	  init_hz_normal();
	  changed = 1;
  }
	// Not the correct place for this...
	currprefs.filesys_limit = changed_prefs.filesys_limit;
	currprefs.harddrive_read_only = changed_prefs.harddrive_read_only;
  
  if(changed)
		init_custom ();
  
  return changed;
}


int lockscr (void)
{
    //SDL_LockSurface(prSDLScreen);
    init_row_map();
    return 1;
}


void unlockscr (void)
{
    //SDL_UnlockSurface(prSDLScreen);
}

void wait_for_vsync(void)
{
  unsigned long start = read_processor_time();
  int wait_till = current_vsync_frame;
  do 
  {
    usleep(10);
    current_vsync_frame = vsync_counter;
  } while (wait_till >= current_vsync_frame && read_processor_time() - start < 20000);
}


bool render_screen (bool immediate)
{
    if (savestate_state == STATE_DOSAVE)
    {
        if(delay_savestate_frame > 0)
          --delay_savestate_frame;
        else
        {
            CreateScreenshot();
            save_thumb(screenshot_filename);
            savestate_state = 0;
        }
    }

#ifdef WITH_LOGGING
    RefreshLiveInfo();
#endif
  
    return true;
}

void show_screen(int mode)
{
  unsigned long start = read_processor_time();

  int wait_till = current_vsync_frame;
  if(vsync_modulo == 1) {
    // Amiga framerate is equal to host framerate
    do 
    {
      usleep(10);
      current_vsync_frame = vsync_counter;
    } while (wait_till >= current_vsync_frame && read_processor_time() - start < 40000);

    if(wait_till + 1 != current_vsync_frame) {
      // We missed a vsync...
      next_synctime = 0;
    }
  } else {
    // Amiga framerate differs from host framerate
    unsigned long wait_till_time = (next_synctime != 0) ? next_synctime : last_synctime + time_per_frame;
    if(current_vsync_frame % vsync_modulo == 0) {
      // Real vsync
      if(start < wait_till_time) {
        // We are in time, wait for vsync
        atomic_set(&vsync_counter, current_vsync_frame);
        do 
        {
          usleep(10);
          current_vsync_frame = vsync_counter;
        } while (wait_till >= current_vsync_frame && read_processor_time() - start < 40000);
      } else {
        // Too late for vsync
      }
    } else {
      // Estimate vsync by time
      while (wait_till_time > read_processor_time()) {
        usleep(10);
      }
      ++current_vsync_frame;
    }
  }

  current_vsync_frame += currprefs.gfx_framerate;

  last_synctime = read_processor_time();

#ifdef USE_RENDER_THREAD
  wait_for_display_thread();
  write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SHOW, 1);
#else
  gl_flip(gfxvidinfo.drawbuffer.bufmem, gfxvidinfo.drawbuffer.outwidth, gfxvidinfo.drawbuffer.outheight);
  atomic_inc(&vsync_counter);
#endif

  idletime += last_synctime - start;

  if (last_synctime - next_synctime > time_per_frame - (long)5000)
    next_synctime = last_synctime + time_per_frame * (1 + currprefs.gfx_framerate);
  else
    next_synctime = next_synctime + time_per_frame * (1 + currprefs.gfx_framerate);
}


unsigned long target_lastsynctime(void)
{
  return last_synctime;
}

bool show_screen_maybe (bool show)
{
	if (show)
		show_screen (0);
	return false;
}

void black_screen_now(void)
{
        SDL_FillRect(prSDLScreen,NULL,0);
	render_screen(true);
	show_screen(0);
}


static void graphics_subinit(void)
{
	if (prSDLScreen == NULL)
	{
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return;
	}
	else
	{
		SDL_ShowCursor(SDL_DISABLE);
		InitAmigaVidMode(&currprefs);
	}
}

STATIC_INLINE int bitsInMask(unsigned long mask)
{
	/* count bits in mask */
	int n = 0;
	while (mask)
	{
		n += mask & 1;
		mask >>= 1;
	}
	return n;
}


STATIC_INLINE int maskShift(unsigned long mask)
{
	/* determine how far mask is shifted */
	int n = 0;
	while (!(mask & 1))
	{
		n++;
		mask >>= 1;
	}
	return n;
}


static int init_colors(void)
{
	int red_bits, green_bits, blue_bits;
	int red_shift, green_shift, blue_shift;

	/* Truecolor: */
	red_bits = bitsInMask(prSDLScreen->format->Rmask);
	green_bits = bitsInMask(prSDLScreen->format->Gmask);
	blue_bits = bitsInMask(prSDLScreen->format->Bmask);
	red_shift = maskShift(prSDLScreen->format->Rmask);
	green_shift = maskShift(prSDLScreen->format->Gmask);
	blue_shift = maskShift(prSDLScreen->format->Bmask);
	alloc_colors64k(red_bits, green_bits, blue_bits, red_shift, green_shift, blue_shift, 0);
	notice_new_xcolors();

	return 1;
}


/*
 * Find the colour depth of the display
 */
static int get_display_depth (void)
{
  const SDL_VideoInfo *vid_info;
  int depth = 0;

  if ((vid_info = SDL_GetVideoInfo())) {
  	depth = vid_info->vfmt->BitsPerPixel;

	  /* Don't trust the answer if it's 16 bits; the display
	   * could actually be 15 bits deep. We'll count the bits
	   * ourselves */
	  if (depth == 16)
	    depth = bitsInMask (vid_info->vfmt->Rmask) + bitsInMask (vid_info->vfmt->Gmask) + bitsInMask (vid_info->vfmt->Bmask);
  }
  return depth;
}

int GetSurfacePixelFormat(void)
{
  int depth = get_display_depth();
  int unit = (depth + 1) & 0xF8;

  return (unit == 8 ? RGBFB_CHUNKY
		: depth == 15 && unit == 16 ? RGBFB_R5G5B5
		: depth == 16 && unit == 16 ? RGBFB_R5G6B5
		: unit == 24 ? RGBFB_B8G8R8
		: unit == 32 ? RGBFB_R8G8B8A8
		: RGBFB_NONE);
}


int graphics_init(bool mousecapture)
{
	graphics_subinit();

	if (!init_colors())
		return 0;
  
	return 1;
}

void graphics_leave (void)
{
	graphics_subshutdown ();
	if (Dummy_prSDLScreen != NULL) {
		SDL_FreeSurface(Dummy_prSDLScreen);
		Dummy_prSDLScreen = NULL;
	}
#ifdef USE_RENDER_THREAD
	if(display_tid != 0) {
	  write_comm_pipe_u32 (display_pipe, DISPLAY_SIGNAL_QUIT, 1);
	  while(display_tid != 0) {
	    sleep_millis(10);
	  }
	  destroy_comm_pipe(display_pipe);
	  xfree(display_pipe);
	  display_pipe = 0;
	  uae_sem_destroy(&display_sem);
	  display_sem = 0;
	}
#else
	{
	SDL_VideoQuit();
	}
#endif
}


#define  systemRedShift      (prSDLScreen->format->Rshift)
#define  systemGreenShift    (prSDLScreen->format->Gshift)
#define  systemBlueShift     (prSDLScreen->format->Bshift)
#define  systemRedMask       (prSDLScreen->format->Rmask)
#define  systemGreenMask     (prSDLScreen->format->Gmask)
#define  systemBlueMask      (prSDLScreen->format->Bmask)

static int save_png(SDL_Surface* surface, char *path)
{
  int w = surface->w;
  int h = surface->h;
  unsigned char * pix = (unsigned char *)surface->pixels;
  unsigned char writeBuffer[1024 * 3];
  FILE *f  = fopen(path,"wb");
  if(!f) return 0;
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                NULL,
                                                NULL,
                                                NULL);
  if(!png_ptr) {
    fclose(f);
    return 0;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);

  if(!info_ptr) {
    png_destroy_write_struct(&png_ptr,NULL);
    fclose(f);
    return 0;
  }

  png_init_io(png_ptr,f);

  png_set_IHDR(png_ptr,
               info_ptr,
               w,
               h,
               8,
               PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr,info_ptr);

  unsigned char *b = writeBuffer;

  int sizeX = w;
  int sizeY = h;
  int y;
  int x;

  unsigned short *p = (unsigned short *)pix;
  for(y = 0; y < sizeY; y++) 
  {
     for(x = 0; x < sizeX; x++) 
     {
       unsigned short v = p[x];
  
       *b++ = ((v & systemRedMask  ) >> systemRedShift  ) << 3; // R
       *b++ = ((v & systemGreenMask) >> systemGreenShift) << 2; // G 
       *b++ = ((v & systemBlueMask ) >> systemBlueShift ) << 3; // B
     }
     p += surface->pitch / 2;
     png_write_row(png_ptr,writeBuffer);
     b = writeBuffer;
  }

  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);

  fclose(f);
  return 1;
}


static void CreateScreenshot(void)
{
	int w, h;

	if (current_screenshot != NULL)
	{
		SDL_FreeSurface(current_screenshot);
		current_screenshot = NULL;
	}

  if(prSDLScreen != NULL) {
  	w = prSDLScreen->w;
  	h = prSDLScreen->h;
  	current_screenshot = SDL_CreateRGBSurfaceFrom(prSDLScreen->pixels, w, h, prSDLScreen->format->BitsPerPixel,	prSDLScreen->pitch,
  		prSDLScreen->format->Rmask, prSDLScreen->format->Gmask, prSDLScreen->format->Bmask, prSDLScreen->format->Amask);
	}
}


static int save_thumb(char *path)
{
	int ret = 0;
	if (current_screenshot != NULL)
	{
		ret = save_png(current_screenshot, path);
		SDL_FreeSurface(current_screenshot);
		current_screenshot = NULL;
	}
	return ret;
}
//static int currVSyncRate = 0;
bool vsync_switchmode(int hz)
{
	int changed_height = changed_prefs.gfx_size.height;
	
	if (hz >= 55)
		hz = 60;
	else
		hz = 50;

	if (hz == 50 && currVSyncRate == 60)
	{
	  // Switch from NTSC -> PAL
		switch (changed_height) {
		case 200: changed_height = 240; break;
		case 216: changed_height = 262; break;
		case 240: changed_height = 270; break;
		case 256: changed_height = 270; break;
		case 262: changed_height = 270; break;
		case 270: changed_height = 270; break;
		}
	}
	else if (hz == 60 && currVSyncRate == 50)
	{
	  // Switch from PAL -> NTSC
		switch (changed_height) {
		case 200: changed_height = 200; break;
		case 216: changed_height = 200; break;
		case 240: changed_height = 200; break;
		case 256: changed_height = 216; break;
		case 262: changed_height = 216; break;
		case 270: changed_height = 240; break;
		}
	}

  if(hz != currVSyncRate) 
  {
    currVSyncRate = hz;
  	black_screen_now();
    fpscounter_reset();
    time_per_frame = 1000 * 1000 / (hz);

    if(hz == host_hz)
      vsync_modulo = 1;
    else if (hz > host_hz)
      vsync_modulo = 6; // Amiga draws 6 frames while host has 5 vsyncs -> sync every 6th Amiga frame
    else
      vsync_modulo = 5; // Amiga draws 5 frames while host has 6 vsyncs -> sync every 5th Amiga frame
  }
  
  if(!picasso_on && !picasso_requested_on)
  	changed_prefs.gfx_size.height = changed_height;

	return true;
}



bool target_graphics_buffer_update(void)
{
	bool rate_changed = false;
  
	if (currprefs.gfx_size.height != changed_prefs.gfx_size.height)
	{
		update_display(&changed_prefs);
		rate_changed = true;
	}

	if (rate_changed)
	{
		black_screen_now();
		fpscounter_reset();
		time_per_frame = 1000 * 1000 / (currprefs.chipset_refreshrate);
	}

	return true;
}

#ifdef PICASSO96
int picasso_palette (struct MyCLUTEntry *CLUT)
{
  int i, changed;

  changed = 0;
  for (i = 0; i < 256; i++) {
    int r = CLUT[i].Red;
    int g = CLUT[i].Green;
    int b = CLUT[i].Blue;
    int value = (r << 16 | g << 8 | b);
  	uae_u32 v = CONVERT_RGB(value);
	  if (v !=  picasso_vidinfo.clut[i]) {
	     picasso_vidinfo.clut[i] = v;
	     changed = 1;
	  } 
  }
  return changed;
}

static int resolution_compare (const void *a, const void *b)
{
  struct PicassoResolution *ma = (struct PicassoResolution *)a;
  struct PicassoResolution *mb = (struct PicassoResolution *)b;
  if (ma->res.width < mb->res.width)
  	return -1;
  if (ma->res.width > mb->res.width)
  	return 1;
  if (ma->res.height < mb->res.height)
  	return -1;
  if (ma->res.height > mb->res.height)
  	return 1;
  return ma->depth - mb->depth;
}
static void sortmodes (void)
{
  int	i = 0, idx = -1;
  int pw = -1, ph = -1;
  while (DisplayModes[i].depth >= 0)
  	i++;
  qsort (DisplayModes, i, sizeof (struct PicassoResolution), resolution_compare);
  for (i = 0; DisplayModes[i].depth >= 0; i++) {
  	if (DisplayModes[i].res.height != ph || DisplayModes[i].res.width != pw) {
	    ph = DisplayModes[i].res.height;
	    pw = DisplayModes[i].res.width;
	    idx++;
	  }
	  DisplayModes[i].residx = idx;
  }
}

static void modesList (void)
{
  int i, j;

  i = 0;
  while (DisplayModes[i].depth >= 0) {
  	write_log ("%d: %s (", i, DisplayModes[i].name);
  	j = 0;
  	while (DisplayModes[i].refresh[j] > 0) {
	    if (j > 0)
	    	write_log (",");
	    write_log ("%d", DisplayModes[i].refresh[j]);
	    j++;
	  }
	  write_log (")\n");
	  i++;
  }
}

void picasso_InitResolutions (void)
{
  struct MultiDisplay *md1;
  int i, count = 0;
  char tmp[200];
  int bit_idx;
  int bits[] = { 8, 16, 32 };
  
  Displays[0].primary = 1;
  Displays[0].disabled = 0;
  Displays[0].rect.left = 0;
  Displays[0].rect.top = 0;
  Displays[0].rect.right = 800;
  Displays[0].rect.bottom = 640;
  sprintf (tmp, "%s (%d*%d)", "Display", Displays[0].rect.right, Displays[0].rect.bottom);
  Displays[0].name = my_strdup(tmp);
  Displays[0].name2 = my_strdup("Display");

  md1 = Displays;
  DisplayModes = md1->DisplayModes = xmalloc (struct PicassoResolution, MAX_PICASSO_MODES);
  for (i = 0; i < MAX_SCREEN_MODES && count < MAX_PICASSO_MODES; i++) {
    for(bit_idx = 0; bit_idx < 3; ++bit_idx) {
      int bitdepth = bits[bit_idx];
      int bit_unit = (bitdepth + 1) & 0xF8;
      int rgbFormat = (bitdepth == 8 ? RGBFB_CLUT : (bitdepth == 16 ? RGBFB_R5G6B5 : RGBFB_R8G8B8A8));
      int pixelFormat = 1 << rgbFormat;
  	  pixelFormat |= RGBFF_CHUNKY;
      
  	  //if (SDL_VideoModeOK (x_size_table[i], y_size_table[i], 16, SDL_SWSURFACE))
  	  {
  	    DisplayModes[count].res.width = x_size_table[i];
  	    DisplayModes[count].res.height = y_size_table[i];
  	    DisplayModes[count].depth = bit_unit >> 3;
        DisplayModes[count].refresh[0] = 50;
        DisplayModes[count].refresh[1] = 60;
        DisplayModes[count].refresh[2] = 0;
        DisplayModes[count].colormodes = pixelFormat;
        sprintf(DisplayModes[count].name, "%dx%d, %d-bit",
  	      DisplayModes[count].res.width, DisplayModes[count].res.height, DisplayModes[count].depth * 8);
  
  	    count++;
      }
    }
  }
  DisplayModes[count].depth = -1;
  sortmodes();
  modesList();
  DisplayModes = Displays[0].DisplayModes;
}


void gfx_set_picasso_state (int on)
{
	if (on == screen_is_picasso)
		return;

	screen_is_picasso = on;
  open_screen(&currprefs);
  if(prSDLScreen != NULL)
    picasso_vidinfo.rowbytes	= prSDLScreen->pitch;
}

void gfx_set_picasso_modeinfo (uae_u32 w, uae_u32 h, uae_u32 depth, RGBFTYPE rgbfmt)
{
  depth >>= 3;
  if( ((unsigned)picasso_vidinfo.width == w ) &&
    ( (unsigned)picasso_vidinfo.height == h ) &&
    ( (unsigned)picasso_vidinfo.depth == depth ) &&
    ( picasso_vidinfo.selected_rgbformat == rgbfmt) )
  	return;

  picasso_vidinfo.selected_rgbformat = rgbfmt;
  picasso_vidinfo.width = w;
  picasso_vidinfo.height = h;
  picasso_vidinfo.depth = depth;
  picasso_vidinfo.extra_mem = 1;

  picasso_vidinfo.pixbytes = depth;
  if (screen_is_picasso)
  {
  	open_screen(&currprefs);
    if(prSDLScreen != NULL)
        picasso_vidinfo.rowbytes	= prSDLScreen->pitch;
    picasso_vidinfo.rgbformat = RGBFB_R5G6B5;
  }
}

uae_u8 *gfx_lock_picasso (void)
{
  if(prSDLScreen == NULL || screen_is_picasso == 0)
    return NULL;
  //SDL_LockSurface(prSDLScreen);
  picasso_vidinfo.rowbytes = prSDLScreen->pitch;
  return (uae_u8 *)prSDLScreen->pixels;
}


void gfx_unlock_picasso(bool dorender)
{
  // We lock the surface directly after create and flip, so no unlock here
  //SDL_UnlockSurface(prSDLScreen);
  if(dorender)
  {
    render_screen(true);
    show_screen(0);
  }
}

#endif // PICASSO96
