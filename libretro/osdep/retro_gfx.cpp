#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "gui.h"
#include "xwin.h"
#include "custom.h"
#include "drawing.h"
#include "events.h"
#include "od-retro/inputmode.h"
#include "savestate.h"
#include "picasso96.h"

#ifdef ANDROIDSDL
#include <android/log.h>
#endif
#include "td-sdl/thread.h"

extern int retrow,retroh,stylusClickOverride;

#include "libretro-core.h"

/* Possible screen modes (x and y resolutions) */
#define MAX_SCREEN_MODES 7
static int x_size_table[MAX_SCREEN_MODES] = { 640, 640, 800, 800,1024, 1152, 1280 };
static int y_size_table[MAX_SCREEN_MODES] = { 400, 480, 480, 600, 768,  864,  960 };

static int red_bits, green_bits, blue_bits;
static int red_shift, green_shift, blue_shift;

struct PicassoResolution *DisplayModes;
struct MultiDisplay Displays[MAX_DISPLAYS];

int screen_is_picasso = 0;

static int curr_layer_width = 0;

static char screenshot_filename_default[255]={
	'/', 't', 'm', 'p', '/', 'n', 'u', 'l', 'l', '.', 'p', 'n', 'g', '\0'
};
char *screenshot_filename=(char *)&screenshot_filename_default[0];
FILE *screenshot_file=NULL;
static void CreateScreenshot(void);
static int save_thumb(char *path);
int delay_savestate_frame = 0;

int justClicked = 0;
int mouseMoving = 0;
int fcounter = 0;
int doStylusRightClick = 0;

int DispManXElementpresent = 0;

static unsigned long previous_synctime = 0;
static unsigned long next_synctime = 0;


uae_sem_t vsync_wait_sem;

/*
DISPMANX_DISPLAY_HANDLE_T   dispmanxdisplay;
DISPMANX_MODEINFO_T         dispmanxdinfo;
DISPMANX_RESOURCE_HANDLE_T  dispmanxresource_amigafb_1;
DISPMANX_RESOURCE_HANDLE_T  dispmanxresource_amigafb_2;
DISPMANX_ELEMENT_HANDLE_T   dispmanxelement;
DISPMANX_UPDATE_HANDLE_T    dispmanxupdate;
VC_RECT_T       src_rect;
VC_RECT_T       dst_rect;
VC_RECT_T       blit_rect;
*/
unsigned char current_resource_amigafb = 0;

#include "libco.h"

extern cothread_t mainThread;
extern cothread_t emuThread;

void vsync_callback(unsigned int a, void* b)
{
LOGI("vsync\n");
//co_switch(mainThread);
	//vsync_timing=SDL_GetTicks();
	//vsync_frequency = vsync_timing - old_time;
	//old_time = vsync_timing;
	//need_frameskip =  ( vsync_frequency > 31 ) ? (need_frameskip+1) : need_frameskip;
	//printf("d: %i", vsync_frequency     );
	uae_sem_post (&vsync_wait_sem);
}


int graphics_setup (void)
{
#ifdef PICASSO96
  picasso_InitResolutions();
  InitPicasso96();
#endif
 // bcm_host_init();
  return 1;
}


void InitAmigaVidMode(struct uae_prefs *p)
{
  LOGI("retro:(%d,%d) gfx(%d,%d) res %i\n",retrow,retroh,p->gfx_size.width,p->gfx_size.height,p->gfx_resolution);

  /* Initialize structure for Amiga video modes */
  gfxvidinfo.pixbytes = 2;
  gfxvidinfo.bufmem = (uae_u8 *)Retro_Screen;
  gfxvidinfo.linemem = 0;
  gfxvidinfo.emergmem = 0;
  gfxvidinfo.width = p->gfx_size.width;
  gfxvidinfo.height = p->gfx_size.height;
#ifdef PICASSO96
  if(screen_is_picasso)
  {
    gfxvidinfo.width  = picasso_vidinfo.width;
    //gfxvidinfo.height = picasso_vidinfo.height;
  }
#endif
  gfxvidinfo.maxblocklines = 0;
  //gfxvidinfo.rowbytes = prSDLScreen->pitch;
  gfxvidinfo.rowbytes =  retrow* 2;
}

void graphics_dispmanshutdown (void)
{

}


void graphics_subshutdown (void)
{
  // Dunno if below lines are usefull for Rpi...
  //SDL_FreeSurface(prSDLScreen);
  //prSDLScreen = NULL;
}



