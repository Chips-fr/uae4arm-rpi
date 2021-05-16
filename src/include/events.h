 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Events
  * These are best for low-frequency events. Having too many of them,
  * or using them for events that occur too frequently, can cause massive
  * slowdown.
  *
  * Copyright 1995-1998 Bernd Schmidt
  */

#ifndef UAE_EVENTS_H
#define UAE_EVENTS_H

#include "uae/types.h"

#include "md-pandora/rpt.h"

extern frame_time_t vsyncmintime, vsyncmaxtime, vsyncwaittime;
extern int vsynctimebase, syncbase;
extern void reset_frame_rate_hack (void);
extern int speedup_timelimit;

extern void compute_vsynctime (void);
extern void init_eventtab (void);
extern void events_schedule (void);

extern uae_u32 currcycle, nextevent;
extern int is_syncline;
typedef void (*evfunc)(void);
typedef void (*evfunc2)(uae_u32);

typedef void (*do_cycles_func)(uae_u32);
extern do_cycles_func do_cycles;
void do_cycles_cpu_fastest (uae_u32 cycles_to_add);
void do_cycles_cpu_norm (uae_u32 cycles_to_add);

typedef uae_u32 evt;

struct ev
{
    bool active;
    evt evtime, oldcycles;
    evfunc handler;
};

struct ev2
{
    bool active;
    evt evtime;
    uae_u32 data;
    evfunc2 handler;
};

enum {
    ev_copper, 
    ev_cia, ev_audio, ev_blitter, ev_dmal, ev_misc, ev_hsync,
    ev_max
};

enum {
    ev2_disk, ev2_ciaa_tod, ev2_ciab_tod, ev2_disk_motor0, ev2_disk_motor1, ev2_disk_motor2, ev2_disk_motor3,
    ev2_max
};

extern int pissoff_value;
#define countdown (regs.pissoff)

extern struct ev eventtab[ev_max];
extern struct ev2 eventtab2[ev2_max];

STATIC_INLINE void cycles_do_special (void)
{
#ifdef JIT
	if (currprefs.cachesize) {
		if (regs.pissoff >= 0)
			regs.pissoff = -1;
	} else 
#endif
  {
		regs.pissoff = 0;
	}
}

STATIC_INLINE void do_extra_cycles (uae_u32 cycles_to_add)
{
	regs.pissoff -= cycles_to_add;
}

STATIC_INLINE uae_u32 get_cycles (void)
{
  return currcycle;
}

STATIC_INLINE void set_cycles (uae_u32 x)
{
  currcycle = x;
	eventtab[ev_hsync].oldcycles = x;
}

STATIC_INLINE int current_hpos (void)
{
  int hp = (get_cycles () - eventtab[ev_hsync].oldcycles) / CYCLE_UNIT;
	return hp;
}

STATIC_INLINE bool cycles_in_range (uae_u32 endcycles)
{
	uae_s32 c = get_cycles ();
	return (uae_s32)endcycles - c > 0;
}

extern void MISC_handler(void);

STATIC_INLINE void event2_newevent (int no, evt t, uae_u32 data)
{
	eventtab2[no].active = true;
  eventtab2[no].evtime = (t * CYCLE_UNIT) + get_cycles();
  eventtab2[no].data = data;
  MISC_handler();
}

STATIC_INLINE void event2_remevent (int no)
{
	eventtab2[no].active = 0;
}

STATIC_INLINE void event_newevent (int no, evt t)
{
  evt ct = get_cycles();
	eventtab[no].active = true;
  eventtab[no].evtime = ct + t * CYCLE_UNIT;
  events_schedule();
}

STATIC_INLINE void event_remevent (int no)
{
	eventtab[no].active = 0;
}

#endif /* UAE_EVENTS_H */
