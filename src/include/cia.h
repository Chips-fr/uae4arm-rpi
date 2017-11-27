 /*
  * UAE - The Un*x Amiga Emulator
  *
  * CIA chip support
  *
  * (c) 1995 Bernd Schmidt
  */

#ifndef UAE_CIA_H
#define UAE_CIA_H

#include "uae/types.h"

extern void CIA_reset (void);
extern void CIA_vsync_prehandler (void);
extern void CIA_hsync_posthandler (void);
extern void CIA_vsync_posthandler (void);
extern void CIA_handler (void);

extern void diskindex_handler (void);
extern void cia_diskindex (void);

extern void rethink_cias (void);
extern void cia_set_overlay (bool);

extern void rtc_hardreset(void);

#endif /* UAE_CIA_H */
