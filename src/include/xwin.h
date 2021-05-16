 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Interface to the graphics system (X, SVGAlib)
  *
  * Copyright 1995-1997 Bernd Schmidt
  */

#ifndef UAE_XWIN_H
#define UAE_XWIN_H

#include "uae/types.h"
#include "md-pandora/rpt.h"

typedef uae_u32 xcolnr;

typedef int (*allocfunc_type)(int, int, int, xcolnr *);

extern xcolnr xcolors[4096];

extern int graphics_setup (void);
extern int graphics_init (bool);
extern void graphics_leave (void);

extern void gui_handle_events (void);
STATIC_INLINE void handle_events (void)
{
  // Still needed for keyboard reset. Need to move this in keyboard management (events RESET do exist).
  gui_handle_events ();
}
extern int handle_msgpump (void);

extern bool vsync_switchmode (int);
STATIC_INLINE int isvsync_chipset (void)
{
	if (picasso_on || currprefs.gfx_apmode[0].gfx_vsync <= 0)
		return 0;
	return 1;
}

STATIC_INLINE int isvsync_rtg (void)
{
	if (!picasso_on || currprefs.gfx_apmode[1].gfx_vsync <= 0)
		return 0;
	return 1;
}

STATIC_INLINE int isvsync (void)
{
	if (picasso_on)
		return isvsync_rtg ();
	else
		return isvsync_chipset ();
}

extern bool render_screen (bool);
extern void show_screen (int);
extern bool show_screen_maybe (bool);

extern int lockscr (void);
extern void unlockscr (void);
extern bool target_graphics_buffer_update (void);

extern void screenshot (int);

extern int bits_in_mask (uae_u32 mask);
extern int mask_shift (uae_u32 mask);
extern unsigned int doMask (int p, int bits, int shift);
extern unsigned int doMask256 (int p, int bits, int shift);
extern void alloc_colors64k (int, int, int, int, int, int, int);
extern void alloc_colors_picasso (int rw, int gw, int bw, int rs, int gs, int bs, int rgbfmt);

struct vidbuffer
{
  uae_u8 *bufmem;
  int rowbytes; /* Bytes per row in the memory pointed at by bufmem. */
  int pixbytes; /* Bytes per pixel. */
	/* size of max visible image */
  int outwidth;
  int outheight;
};

extern int max_uae_width, max_uae_height;

struct vidbuf_description
{
  struct vidbuffer drawbuffer;
};

extern struct vidbuf_description gfxvidinfo;

#endif /* UAE_XWIN_H */