static void open_screen(struct uae_prefs *p)
{
  char layersize[20];

  uint32_t                    vc_image_ptr;
  int width;
  int height;
#ifdef PICASSO96
  if (screen_is_picasso)
  {
    width  = picasso_vidinfo.width;
    height = picasso_vidinfo.height;
  } else
#endif
  {
p->gfx_resolution = p->gfx_size.width > 600 ? 1 : 0;
    width  = p->gfx_size.width;
    height = p->gfx_size.height;
  }

  // if(prSDLScreen != NULL)
  {
    InitAmigaVidMode(p);
    init_row_map();
  }    
  //framecnt = 1; // Don't draw frame before reset done
}


void update_display(struct uae_prefs *p)
{
  open_screen(p);
  framecnt = 1; // Don't draw frame before reset done
}


int check_prefs_changed_gfx (void)
{
  int changed = 0;
  if(currprefs.gfx_size.height != changed_prefs.gfx_size.height ||
     currprefs.gfx_size.width != changed_prefs.gfx_size.width ||
     currprefs.gfx_size_fs.width != changed_prefs.gfx_size_fs.width ||
     currprefs.gfx_resolution != changed_prefs.gfx_resolution)
  {
  	cfgfile_configuration_change(1);
    currprefs.gfx_size.height = changed_prefs.gfx_size.height;
    currprefs.gfx_size.width = changed_prefs.gfx_size.width;
    currprefs.gfx_size_fs.width = changed_prefs.gfx_size_fs.width;
    currprefs.gfx_resolution = changed_prefs.gfx_resolution;
    update_display(&currprefs);
    changed = 1;
  }
  if (currprefs.leds_on_screen != changed_prefs.leds_on_screen ||
      currprefs.pandora_vertical_offset != changed_prefs.pandora_vertical_offset)	
  {
    currprefs.leds_on_screen = changed_prefs.leds_on_screen;
    currprefs.pandora_vertical_offset = changed_prefs.pandora_vertical_offset;
    changed = 1;
  }
  if (currprefs.chipset_refreshrate != changed_prefs.chipset_refreshrate) 
  {
  	currprefs.chipset_refreshrate = changed_prefs.chipset_refreshrate;
	  init_hz ();
	  changed = 1;
  }

  return changed;
}


int lockscr (void)
{
    //SDL_LockSurface(prSDLScreen);
    return 1;
}


void unlockscr (void)
{
    //SDL_UnlockSurface(prSDLScreen);
}
#include "inputdevice.h"
#include "joystick.h"
extern int Retro_PollEvent();
#define getjoystate(NR,DIR,BUT) read_joystick(NR,DIR,BUT)
void flush_screen ()
{
    //SDL_UnlockSurface (prSDLScreen);

    if (show_inputmode)
    {
        inputmode_redraw();	
    }


    if (savestate_state == STATE_DOSAVE)
    {
    if(delay_savestate_frame > 0)
      --delay_savestate_frame;
    else
    {
/*
        CreateScreenshot();
        save_thumb(screenshot_filename);
        savestate_state = 0;
*/
    }
  }

  unsigned long start = read_processor_time();

co_switch(mainThread);

  last_synctime = read_processor_time();
  
  if(last_synctime - next_synctime > time_per_frame - 1000)
    adjust_idletime(0);
  else
    adjust_idletime(next_synctime - start);
  
  if(last_synctime - next_synctime > time_per_frame - 5000)
    next_synctime = last_synctime + time_per_frame * (1 + currprefs.gfx_framerate);
  else
    next_synctime = next_synctime + time_per_frame * (1 + currprefs.gfx_framerate);

	init_row_map();


}


void black_screen_now(void)
{
memset(Retro_Screen,0,retrow*retroh*PITCH);
//	SDL_FillRect(Dummy_prSDLScreen,NULL,0);
//	SDL_Flip(Dummy_prSDLScreen);
}


static void graphics_subinit (void)
{
	if (0/*prSDLScreen == NULL*/)
	{
		fprintf(stderr, "Unable to set video mode: %s\n", "fuck");
		return;
	}
	else
	{
	//	SDL_ShowCursor(SDL_DISABLE);

    InitAmigaVidMode(&currprefs);
	}
}

STATIC_INLINE int bitsInMask (unsigned long mask)
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


STATIC_INLINE int maskShift (unsigned long mask)
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


static int init_colors (void)
{
	int i;
	int red_bits, green_bits, blue_bits;
	int red_shift, green_shift, blue_shift;

	/* Truecolor: */
	red_bits =  5;//bitsInMask(prSDLScreen->format->Rmask);
	green_bits =6;// bitsInMask(prSDLScreen->format->Gmask);
	blue_bits = 5;//bitsInMask(prSDLScreen->format->Bmask);
	red_shift = 11;//maskShift(prSDLScreen->format->Rmask);
	green_shift = 5;//maskShift(prSDLScreen->format->Gmask);
	blue_shift = 0;//maskShift(prSDLScreen->format->Bmask);
	alloc_colors64k (red_bits, green_bits, blue_bits, red_shift, green_shift, blue_shift, 0);
	notice_new_xcolors();
	for (i = 0; i < 4096; i++)
		xcolors[i] = xcolors[i] * 0x00010001;

	return 1;
}


/*
 * Find the colour depth of the display
 */
static int get_display_depth (void)
{  
return 16;
#if 0
 // const SDL_VideoInfo *vid_info;
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
#endif
}

int GetSurfacePixelFormat(void)
{
  int depth = 16;//get_display_depth();
  int unit = (depth + 1) & 0xF8;

  return (unit == 8 ? RGBFB_CHUNKY
		: depth == 15 && unit == 16 ? RGBFB_R5G5B5
		: depth == 16 && unit == 16 ? RGBFB_R5G6B5
		: unit == 24 ? RGBFB_B8G8R8
		: unit == 32 ? RGBFB_R8G8B8A8
		: RGBFB_NONE);
}


int graphics_init (void)
{
	int i,j;

	//uae_sem_init (&vsync_wait_sem, 0, 1);

	// In case we have load a .uae file, we need to realign retro & gfx resolution...
	retrow = currprefs.gfx_size.width;
	retroh = currprefs.gfx_size.height;

	graphics_subinit ();


	if (!init_colors ())
		return 0;

	buttonstate[0] = buttonstate[1] = buttonstate[2] = 0;
	keyboard_init();
  
	return 1;
}

void graphics_leave (void)
{
	graphics_subshutdown ();
	//SDL_FreeSurface(Dummy_prSDLScreen);
	//bcm_host_deinit();
	//SDL_VideoQuit();
}

#if 0
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

#endif

static void CreateScreenshot(void)
{
#if 0
	int w, h;

  if(current_screenshot != NULL)
  {
    SDL_FreeSurface(current_screenshot);
    current_screenshot = NULL;
  }

	w=prSDLScreen->w;
	h=prSDLScreen->h;

	current_screenshot = SDL_CreateRGBSurface(prSDLScreen->flags,w,h,prSDLScreen->format->BitsPerPixel,prSDLScreen->format->Rmask,prSDLScreen->format->Gmask,prSDLScreen->format->Bmask,prSDLScreen->format->Amask);
  SDL_BlitSurface(prSDLScreen, NULL, current_screenshot, NULL);
#endif
}


static int save_thumb(char *path)
{
	int ret = 0;
#if 0
	if(current_screenshot != NULL)
	{
	  ret = save_png(current_screenshot, path);
	  SDL_FreeSurface(current_screenshot);
	  current_screenshot = NULL;
	}
	return ret;
#endif
}


#ifdef PICASSO96


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
  int bitdepth;
  
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
  DisplayModes = md1->DisplayModes = (struct PicassoResolution*) xmalloc (sizeof (struct PicassoResolution) * MAX_PICASSO_MODES);
  for (i = 0; i < MAX_SCREEN_MODES && count < MAX_PICASSO_MODES; i++) {
    for(bitdepth = 16; bitdepth <= 32; bitdepth += 16) {
      int bit_unit = (bitdepth + 1) & 0xF8;
      int rgbFormat = (bitdepth == 16 ? RGBFB_R5G6B5 : RGBFB_R8G8B8A8);
      int pixelFormat = 1 << rgbFormat;
  	  pixelFormat |= RGBFF_CHUNKY;
      
  	  if (bitdepth==16/*SDL_VideoModeOK (x_size_table[i], y_size_table[i], bitdepth, SDL_SWSURFACE)*/)
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
    picasso_vidinfo.rowbytes	= retrow*PITCH;//prSDLScreen->pitch;
  }
}

void gfx_set_picasso_state (int on)
{
	if (on == screen_is_picasso)
		return;

	screen_is_picasso = on;
  open_screen(&currprefs);
  picasso_vidinfo.rowbytes	= retrow*PITCH;//prSDLScreen->pitch;
}

uae_u8 *gfx_lock_picasso (void)
{
  // We lock the surface directly after create and flip
  picasso_vidinfo.rowbytes = PITCH*retrow;//prSDLScreen->pitch;
  return (uae_u8 *)Retro_Screen;//prSDLScreen->pixels;
}

void gfx_unlock_picasso (void)
{
  // We lock the surface directly after create and flip, so no unlock here
}

#endif // PICASSO96
