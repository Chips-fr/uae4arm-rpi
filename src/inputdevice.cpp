 /*
* UAE - The Un*x Amiga Emulator
*
* joystick/mouse emulation
*
* Copyright 2001-2016 Toni Wilen
*
* new fetures:
* - very configurable (and very complex to configure :)
* - supports multiple native input devices (joysticks and mice)
* - supports mapping joystick/mouse buttons to keys and vice versa
* - joystick mouse emulation (supports both ports)
* - supports parallel port joystick adapter
* - full cd32 pad support (supports both ports)
* - fully backward compatible with old joystick/mouse configuration
*
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "keybuf.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "memory.h"
#include "newcpu.h"
#include "uae.h"
#include "picasso96.h"
#include "ar.h"
#include "gui.h"
#include "savestate.h"
#include "zfile.h"
#include "cia.h"
#include "autoconf.h"
#include "cdtv.h"
#include "statusline.h"
#include "native2amiga_api.h"

#if SIZEOF_TCHAR != 1
/* FIXME: replace strcasecmp with _tcsicmp in source code instead */
#undef strcasecmp
#define strcasecmp _tcsicmp
#endif

// 01 = host events
// 02 = joystick
// 04 = cia buttons
// 16 = potgo
// 32 = vsync

#define COMPA_RESERVED_FLAGS ID_FLAG_INVERT

#define ID_FLAG_CANRELEASE 0x1000
#define ID_FLAG_TOGGLED 0x2000
#define ID_FLAG_CUSTOMEVENT_TOGGLED1 0x4000
#define ID_FLAG_CUSTOMEVENT_TOGGLED2 0x8000

#define IE_INVERT 0x80
#define IE_CDTV 0x100

#define INPUTEVENT_JOY1_CD32_FIRST INPUTEVENT_JOY1_CD32_PLAY
#define INPUTEVENT_JOY2_CD32_FIRST INPUTEVENT_JOY2_CD32_PLAY
#define INPUTEVENT_JOY1_CD32_LAST INPUTEVENT_JOY1_CD32_BLUE
#define INPUTEVENT_JOY2_CD32_LAST INPUTEVENT_JOY2_CD32_BLUE

#define JOYMOUSE_CDTV 8

#define DEFEVENT(A, B, C, D, E, F) {_T(#A), B, NULL, C, D, E, F, 0 },
#define DEFEVENT2(A, B, B2, C, D, E, F, G) {_T(#A), B, B2, C, D, E, F, G },
static const struct inputevent events[] = {
	{0, 0, 0, AM_K, 0, 0, 0, 0},
#include "inputevents.def"
	{0, 0, 0, 0, 0, 0, 0, 0}
};
#undef DEFEVENT
#undef DEFEVENT2

static int sublevdir[2][MAX_INPUT_SUB_EVENT];

static const int slotorder1[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static const int slotorder2[] = { 8, 1, 2, 3, 4, 5, 6, 7 };

struct uae_input_device2 {
	uae_u32 buttonmask;
	int states[MAX_INPUT_DEVICE_EVENTS / 2][MAX_INPUT_SUB_EVENT + 1];
};

static struct uae_input_device2 joysticks2[MAX_INPUT_DEVICES];
static struct uae_input_device2 mice2[MAX_INPUT_DEVICES];
static uae_u8 scancodeused[MAX_INPUT_DEVICES][256];
static uae_u64 qualifiers, qualifiers_r;
static uae_s16 *qualifiers_evt[MAX_INPUT_QUALIFIERS];

static int joymodes[MAX_JPORTS];
static const int *joyinputs[MAX_JPORTS];

static int input_acquired;

static int handle_input_event (int nr, int state, int max, int autofire);

static struct inputdevice_functions idev[IDTYPE_MAX];

struct temp_uids {
	TCHAR *name;
	TCHAR *configname;
	uae_s8 disabled;
	uae_s8 empty;
	uae_s8 custom;
	int joystick;
	int devtype;
	int idnum;
	bool initialized;
	int kbreventcnt[MAX_INPUT_DEVICES];
	int lastdevtype;
	int matcheddevices[MAX_INPUT_DEVICES];
	int nonmatcheddevices[MAX_INPUT_DEVICES];
};
static struct temp_uids temp_uid;
static int temp_uid_index[MAX_INPUT_DEVICES][IDTYPE_MAX];
static int temp_uid_cnt[IDTYPE_MAX];

static int isdevice (struct uae_input_device *id)
{
	int i, j;
	for (i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (id->eventid[i][j] > 0)
				return 1;
		}
	}
	return 0;
}

static void check_enable(int ei);

int inputdevice_uaelib (const TCHAR *s, const TCHAR *parm)
{
	int i;

	if (!_tcsncmp(s, _T("KEY_RAW_"), 8)) {
		// KEY_RAW_UP <code>
		// KEY_RAW_DOWN <code>
		int v;
		const TCHAR *value = parm;
		TCHAR *endptr;
		int base = 10;
		int state = _tcscmp(s, _T("KEY_RAW_UP")) ? 1 : 0;
		if (value[0] == '0' && _totupper(value[1]) == 'X')
			value += 2, base = 16;
		v = _tcstol(value, &endptr, base);
		for (i = 1; events[i].name; i++) {
			const struct inputevent *ie = &events[i];
			if (_tcsncmp(ie->confname, _T("KEY_"), 4))
				continue;
			if (ie->data == v) {
				handle_input_event(i, state, 1, 0);
				return 1;
			}
		}
		return 0;
	}

	for (i = 1; events[i].name; i++) {
		if (!_tcscmp (s, events[i].confname)) {
			check_enable(i);
			handle_input_event (i, parm ? _tstol (parm) : 0, 1, 0);
			return 1;
		}
	}
	return 0;
}

int inputdevice_uaelib(const TCHAR *s, int parm, int max, bool autofire)
{
	for (int i = 1; events[i].name; i++) {
		if (!_tcscmp(s, events[i].confname)) {
			check_enable(i);
			handle_input_event(i, parm, max, autofire);
			return 1;
		}
	}
	return 0;
}

static int getdevnum(int type, int devnum)
{
	int jcnt = idev[IDTYPE_JOYSTICK].get_num();
	int mcnt = idev[IDTYPE_MOUSE].get_num();
	int kcnt = idev[IDTYPE_KEYBOARD].get_num();

	if (type == IDTYPE_JOYSTICK)
		return devnum;
	else if (type == IDTYPE_MOUSE)
		return jcnt + devnum;
	else if (type == IDTYPE_KEYBOARD)
		return jcnt + mcnt + devnum;
	return -1;
}

static int gettype (int devnum)
{
	int jcnt = idev[IDTYPE_JOYSTICK].get_num ();
	int mcnt = idev[IDTYPE_MOUSE].get_num ();
	int kcnt = idev[IDTYPE_KEYBOARD].get_num ();

	if (devnum < jcnt)
		return IDTYPE_JOYSTICK;
	else if (devnum < jcnt + mcnt)
		return IDTYPE_MOUSE;
	else if (devnum < jcnt + mcnt + kcnt)
		return IDTYPE_KEYBOARD;
	return -1;
}

static struct inputdevice_functions *getidf (int devnum)
{
	int type = gettype (devnum);
	if (type < 0)
		return NULL;
	return &idev[type];
}

const struct inputevent *inputdevice_get_eventinfo(int evt)
{
	if (evt > 0 && !events[evt].name)
		return NULL;
	return &events[evt];
}

static struct uae_input_device *joysticks;
static struct uae_input_device *mice;
static struct uae_input_device *keyboards;
static struct uae_input_device_kbr_default *keyboard_default, **keyboard_default_table;
static int default_keyboard_layout[MAX_JPORTS];

#define KBR_DEFAULT_MAP_FIRST 0
#define KBR_DEFAULT_MAP_LAST 5
#define KBR_DEFAULT_MAP_CD32_FIRST 6
#define KBR_DEFAULT_MAP_CD32_LAST 8

#define KBR_DEFAULT_MAP_NP 0
#define KBR_DEFAULT_MAP_CK 1
#define KBR_DEFAULT_MAP_SE 2
#define KBR_DEFAULT_MAP_NP3 3
#define KBR_DEFAULT_MAP_CK3 4
#define KBR_DEFAULT_MAP_SE3 5
#define KBR_DEFAULT_MAP_CD32_NP 6
#define KBR_DEFAULT_MAP_CD32_CK 7
#define KBR_DEFAULT_MAP_CD32_SE 8
#define KBR_DEFAULT_MAP_CDTV 10
static int **keyboard_default_kbmaps;

static int mouse_axis[MAX_INPUT_DEVICES][MAX_INPUT_DEVICE_EVENTS];
static int oldm_axis[MAX_INPUT_DEVICES][MAX_INPUT_DEVICE_EVENTS];

#define MOUSE_AXIS_TOTAL 4

static uae_s16 mouse_x[MAX_JPORTS], mouse_y[MAX_JPORTS];
static uae_s16 mouse_delta[MAX_JPORTS][MOUSE_AXIS_TOTAL];
static uae_s16 mouse_deltanoreset[MAX_JPORTS][MOUSE_AXIS_TOTAL];
static int joybutton[MAX_JPORTS];
static int joydir[MAX_JPORTS];
#ifndef INPUTDEVICE_SIMPLE
static int joydirpot[MAX_JPORTS][2];
#endif
static uae_s16 mouse_frame_x[MAX_JPORTS], mouse_frame_y[MAX_JPORTS];

static int mouse_port[NORMAL_JPORTS];
static int cd32_shifter[NORMAL_JPORTS];
static int cd32_pad_enabled[NORMAL_JPORTS];
#ifndef INPUTDEVICE_SIMPLE
static int parport_joystick_enabled;
#endif
static int oleft[MAX_JPORTS], oright[MAX_JPORTS], otop[MAX_JPORTS], obot[MAX_JPORTS];
static int relativecount[MAX_JPORTS][2];

uae_u16 potgo_value;
#ifndef INPUTDEVICE_SIMPLE
static int pot_cap[NORMAL_JPORTS][2];
#endif
static uae_u8 pot_dat[NORMAL_JPORTS][2];
static int pot_dat_act[NORMAL_JPORTS][2];
#ifndef INPUTDEVICE_SIMPLE
static int analog_port[NORMAL_JPORTS][2];
#endif
static int digital_port[NORMAL_JPORTS][2];
#define POTDAT_DELAY_PAL 8
#define POTDAT_DELAY_NTSC 7

static int use_joysticks[MAX_INPUT_DEVICES];
static int use_mice[MAX_INPUT_DEVICES];
static int use_keyboards[MAX_INPUT_DEVICES];

#ifdef INPUTDEVICE_SIMPLE
#define INPUT_QUEUE_SIZE 6
#else
#define INPUT_QUEUE_SIZE 16
#endif
struct input_queue_struct {
	int evt, storedstate, state, max, linecnt, nextlinecnt;
};
static struct input_queue_struct input_queue[INPUT_QUEUE_SIZE];

uae_u8 *restore_input (uae_u8 *src)
{
	restore_u32 ();
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
#ifndef INPUTDEVICE_SIMPLE
			pot_cap[i][j] = restore_u16 ();
#else
      restore_u16();
#endif
		}
	}
	return src;
}
uae_u8 *save_input (int *len, uae_u8 *dstptr)
{
	uae_u8 *dstbak, *dst;

	if (dstptr)
		dstbak = dst = dstptr;
	else
		dstbak = dst = xmalloc (uae_u8, 1000);
	save_u32 (0);
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
#ifndef INPUTDEVICE_SIMPLE
			save_u16 (pot_cap[i][j]);
#else
      save_u16(0);
#endif
		}
	}
	*len = dst - dstbak;
	return dstbak;
}

static void freejport (struct uae_prefs *dst, int num)
{
	bool override = dst->jports[num].nokeyboardoverride;
	memset (&dst->jports[num], 0, sizeof (struct jport));
	dst->jports[num].id = -1;
	dst->jports[num].nokeyboardoverride = override;
}
static void copyjport (const struct uae_prefs *src, struct uae_prefs *dst, int num)
{
	if (!src)
		return;
	freejport (dst, num);
	memcpy(&dst->jports[num].idc, &src->jports[num].idc, sizeof(struct inputdevconfig));
	dst->jports[num].id = src->jports[num].id;
	dst->jports[num].mode = src->jports[num].mode;
	dst->jports[num].autofire = src->jports[num].autofire;
	dst->jports[num].nokeyboardoverride = src->jports[num].nokeyboardoverride;
}

#define MAX_STORED_JPORTS 8
struct stored_jport
{
	struct jport jp;
	bool inuse;
	bool defaultports;
	uae_u32 age;
};
static struct stored_jport stored_jports[MAX_JPORTS][8];
static uae_u32 stored_jport_cnt;

// return port where idc was previously plugged if any and forgot it.
static int inputdevice_get_unplugged_device(struct inputdevconfig *idc)
{
	for (int portnum = 0; portnum < MAX_JPORTS; portnum++) {
		for (int i = 0; i < MAX_STORED_JPORTS; i++) {
			struct stored_jport *jp = &stored_jports[portnum][i];
			if (jp->inuse && jp->jp.id == JPORT_UNPLUGGED) {
				if (!_tcscmp(idc->name, jp->jp.idc.name) && !_tcscmp(idc->configname, jp->jp.idc.configname)) {
					jp->inuse = false;
					return portnum;
				}
			}
		}
	}
	return -1;
}

// forget port's unplugged device
void inputdevice_forget_unplugged_device(int portnum)
{
	for (int i = 0; i < MAX_STORED_JPORTS; i++) {
		struct stored_jport *jp = &stored_jports[portnum][i];
		if (jp->inuse && jp->jp.id == JPORT_UNPLUGGED) {
			jp->inuse = false;
		}
	}
}

static struct jport *inputdevice_get_used_device(int portnum, int ageindex)
{
	int idx = -1;
	int used[MAX_STORED_JPORTS] = { 0 };
	if (ageindex < 0)
		return NULL;
	while (ageindex >= 0) {
		uae_u32 age = 0;
		idx = -1;
		for (int i = 0; i < MAX_STORED_JPORTS; i++) {
			struct stored_jport *jp = &stored_jports[portnum][i];
			if (jp->inuse && !used[i] && jp->age > age) {
				age = jp->age;
				idx = i;
			}
		}
		if (idx < 0)
			return NULL;
		used[idx] = 1;
		ageindex--;
	}
	return &stored_jports[portnum][idx].jp;
}

static void inputdevice_store_clear(void)
{
	for (int j = 0; j < MAX_JPORTS; j++) {
		for (int i = 0; i < MAX_STORED_JPORTS; i++) {
			struct stored_jport *jp = &stored_jports[j][i];
			if (!jp->defaultports)
				memset(jp, 0, sizeof(struct stored_jport));
		}
	}
}

static void inputdevice_set_newest_used_device(int portnum, struct jport *jps)
{
	for (int i = 0; i < MAX_STORED_JPORTS; i++) {
		struct stored_jport *jp = &stored_jports[portnum][i];
		if (jp->inuse && &jp->jp == jps && !jp->defaultports) {
			stored_jport_cnt++;
			jp->age = stored_jport_cnt;
		}
	}
}

static void inputdevice_store_used_device(struct jport *jps, int portnum, bool defaultports)
{
	if (jps->id == JPORT_NONE)
		return;

	// already added? if custom or kbr layout: delete all old
	for (int i = 0; i < MAX_STORED_JPORTS; i++) {
		struct stored_jport *jp = &stored_jports[portnum][i];
		if (jp->inuse && ((jps->id == jp->jp.id) || (jps->idc.configname[0] != 0 && jp->jp.idc.configname[0] != 0 && !_tcscmp(jps->idc.configname, jp->jp.idc.configname)))) {
			if (!jp->defaultports) {
				jp->inuse = false;
			}
		}
	}

	// delete from other ports
	for (int j = 0; j < MAX_JPORTS; j++) {
		for (int i = 0; i < MAX_STORED_JPORTS; i++) {
			struct stored_jport *jp = &stored_jports[j][i];
			if (jp->inuse && ((jps->id == jp->jp.id) || (jps->idc.configname[0] != 0 && jp->jp.idc.configname[0] != 0 && !_tcscmp(jps->idc.configname, jp->jp.idc.configname)))) {
				if (!jp->defaultports) {
					jp->inuse = false;
				}
			}
		}
	}
	// delete oldest if full
	for (int i = 0; i < MAX_STORED_JPORTS; i++) {
		struct stored_jport *jp = &stored_jports[portnum][i];
		if (!jp->inuse)
			break;
		if (i == MAX_STORED_JPORTS - 1) {
			uae_u32 age = 0xffffffff;
			int idx = -1;
			for (int j = 0; j < MAX_STORED_JPORTS; j++) {
				struct stored_jport *jp = &stored_jports[portnum][j];
				if (jp->age < age && !jp->defaultports) {
					age = jp->age;
					idx = j;
				}
			}
			if (idx >= 0) {
				struct stored_jport *jp = &stored_jports[portnum][idx];
				jp->inuse = false;
			}
		}
	}
	// add new	
	for (int i = 0; i < MAX_STORED_JPORTS; i++) {
		struct stored_jport *jp = &stored_jports[portnum][i];
		if (!jp->inuse) {
			memcpy(&jp->jp, jps, sizeof(struct jport));
			write_log(_T("Stored port %d/%d d=%d: added %d %d %s %s\n"), portnum, i, defaultports, jp->jp.id, jp->jp.mode, jp->jp.idc.name, jp->jp.idc.configname);
			jp->inuse = true;
			jp->defaultports = defaultports;
			stored_jport_cnt++;
			jp->age = stored_jport_cnt;
			return;
		}
	}
}

static void inputdevice_store_unplugged_port(struct uae_prefs *p, struct inputdevconfig *idc)
{
	struct jport jpt = { 0 };
	_tcscpy(jpt.idc.configname, idc->configname);
	_tcscpy(jpt.idc.name, idc->name);
	jpt.id = JPORT_UNPLUGGED;
	for (int i = 0; i < MAX_JPORTS; i++) {
		struct jport *jp = &p->jports[i];
		if (!_tcscmp(jp->idc.name, idc->name) && !_tcscmp(jp->idc.configname, idc->configname)) {
			write_log(_T("On the fly unplugged stored, port %d '%s' (%s)\n"), i, jpt.idc.name, jpt.idc.configname);
			inputdevice_store_used_device(&jpt, i, false);
		}
	}
}

static bool isemptykey(int keyboard, int scancode)
{
	int j = 0;
	struct uae_input_device *na = &keyboards[keyboard];
	while (j < MAX_INPUT_DEVICE_EVENTS && na->extra[j] >= 0) {
		if (na->extra[j] == scancode) {
			for (int k = 0; k < MAX_INPUT_SUB_EVENT; k++) {
				if (na->eventid[j][k] > 0)
					return false;
				if (na->custom[j][k] != NULL)
					return false;
			}
			break;
		}
		j++;
	}
	return true;
}

static void out_config (struct zfile *f, int id, int num, const TCHAR *s1, const TCHAR *s2)
{
	TCHAR tmp[MAX_DPATH];
	_stprintf (tmp, _T("input.%d.%s%d"), id, s1, num);
	cfgfile_write_str (f, tmp, s2);
}

static bool write_config_head (struct zfile *f, int idnum, int devnum, const TCHAR *name, struct uae_input_device *id,  struct inputdevice_functions *idf)
{
	TCHAR *s = NULL;
	TCHAR tmp2[CONFIG_BLEN];

	if (idnum == GAMEPORT_INPUT_SETTINGS) {
		if (!isdevice (id))
			return false;
		if (!id->enabled)
			return false;
	}

	if (id->name)
		s = id->name;
	else if (devnum < idf->get_num ())
		s = idf->get_friendlyname (devnum);
	if (s) {
		_stprintf (tmp2, _T("input.%d.%s.%d.friendlyname"), idnum + 1, name, devnum);
		cfgfile_write_str (f, tmp2, s);
	}

	s = NULL;
	if (id->configname)
		s = id->configname;
	else if (devnum < idf->get_num ())
		s = idf->get_uniquename (devnum);
	if (s) {
		_stprintf (tmp2, _T("input.%d.%s.%d.name"), idnum + 1, name, devnum);
		cfgfile_write_str (f, tmp2, s);
	}

	if (!isdevice (id)) {
		_stprintf (tmp2, _T("input.%d.%s.%d.empty"), idnum + 1, name, devnum);
		cfgfile_write_bool (f, tmp2, true);
		if (id->enabled) {
			_stprintf (tmp2, _T("input.%d.%s.%d.disabled"), idnum + 1, name, devnum);
			cfgfile_write_bool (f, tmp2, id->enabled ? false : true);
		}
		return false;
	}

	if (idnum == GAMEPORT_INPUT_SETTINGS) {
		_stprintf (tmp2, _T("input.%d.%s.%d.custom"), idnum + 1, name, devnum);
		cfgfile_write_bool (f, tmp2, true);
	} else {
		_stprintf (tmp2, _T("input.%d.%s.%d.empty"), idnum + 1, name, devnum);
		cfgfile_write_bool (f, tmp2, false);
		_stprintf (tmp2, _T("input.%d.%s.%d.disabled"), idnum + 1, name, devnum);
		cfgfile_write_bool (f, tmp2, id->enabled ? false : true);
	}
	return true;
}

static bool write_slot (TCHAR *p, struct uae_input_device *uid, int i, int j)
{
	bool ok = false;
	if (i < 0 || j < 0) {
		_tcscpy (p, _T("NULL"));
		return false;
	}
	uae_u64 flags = uid->flags[i][j];
	if (uid->custom[i][j] && _tcslen (uid->custom[i][j]) > 0) {
		_stprintf (p, _T("'%s'.%d"), uid->custom[i][j], (int)(flags & ID_FLAG_SAVE_MASK_CONFIG));
		ok = true;
	} else if (uid->eventid[i][j] > 0) {
		_stprintf (p, _T("%s.%d"), events[uid->eventid[i][j]].confname, (int)(flags & ID_FLAG_SAVE_MASK_CONFIG));
		ok = true;
	} else {
		_tcscpy (p, _T("NULL"));
	}
	if (ok && (flags & ID_FLAG_SAVE_MASK_QUALIFIERS)) {
		TCHAR *p2 = p + _tcslen (p);
		*p2++ = '.';
		for (int i = 0; i < MAX_INPUT_QUALIFIERS * 2; i++) {
			if ((ID_FLAG_QUALIFIER1 << i) & flags) {
				if (i & 1)
					_stprintf (p2, _T("%c"), 'a' + i / 2);
				else
					_stprintf (p2, _T("%c"), 'A' + i / 2);
				p2++;
			}
		}
	}
	return ok;
}

static struct inputdevice_functions *getidf (int devnum);

static void kbrlabel (TCHAR *s)
{
	while (*s) {
		*s = _totupper (*s);
		if (*s == ' ')
			*s = '_';
		s++;
	}
}

static void write_config2 (struct zfile *f, int idnum, int i, int offset, const TCHAR *extra, struct uae_input_device *id)
{
	TCHAR tmp2[CONFIG_BLEN], tmp3[CONFIG_BLEN], *p;
	int evt, got, j, k;
	TCHAR *custom;
	const int *slotorder;
	int io = i + offset;

	tmp2[0] = 0;
	p = tmp2;
	got = 0;

	slotorder = slotorder1;
	// if gameports non-custom mapping in slot0 -> save slot8 as slot0
	if (id->port[io][0] && !(id->flags[io][0] & ID_FLAG_GAMEPORTSCUSTOM_MASK))
		slotorder = slotorder2;

	for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {

		evt = id->eventid[io][slotorder[j]];
		custom = id->custom[io][slotorder[j]];
		if (custom == NULL && evt <= 0) {
			for (k = j + 1; k < MAX_INPUT_SUB_EVENT; k++) {
				if ((id->port[io][k] == 0 || id->port[io][k] == MAX_JPORTS + 1) && (id->eventid[io][slotorder[k]] > 0 || id->custom[io][slotorder[k]] != NULL))
					break;
			}
			if (k == MAX_INPUT_SUB_EVENT)
				break;
		}

		if (p > tmp2) {
			*p++ = ',';
			*p = 0;
		}
		bool ok = write_slot (p, id, io, slotorder[j]);
		p += _tcslen (p);
		if (ok) {
			if (id->port[io][slotorder[j]] > 0 && id->port[io][slotorder[j]] < MAX_JPORTS + 1) {
				int pnum = id->port[io][slotorder[j]] - 1;
				_stprintf (p, _T(".%d"), pnum);
				p += _tcslen (p);
				if (idnum != GAMEPORT_INPUT_SETTINGS && j == 0 && id->port[io][SPARE_SUB_EVENT] && slotorder == slotorder1) {
					*p++ = '.';
					write_slot (p, id, io, SPARE_SUB_EVENT);
					p += _tcslen (p);
				}
			}
		}
	}
	if (p > tmp2) {
		_stprintf (tmp3, _T("input.%d.%s%d"), idnum + 1, extra, i);
		cfgfile_write_str (f, tmp3, tmp2);
	}
}

static void write_kbr_config (struct zfile *f, int idnum, int devnum, struct uae_input_device *kbr, struct inputdevice_functions *idf)
{
	TCHAR tmp1[CONFIG_BLEN], tmp2[CONFIG_BLEN], tmp3[CONFIG_BLEN], tmp4[CONFIG_BLEN], tmp5[CONFIG_BLEN], *p;
	int i, j, k, evt, skip;
	const int *slotorder;

	if (!keyboard_default)
		return;

	if (!write_config_head (f, idnum, devnum, _T("keyboard"), kbr, idf))
		return;

	i = 0;
	while (i < MAX_INPUT_DEVICE_EVENTS && kbr->extra[i] >= 0) {

		slotorder = slotorder1;
		// if gameports non-custom mapping in slot0 -> save slot8 as slot0
		if (kbr->port[i][0] && !(kbr->flags[i][0] & ID_FLAG_GAMEPORTSCUSTOM_MASK))
			slotorder = slotorder2;

		skip = 0;
		k = 0;
		while (keyboard_default[k].scancode >= 0) {
			if (keyboard_default[k].scancode == kbr->extra[i]) {
				skip = 1;
				for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
					if (keyboard_default[k].node[j].evt != 0) {
						if (keyboard_default[k].node[j].evt != kbr->eventid[i][slotorder[j]] || keyboard_default[k].node[j].flags != (kbr->flags[i][slotorder[j]] & ID_FLAG_SAVE_MASK_FULL))
							skip = 0;
					} else if ((kbr->flags[i][slotorder[j]] & ID_FLAG_SAVE_MASK_FULL) != 0 || kbr->eventid[i][slotorder[j]] > 0) {
						skip = 0;
					}
				}
				break;
			}
			k++;
		}
		bool isdefaultspare =
			kbr->port[i][SPARE_SUB_EVENT] &&
			keyboard_default[k].node[0].evt == kbr->eventid[i][SPARE_SUB_EVENT] && keyboard_default[k].node[0].flags == (kbr->flags[i][SPARE_SUB_EVENT] & ID_FLAG_SAVE_MASK_FULL);

		if (kbr->port[i][0] > 0 && !(kbr->flags[i][0] & ID_FLAG_GAMEPORTSCUSTOM_MASK) && 
			(kbr->eventid[i][1] <= 0 && kbr->eventid[i][2] <= 0 && kbr->eventid[i][3] <= 0) &&
			(kbr->port[i][SPARE_SUB_EVENT] == 0 || isdefaultspare))
			skip = 1;
		if (kbr->eventid[i][0] == 0 && (kbr->flags[i][0] & ID_FLAG_SAVE_MASK_FULL) == 0 && keyboard_default[k].scancode < 0)
			skip = 1;
		if (skip) {
			i++;
			continue;
		}

		if (!input_get_default_keyboard(devnum)) {
			bool isempty = true;
			for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
				if (kbr->eventid[i][j] > 0) {
					isempty = false;
					break;
				}
				if (kbr->custom[i][j] != NULL) {
					isempty = false;
					break;
				}
			}
			if (isempty) {
				i++;
				continue;
			}
		}

		tmp2[0] = 0;
		p = tmp2;
		for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			TCHAR *custom = kbr->custom[i][slotorder[j]];
			evt = kbr->eventid[i][slotorder[j]];
			if (custom == NULL && evt <= 0) {
				for (k = j + 1; k < MAX_INPUT_SUB_EVENT; k++) {
					if (kbr->eventid[i][slotorder[k]] > 0 || kbr->custom[i][slotorder[k]] != NULL)
						break;
				}
				if (k == MAX_INPUT_SUB_EVENT)
					break;
			}
			if (p > tmp2) {
				*p++ = ',';
				*p = 0;
			}
			bool ok = write_slot (p, kbr, i, slotorder[j]);
			p += _tcslen (p);
			if (ok) {
				// save port number + SPARE SLOT if needed
				if (kbr->port[i][slotorder[j]] > 0 && (kbr->flags[i][slotorder[j]] & ID_FLAG_GAMEPORTSCUSTOM_MASK)) {
					_stprintf (p, _T(".%d"), kbr->port[i][slotorder[j]] - 1);
					p += _tcslen (p);
					if (idnum != GAMEPORT_INPUT_SETTINGS && j == 0 && kbr->port[i][SPARE_SUB_EVENT] && !isdefaultspare && slotorder == slotorder1) {
						*p++ = '.';
						write_slot (p, kbr, i, SPARE_SUB_EVENT);
						p += _tcslen (p);
					}
				}
			}
		}
		idf->get_widget_type (devnum, i, tmp5, NULL);
		_stprintf (tmp3, _T("%d%s%s"), kbr->extra[i], tmp5[0] ? _T(".") : _T(""), tmp5[0] ? tmp5 : _T(""));
		kbrlabel (tmp3);
		_stprintf (tmp1, _T("keyboard.%d.button.%s"), devnum, tmp3);
		_stprintf (tmp4, _T("input.%d.%s"), idnum + 1, tmp1);
		cfgfile_write_str (f, tmp4, tmp2[0] ? tmp2 : _T("NULL"));
		i++;
	}
}

static void write_config (struct zfile *f, int idnum, int devnum, const TCHAR *name, struct uae_input_device *id, struct inputdevice_functions *idf)
{
	TCHAR tmp1[MAX_DPATH];
	int i;

	if (!write_config_head (f, idnum, devnum, name, id, idf))
		return;

	_stprintf (tmp1, _T("%s.%d.axis."), name, devnum);
	for (i = 0; i < ID_AXIS_TOTAL; i++)
		write_config2 (f, idnum, i, ID_AXIS_OFFSET, tmp1, id);
	_stprintf (tmp1, _T("%s.%d.button.") ,name, devnum);
	for (i = 0; i < ID_BUTTON_TOTAL; i++)
		write_config2 (f, idnum, i, ID_BUTTON_OFFSET, tmp1, id);
}

static const TCHAR *kbtypes[] = { _T("amiga"), _T("pc"), NULL };

void write_inputdevice_config (struct uae_prefs *p, struct zfile *f)
{
	int i, id;

	cfgfile_write (f, _T("input.config"), _T("%d"), p->input_selected_setting == GAMEPORT_INPUT_SETTINGS ? 0 : p->input_selected_setting + 1);
  cfgfile_write (f, _T("input.joymouse_speed_analog"), _T("%d"), p->input_joymouse_multiplier);
	cfgfile_write (f, _T("input.joymouse_speed_digital"), _T("%d"), p->input_joymouse_speed);
	cfgfile_write (f, _T("input.joymouse_deadzone"), _T("%d"), p->input_joymouse_deadzone);
	cfgfile_write (f, _T("input.joystick_deadzone"), _T("%d"), p->input_joystick_deadzone);
	cfgfile_write (f, _T("input.analog_joystick_multiplier"), _T("%d"), p->input_analog_joystick_mult);
	cfgfile_write (f, _T("input.analog_joystick_offset"), _T("%d"), p->input_analog_joystick_offset);
	cfgfile_write (f, _T("input.mouse_speed"), _T("%d"), p->input_mouse_speed);
	cfgfile_write (f, _T("input.autofire_speed"), _T("%d"), p->input_autofire_linecnt);
	cfgfile_dwrite_str (f, _T("input.keyboard_type"), kbtypes[p->input_keyboard_type]);
	for (id = 0; id < MAX_INPUT_SETTINGS; id++) {
		TCHAR tmp[MAX_DPATH];
		if (id < GAMEPORT_INPUT_SETTINGS) {
			_stprintf (tmp, _T("input.%d.name"), id + 1);
			cfgfile_dwrite_str (f, tmp, p->input_config_name[id]);
		}
		for (i = 0; i < MAX_INPUT_DEVICES; i++)
			write_config (f, id, i, _T("joystick"), &p->joystick_settings[id][i], &idev[IDTYPE_JOYSTICK]);
		for (i = 0; i < MAX_INPUT_DEVICES; i++)
			write_config (f, id, i, _T("mouse"), &p->mouse_settings[id][i], &idev[IDTYPE_MOUSE]);
		for (i = 0; i < MAX_INPUT_DEVICES; i++)
			write_kbr_config (f, id, i, &p->keyboard_settings[id][i], &idev[IDTYPE_KEYBOARD]);
	}
}

static uae_u64 getqual (const TCHAR **pp)
{
	const TCHAR *p = *pp;
	uae_u64 mask = 0;

	while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
		bool press = (*p >= 'A' && *p <= 'Z');
		int shift, inc;

		if (press) {
			shift = *p - 'A';
			inc = 0;
		} else {
			shift = *p - 'a';
			inc = 1;
		}
		mask |= ID_FLAG_QUALIFIER1 << (shift * 2 + inc);
		p++;
	}
	while (*p != 0 && *p !='.' && *p != ',')
		p++;
	if (*p == '.' || *p == ',')
		p++;
	*pp = p;
	return mask;
}

static int getnum (const TCHAR **pp)
{
	const TCHAR *p = *pp;
	int v;

	if (!_tcsnicmp (p, _T("false"), 5))
		v = 0;
	else if (!_tcsnicmp (p, _T("true"), 4))
		v = 1;
	else
		v = _tstol (p);

	while (*p != 0 && *p !='.' && *p != ',' && *p != '=')
		p++;
	if (*p == '.' || *p == ',')
		p++;
	*pp = p;
	return v;
}
static TCHAR *getstring (const TCHAR **pp)
{
  int i;
	static TCHAR str[CONFIG_BLEN];
  const TCHAR *p = *pp;
	bool quoteds = false;
	bool quotedd = false;

  if (*p == 0)
  	return 0;
  i = 0;
	while (*p != 0 && i < 1000 - 1) {
		if (*p == '\"')
			quotedd = quotedd ? false : true;
		if (*p == '\'')
			quoteds = quoteds ? false : true;
		if (!quotedd && !quoteds) {
			if (*p == '.' || *p == ',')
				break;
		}
    str[i++] = *p++;
	}
  if (*p == '.' || *p == ',') 
    p++;
  str[i] = 0;
  *pp = p;
  return str;
}

static void reset_inputdevice_settings (struct uae_input_device *uid)
{
	for (int l = 0; l < MAX_INPUT_DEVICE_EVENTS; l++) {
		for (int i = 0; i < MAX_INPUT_SUB_EVENT_ALL; i++) {
			uid->eventid[l][i] = 0;
			uid->flags[l][i] = 0;
			if (uid->custom[l][i]) {
			  xfree (uid->custom[l][i]);
			  uid->custom[l][i] = NULL;
		  }
	  }
  }
}
static void reset_inputdevice_slot (struct uae_prefs *prefs, int slot)
{
	for (int m = 0; m < MAX_INPUT_DEVICES; m++) {
		reset_inputdevice_settings (&prefs->joystick_settings[slot][m]);
		reset_inputdevice_settings (&prefs->mouse_settings[slot][m]);
		reset_inputdevice_settings (&prefs->keyboard_settings[slot][m]);
	}
}

static void reset_inputdevice_config_temp(void)
{
	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		for (int j = 0; j < IDTYPE_MAX; j++) {
			temp_uid_index[i][j] = -1;
		}
	}
	for (int i = 0; i < IDTYPE_MAX; i++) {
		temp_uid_cnt[i] = 0;
	}
	memset(&temp_uid, 0, sizeof temp_uid);
	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		temp_uid.matcheddevices[i] = -1;
		temp_uid.nonmatcheddevices[i] = -1;
	}
	temp_uid.idnum = -1;
	temp_uid.lastdevtype = -1;
}

void reset_inputdevice_config (struct uae_prefs *prefs, bool reset)
{
	for (int i = 0; i< MAX_INPUT_SETTINGS; i++)
		reset_inputdevice_slot (prefs, i);
	reset_inputdevice_config_temp();

	if (reset) {
		inputdevice_store_clear();
	}
}

static void set_kbr_default_event (struct uae_input_device *kbr, struct uae_input_device_kbr_default *trans, int num)
{
	if (!kbr->enabled || !trans)
		return;
	for (int i = 0; trans[i].scancode >= 0; i++) {
		if (kbr->extra[num] == trans[i].scancode) {
			int k;
			for (k = 0; k < MAX_INPUT_SUB_EVENT; k++) {
				if (kbr->eventid[num][k] == 0)
					break;
			}
			if (k == MAX_INPUT_SUB_EVENT) {
				write_log (_T("corrupt default keyboard mappings\n"));
				return;
			}
			int l = 0;
			while (k < MAX_INPUT_SUB_EVENT && trans[i].node[l].evt) {
				int evt = trans[i].node[l].evt;
				if (evt < 0 || evt >= INPUTEVENT_SPC_LAST)
					gui_message(_T("invalid event in default keyboard table!"));
				kbr->eventid[num][k] = evt;
				kbr->flags[num][k] = trans[i].node[l].flags;
				l++;
				k++;
			}
			break;
		}
	}
}

static void clear_id (struct uae_input_device *id)
{
	int i, j;
	for (i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (j = 0; j < MAX_INPUT_SUB_EVENT_ALL; j++)
			xfree (id->custom[i][j]);
	}

	TCHAR *cn = id->configname;
	TCHAR *n = id->name;
	memset (id, 0, sizeof (struct uae_input_device));
	id->configname = cn;
	id->name = n;
}

static void set_kbr_default (struct uae_prefs *p, int index, int devnum, struct uae_input_device_kbr_default *trans)
{
	int i, j;
	struct uae_input_device *kbr;
	struct inputdevice_functions *id = &idev[IDTYPE_KEYBOARD];
	uae_u32 scancode;

	for (j = 0; j < MAX_INPUT_DEVICES; j++) {
		if (devnum >= 0 && devnum != j)
			continue;
		kbr = &p->keyboard_settings[index][j];
		uae_s8 ena = kbr->enabled;
		clear_id (kbr);
		if (ena > 0)
			kbr->enabled = ena;
		for (i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++)
			kbr->extra[i] = -1;
		if (j < id->get_num ()) {
			if (input_get_default_keyboard (j))
				kbr->enabled = 1;
			for (i = 0; i < id->get_widget_num (j); i++) {
				id->get_widget_type (j, i, 0, &scancode);
				kbr->extra[i] = scancode;
				if (j == 0 || kbr->enabled)
				  set_kbr_default_event (kbr, trans, i);
			}
		}
	}
}

static void inputdevice_default_kb (struct uae_prefs *p, int num)
{
	if (num == GAMEPORT_INPUT_SETTINGS) {
		reset_inputdevice_slot (p, num);
	}
	set_kbr_default (p, num, -1, keyboard_default);
}

static void inputdevice_default_kb_all (struct uae_prefs *p)
{
	for (int i = 0; i < MAX_INPUT_SETTINGS; i++)
		inputdevice_default_kb (p, i);
}

static const int af_port1[] = {
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_CD32_RED,
	-1
};
static const int af_port2[] = {
	INPUTEVENT_JOY2_FIRE_BUTTON, INPUTEVENT_JOY2_CD32_RED,
	-1
};
static const int af_port3[] = {
	INPUTEVENT_PAR_JOY1_FIRE_BUTTON, INPUTEVENT_PAR_JOY1_2ND_BUTTON,
	-1
};
static const int af_port4[] = {
	INPUTEVENT_PAR_JOY2_FIRE_BUTTON, INPUTEVENT_PAR_JOY2_2ND_BUTTON,
	-1
};
static const int *af_ports[] = { af_port1, af_port2, af_port3, af_port4 };

static void setautofireevent (struct uae_input_device *uid, int num, int sub, int af, int index)
{
	if (!af)
		return;
	const int *afp = af_ports[index];
	for (int k = 0; afp[k] >= 0; k++) {
		if (afp[k] == uid->eventid[num][sub]) {
			uid->flags[num][sub] &= ~ID_FLAG_AUTOFIRE_MASK;
			if (af >= JPORT_AF_NORMAL)
				uid->flags[num][sub] |= ID_FLAG_AUTOFIRE;
#ifndef INPUTDEVICE_SIMPLE
			if (af == JPORT_AF_TOGGLE)
				uid->flags[num][sub] |= ID_FLAG_TOGGLE;
			if (af == JPORT_AF_ALWAYS)
				uid->flags[num][sub] |= ID_FLAG_INVERTTOGGLE;
#endif
			return;
		}
	}
}

static void setcompakbevent(struct uae_prefs *p, struct uae_input_device *uid, int l, int evt, int port, int af)
{
	inputdevice_sparecopy(uid, l, 0);
	if (p->jports[port].nokeyboardoverride && uid->port[l][0] == 0) {
		uid->eventid[l][MAX_INPUT_SUB_EVENT - 1] = uid->eventid[l][0];
		uid->flags[l][MAX_INPUT_SUB_EVENT - 1] = uid->flags[l][0] | ID_FLAG_RESERVEDGAMEPORTSCUSTOM;
		uid->custom[l][MAX_INPUT_SUB_EVENT - 1] = my_strdup(uid->custom[l][0]);
		uid->eventid[l][MAX_INPUT_SUB_EVENT - 1] = uid->eventid[l][0];
	}
	uid->eventid[l][0] = evt;
	uid->flags[l][0] &= COMPA_RESERVED_FLAGS;
	uid->port[l][0] = port + 1;
	xfree(uid->custom[l][0]);
	uid->custom[l][0] = NULL;
	setautofireevent(uid, l, 0, af, port);
}

static int matchdevice(struct inputdevice_functions *inf, const TCHAR *configname, const TCHAR *name)
{
	int match = -1;
	for (int j = 0; j < 2; j++) {
		bool fullmatch = j == 0;
		for (int i = 0; i < inf->get_num(); i++) {
			TCHAR *aname1 = inf->get_friendlyname(i);
			TCHAR *aname2 = inf->get_uniquename(i);
			if (fullmatch && (!aname1 || !name))
				continue;
			if (aname2 && configname) {
				bool matched = false;
				TCHAR bname[MAX_DPATH];
				TCHAR bname2[MAX_DPATH];
				TCHAR *p1, *p2;
				_tcscpy(bname, configname);
				_tcscpy(bname2, aname2);
				// strip possible local guid part
				p1 = _tcschr(bname, '{');
				p2 = _tcschr(bname2, '{');
				if (!p1 && !p2) {
					// check possible directinput names too
					p1 = _tcschr(bname, ' ');
					p2 = _tcschr(bname2, ' ');
				}
				if (!_tcscmp(bname, bname2)) {
					matched = true;
				} else if (p1 && p2 && p1 - bname == p2 - bname2) {
					*p1 = 0;
					*p2 = 0;
					if (bname[0] && !_tcscmp(bname2, bname))
						matched = true;
				}
				if (matched && fullmatch && _tcscmp(aname1, name) != 0)
					matched = false;
				if (matched) {
					if (match >= 0)
						match = -2;
					else
						match = i;
				}
				if (match == -2)
					break;
			}
		}
		if (match != -1)
			break;
	}
	// multiple matches -> use complete local-only id string for comparisons
	if (match == -2) {
		for (int j = 0; j < 2; j++) {
			bool fullmatch = j == 0;
			match = -1;
			for (int i = 0; i < inf->get_num(); i++) {
				TCHAR *aname1 = inf->get_friendlyname(i);
				TCHAR *aname2 = inf->get_uniquename(i);
				if (aname2 && configname) {
					const TCHAR *bname2 = configname;
					bool matched = false;
					if (fullmatch && (!aname1 || !name))
						continue;
					if (aname2 && bname2 && !_tcscmp(aname2, bname2))
						matched = true;
					if (matched && fullmatch && _tcscmp(aname1, name) != 0)
						matched = false;
					if (matched) {
						if (match >= 0) {
							match = -2;
							break;
						} else {
							match = i;
						}
					}
				}
			}
			if (match != -1)
				break;
		}
	}
	if (match < 0) {
		// no match, try friendly names
		for (int i = 0; i < inf->get_num(); i++) {
			TCHAR *aname1 = inf->get_friendlyname(i);
			if (aname1 && name) {
				const TCHAR *bname1 = name;
				if (aname1 && bname1 && !_tcscmp(aname1, bname1)) {
					if (match >= 0) {
						match = -2;
						break;
					} else {
						match = i;
					}
				}
			}
		}
	}
	return match;
}

static bool read_slot (const TCHAR *parm, int num, int joystick, int button, struct uae_input_device *id, int keynum, int subnum, const struct inputevent *ie, uae_u64 flags, int port, TCHAR *custom)
{
	int mask;

	if (custom == NULL && ie->name == NULL) {
		if (!_tcscmp (parm, _T("NULL"))) {
			if (joystick < 0) {
				id->eventid[keynum][subnum] = 0;
				id->flags[keynum][subnum] = 0;
			} else if (button) {
				id->eventid[num + ID_BUTTON_OFFSET][subnum] = 0;
				id->flags[num + ID_BUTTON_OFFSET][subnum] = 0;
			} else {
				id->eventid[num + ID_AXIS_OFFSET][subnum] = 0;
				id->flags[num + ID_AXIS_OFFSET][subnum] = 0;
			}
		}
		return false;
	}
	if (custom)
		ie = &events[INPUTEVENT_SPC_CUSTOM_EVENT];

	if (joystick < 0) {
		if (!(ie->allow_mask & AM_K))
			return false;
		id->eventid[keynum][subnum] = ie - events;
		id->flags[keynum][subnum] = flags;
		id->port[keynum][subnum] = port;
		xfree (id->custom[keynum][subnum]);
		id->custom[keynum][subnum] = custom;
	} else  if (button) {
		if (joystick)
			mask = AM_JOY_BUT;
		else
			mask = AM_MOUSE_BUT;
		if (!(ie->allow_mask & mask))
			return false;
		id->eventid[num + ID_BUTTON_OFFSET][subnum] = ie - events;
		id->flags[num + ID_BUTTON_OFFSET][subnum] = flags;
		id->port[num + ID_BUTTON_OFFSET][subnum] = port;
		xfree (id->custom[num + ID_BUTTON_OFFSET][subnum]);
		id->custom[num + ID_BUTTON_OFFSET][subnum] = custom;
	} else {
		if (joystick)
			mask = AM_JOY_AXIS;
		else
			mask = AM_MOUSE_AXIS;
		if (!(ie->allow_mask & mask))
			return false;
		id->eventid[num + ID_AXIS_OFFSET][subnum] = ie - events;
		id->flags[num + ID_AXIS_OFFSET][subnum] = flags;
		id->port[num + ID_AXIS_OFFSET][subnum] = port;
		xfree (id->custom[num + ID_AXIS_OFFSET][subnum]);
		id->custom[num + ID_AXIS_OFFSET][subnum] = custom;
	}
	return true;
}

static const struct inputevent *readevent (const TCHAR *name, TCHAR **customp)
{
	int i = 1;
	while (events[i].name) {
		if (!_tcscmp (events[i].confname, name))
			return &events[i];
		i++;
	}
	if (_tcslen (name) > 2 && name[0] == '\'') {
		name++;
		const TCHAR *end = name;
		while (*end && *end != '\'')
			end++;
		if (!customp || *end == 0)
			return NULL;
		TCHAR *custom = my_strdup (name);
		custom[end - name] = 0;
		*customp = custom;
	}
	return &events[0];
}

void read_inputdevice_config (struct uae_prefs *pr, const TCHAR *option, TCHAR *value)
{
	struct uae_input_device *id = NULL;
	const struct inputevent *ie;
	int devnum, num, button, joystick, subnum, idnum, keynum, devtype;
  const TCHAR *p;
	TCHAR *p2, *custom;
	struct temp_uids *tid = &temp_uid;
	struct inputdevice_functions *idf = NULL;

  option += 6; /* "input." */
  p = getstring (&option);
	if (!strcasecmp (p, _T("config"))) {
		pr->input_selected_setting = _tstol (value) - 1;
		if (pr->input_selected_setting == -1)
			pr->input_selected_setting = GAMEPORT_INPUT_SETTINGS;
		if (pr->input_selected_setting < 0 || pr->input_selected_setting > MAX_INPUT_SETTINGS)
			pr->input_selected_setting = 0;
	}
  if (!strcasecmp (p, _T("joymouse_speed_analog")))
	  pr->input_joymouse_multiplier = _tstol (value);
	if (!strcasecmp (p, _T("joymouse_speed_digital")))
		pr->input_joymouse_speed = _tstol (value);
	if (!strcasecmp (p, _T("joystick_deadzone")))
		pr->input_joystick_deadzone = _tstol (value);
	if (!strcasecmp (p, _T("joymouse_deadzone")))
		pr->input_joymouse_deadzone = _tstol (value);
	if (!strcasecmp (p, _T("mouse_speed")))
		pr->input_mouse_speed = _tstol (value);
  if (!strcasecmp (p, _T("autofire")))
	  pr->input_autofire_linecnt = _tstol (value) * 312;
	if (!strcasecmp (p, _T("autofire_speed")))
		pr->input_autofire_linecnt = _tstol (value);
	if (!strcasecmp (p, _T("analog_joystick_multiplier")))
		pr->input_analog_joystick_mult = _tstol (value);
	if (!strcasecmp (p, _T("analog_joystick_offset")))
		pr->input_analog_joystick_offset = _tstol (value);
	if (!strcasecmp (p, _T("keyboard_type"))) {
		cfgfile_strval (option, value, NULL, &pr->input_keyboard_type, kbtypes, 0);
		keyboard_default = keyboard_default_table[pr->input_keyboard_type];
		inputdevice_default_kb_all (pr);
	}

	idnum = _tstol (p);
	if (idnum <= 0 || idnum > MAX_INPUT_SETTINGS)
		return;
	idnum--;

	if (idnum != tid->idnum) {
		reset_inputdevice_config_temp();
		tid->idnum = idnum;
	}

	if (!_tcscmp (option, _T("name"))) {
		if (idnum < GAMEPORT_INPUT_SETTINGS)
			_tcscpy (pr->input_config_name[idnum], value);
		return;
	} 

	if (_tcsncmp (option, _T("mouse."), 6) == 0) {
		p = option + 6;
	} else if (_tcsncmp (option, _T("joystick."), 9) == 0) {
		p = option + 9;
	} else if (_tcsncmp (option, _T("keyboard."), 9) == 0) {
		p = option + 9;
	} else
		return;

	devnum = getnum (&p);
	if (devnum < 0 || devnum >= MAX_INPUT_DEVICES)
		return;

	p2 = getstring (&p);
	if (!p2)
		return;

	if (_tcsncmp (option, _T("mouse."), 6) == 0) {
		id = &pr->mouse_settings[idnum][devnum];
		joystick = 0;
		devtype = IDTYPE_MOUSE;
	} else if (_tcsncmp (option, _T("joystick."), 9) == 0) {
		id = &pr->joystick_settings[idnum][devnum];
		joystick = 1;
		devtype = IDTYPE_JOYSTICK;
	} else if (_tcsncmp (option, _T("keyboard."), 9) == 0) {
		id = &pr->keyboard_settings[idnum][devnum];
		joystick = -1;
		devtype = IDTYPE_KEYBOARD;
	}
	if (!id)
		return;
	idf = &idev[devtype];

	if (devtype != tid->lastdevtype) {
		tid->lastdevtype = devtype;
	}

	if (!_tcscmp (p2, _T("name"))) {
		xfree(tid->configname);
		tid->configname = my_strdup (value);
		tid->joystick = joystick;
		tid->devtype = devtype;
		tid->custom = false;
		tid->empty = false;
		tid->disabled = false;
		return;
	}
	if (!_tcscmp (p2, _T("friendlyname"))) {
		xfree (tid->name);
		tid->name = my_strdup (value);
		tid->joystick = joystick;
		tid->devtype = devtype;
		tid->custom = false;
		tid->empty = false;
		tid->disabled = false;
		return;
	}
	if (!_tcscmp (p2, _T("custom"))) {
		p = value;
		tid->custom = getnum(&p);
		tid->joystick = joystick;
		tid->devtype = devtype;
		tid->empty = false;
		return;
	}
	if (!_tcscmp(p2, _T("empty"))) {
		p = value;
		tid->empty = getnum(&p);
		tid->joystick = joystick;
		tid->devtype = devtype;
		return;
	}
	if (!_tcscmp(p2, _T("disabled"))) {
		p = value;
		tid->disabled = getnum(&p);
		tid->joystick = joystick;
		tid->devtype = devtype;
		return;
	}

	bool newdev = false;
	if (temp_uid_index[devnum][tid->devtype] == -1) {
		int newdevnum = -1;
		if (tid->devtype == IDTYPE_KEYBOARD) {
			// keyboard devnum == 0: always select keyboard zero.
			if (devnum == 0) {
				newdevnum = 0;
				tid->disabled = false;
				tid->empty = false;
			} else if (tid->kbreventcnt[0] == 0) {
				write_log(_T("Previous keyboard skipped\n"));
				// if previously found keyboard had no events, next will be tried again
				newdevnum = 0;
				tid->disabled = false;
				tid->empty = false;
			} else {
				newdevnum = matchdevice(idf, tid->configname, tid->name);
			}
		} else {
			// match devices with empty names to first free slot
			if (tid->configname && tid->configname[0] == 0 && tid->name && tid->name[0] == 0) {
				for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
					if (tid->matcheddevices[i] < 0) {
						newdevnum = i;
						break;
					}
				}
			} else {
  			newdevnum = matchdevice(idf, tid->configname, tid->name);
    	}
	}
		newdev = true;
		if (newdevnum >= 0) {
			temp_uid_index[devnum][tid->devtype] = newdevnum;
			write_log(_T("%d %d: %d -> %d (%s)\n"), idnum, tid->devtype, devnum, temp_uid_index[devnum][tid->devtype], tid->name);
			tid->matcheddevices[devnum] = newdevnum;
		} else {
			newdevnum = idf->get_num() + temp_uid_cnt[tid->devtype];
			if (newdevnum < MAX_INPUT_DEVICES) {
				temp_uid_index[devnum][tid->devtype] = newdevnum;
				temp_uid_cnt[tid->devtype]++;
				if (tid->name)
					write_log(_T("%d %d: %d -> %d (NO MATCH) (%s)\n"), idnum, tid->devtype, devnum, temp_uid_index[devnum][tid->devtype], tid->name);
				if (!tid->disabled && !tid->empty)
					tid->nonmatcheddevices[devnum] = newdevnum;
			} else {
				temp_uid_index[devnum][tid->devtype] = -1;
			}
		}
	}

	devnum = temp_uid_index[devnum][tid->devtype];
	if (devnum < 0) {
		if (devnum == -1)
			write_log(_T("%s (%s) not found and no free slots\n"), tid->name, tid->configname);
		temp_uid_index[devnum][tid->devtype] = -2;
		return;
	}

	if (tid->devtype == IDTYPE_MOUSE) {
		id = &pr->mouse_settings[idnum][devnum];
	} else if (tid->devtype == IDTYPE_JOYSTICK) {
		id = &pr->joystick_settings[idnum][devnum];
	} else if (tid->devtype == IDTYPE_KEYBOARD) {
		id = &pr->keyboard_settings[idnum][devnum];
	} else {
		return;
	}

	if (newdev) {
		if (!tid->initialized)
			clear_id(id);
		if (!tid->empty && tid->devtype == IDTYPE_KEYBOARD && !tid->initialized) {
			set_kbr_default(pr, idnum, devnum, keyboard_default);
		}
		tid->initialized = true;
		id->enabled = tid->disabled == 0 ? 1 : 0;
		if (idnum == GAMEPORT_INPUT_SETTINGS) {
			id->enabled = 0;
		}
		if (tid->custom) {
			id->enabled = 1;
		}
		xfree(tid->configname);
		xfree(tid->name);
		tid->configname = NULL;
		tid->name = NULL;
	}

	if (idnum == GAMEPORT_INPUT_SETTINGS && id->enabled == 0)
		return;

	button = 0;
	keynum = 0;
	joystick = tid->joystick;
	if (joystick < 0) {
		num = getnum (&p);
		for (keynum = 0; keynum < MAX_INPUT_DEVICE_EVENTS; keynum++) {
			if (id->extra[keynum] == num)
				break;
		}
		if (keynum >= MAX_INPUT_DEVICE_EVENTS)
			return;
	} else {
		button = -1;
		if (!_tcscmp (p2, _T("axis")))
			button = 0;
		else if(!_tcscmp (p2, _T("button")))
			button = 1;
		if (button < 0)
			return;
		num = getnum (&p);
	}
	p = value;

	bool oldcustommapping = false;
	custom = NULL;
	for (subnum = 0; subnum < MAX_INPUT_SUB_EVENT; subnum++) {
		uae_u64 flags;
		int port;
		xfree (custom);
		custom = NULL;
		p2 = getstring (&p);
		if (!p2)
			break;
		ie = readevent (p2, &custom);
		flags = 0;
		port = 0;
		if (p[-1] == '.')
			flags = getnum (&p) & ID_FLAG_SAVE_MASK_CONFIG;
		if (p[-1] == '.') {
			if ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'))
				flags |= getqual (&p);
			if (p[-1] == '.')
				port = getnum (&p) + 1;
		}
		if (flags & ID_FLAG_RESERVEDGAMEPORTSCUSTOM)
			oldcustommapping = true;
		if (idnum == GAMEPORT_INPUT_SETTINGS && port == 0)
			continue;
		if (p[-1] == '.' && idnum != GAMEPORT_INPUT_SETTINGS) {
			p2 = getstring (&p);
			if (p2) {
				int flags2 = 0;
				if (p[-1] == '.')
					flags2 = getnum (&p) & ID_FLAG_SAVE_MASK_CONFIG;
				if (p[-1] == '.' && (p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'))
					flags |= getqual (&p);
				TCHAR *custom2 = NULL;
				const struct inputevent *ie2 = readevent (p2, &custom2);
				read_slot (p2, num, joystick, button, id, keynum, SPARE_SUB_EVENT, ie2, flags2, MAX_JPORTS + 1, custom2);
			}
		}

		while (*p != 0) {
			if (p[-1] == ',')
				break;
			p++;
		}
		if (!read_slot (p2, num, joystick, button, id, keynum, subnum, ie, flags, port, custom))
			continue;
		custom = NULL;
	}
	if (joystick < 0 && !oldcustommapping)
		tid->kbreventcnt[devnum]++;
	xfree (custom);
}

static int mousehack_alive_cnt;
static int lastmx, lastmy;
static int mouseoffset_x, mouseoffset_y;
static int tablet_data;

STATIC_INLINE int mousehack_alive (void)
{
  return mousehack_alive_cnt > 0 ? mousehack_alive_cnt : 0;
}

#define MH_E 0
#define MH_CNT 2
#define MH_MAXX 4
#define MH_MAXY 6
#define MH_MAXZ 8
#define MH_X 10
#define MH_Y 12
#define MH_Z 14
#define MH_RESX 16
#define MH_RESY 18
#define MH_MAXAX 20
#define MH_MAXAY 22
#define MH_MAXAZ 24
#define MH_AX 26
#define MH_AY 28
#define MH_AZ 30
#define MH_PRESSURE 32
#define MH_BUTTONBITS 34
#define MH_INPROXIMITY 38
#define MH_ABSX 40
#define MH_ABSY 42

#define MH_END 44
#define MH_START 4

int inputdevice_is_tablet (void)
{
  if (uae_boot_rom_type <= 0)
  	return 0;
  if (currprefs.input_tablet == TABLET_OFF)
  	return 0;
  if (currprefs.input_tablet == TABLET_MOUSEHACK)
  	return -1;
  return 0;
}

static uae_u8 *mousehack_address;
static bool mousehack_enabled;

static void mousehack_reset (void)
{
  mouseoffset_x = mouseoffset_y = 0;
  mousehack_alive_cnt = 0;
  tablet_data = 0;
	if (rtarea_bank.baseaddr) {
		put_long_host(rtarea_bank.baseaddr + RTAREA_INTXY, 0xffffffff);
		if (mousehack_address)
			put_byte_host(mousehack_address + MH_E, 0);
	}
	mousehack_address = 0;
	mousehack_enabled = false;
}

static bool mousehack_enable (void)
{
  int mode;

	if (uae_boot_rom_type <= 0 || currprefs.input_tablet == TABLET_OFF)
  	return false;
	if (mousehack_address && mousehack_enabled)
  	return true;
  mode = 0x80;
  if (currprefs.input_tablet == TABLET_MOUSEHACK)
  	mode |= 1;
	if (mousehack_address && rtarea_bank.baseaddr) {
		write_log (_T("Mouse driver enabled (%s)\n"), _T("mousehack"));
		put_byte_host(mousehack_address + MH_E, mode);
		mousehack_enabled = true;
	}
	return true;
}

void input_mousehack_mouseoffset (uaecptr pointerprefs)
{
  mouseoffset_x = (uae_s16)get_word (pointerprefs + 28);
  mouseoffset_y = (uae_s16)get_word (pointerprefs + 30);
}

void mousehack_wakeup(void)
{
	if (mousehack_alive_cnt == 0)
		mousehack_alive_cnt = -100;
	else if (mousehack_alive_cnt > 0)
		mousehack_alive_cnt = 100;
	if (uaeboard_bank.baseaddr) {
		uaeboard_bank.baseaddr[0x201] &= ~3;
	}
}

int input_mousehack_status(TrapContext *ctx, int mode, uaecptr diminfo, uaecptr dispinfo, uaecptr vp, uae_u32 moffset)
{
	if (mode == 4) {
		return mousehack_enable () ? 1 : 0;
	} else if (mode == 5) {
		mousehack_address = (trap_get_dreg(ctx, 0) & 0xffff) + rtarea_bank.baseaddr;
		mousehack_enable ();
	} else if (mode == 0) {
		if (mousehack_address) {
			uae_u8 v = get_byte_host(mousehack_address + MH_E);
			v |= 0x40;
			put_byte_host(mousehack_address + MH_E, v);
			write_log (_T("Tablet driver running (%08x,%02x)\n"), mousehack_address, v);
		}
  }
	return 1;
}

void inputdevice_tablet_strobe (void)
{
  mousehack_enable ();
	if (uae_boot_rom_type <= 0)
  	return;
  if (!tablet_data)
  	return;
	if (mousehack_address)
		put_byte_host(mousehack_address + MH_CNT, get_byte_host(mousehack_address + MH_CNT) + 1);
}

static void inputdevice_mh_abs (int x, int y, uae_u32 buttonbits)
{
  x -= mouseoffset_x + 1;
  y -= mouseoffset_y + 2;

  mousehack_enable ();
	if (mousehack_address) {
  	uae_u8 tmp1[4], tmp2[4];
		uae_u8 *p = mousehack_address;

	  memcpy (tmp1, p + MH_ABSX, sizeof tmp1);
	  memcpy (tmp2, p + MH_BUTTONBITS, sizeof tmp2);

	  //write_log (_T("%04dx%04d %08x\n"), x, y, buttonbits);

    p[MH_ABSX] = x >> 8;
    p[MH_ABSX + 1] = x;
    p[MH_ABSY] = y >> 8;
    p[MH_ABSY + 1] = y;

	  p[MH_BUTTONBITS + 0] = buttonbits >> 24;
	  p[MH_BUTTONBITS + 1] = buttonbits >> 16;
	  p[MH_BUTTONBITS + 2] = buttonbits >>  8;
	  p[MH_BUTTONBITS + 3] = buttonbits >>  0;

	  if (!memcmp (tmp1, p + MH_ABSX, sizeof tmp1) && !memcmp (tmp2, p + MH_BUTTONBITS, sizeof tmp2))
    	return;
	  p[MH_E] = 0xc0 | 1;
    p[MH_CNT]++;
    tablet_data = 1;
  }
}

static void mousehack_helper (uae_u32 buttonmask)
{
  int x, y;

  if (currprefs.input_tablet < TABLET_MOUSEHACK)
  	return;
  x = lastmx;
  y = lastmy;

#ifdef PICASSO96
  if (picasso_on) {
	  x -= picasso96_state.XOffset;
	  y -= picasso96_state.YOffset;
  } else
#endif
  {
    x = coord_native_to_amiga_x (x);
	  y = coord_native_to_amiga_y (y) << 1;
  }
	inputdevice_mh_abs (x, y, buttonmask);
}

static int getbuttonstate (int joy, int button)
{
	return (joybutton[joy] & (1 << button)) ? 1 : 0;
}

static int getvelocity (int num, int subnum, int pct)
{
	int val;
	int v;

	if (pct > 1000)
		pct = 1000;
	val = mouse_delta[num][subnum];
	v = val * pct / 1000;
	if (!v) {
		if (val < -maxvpos / 2)
			v = -2;
		else if (val < 0)
			v = -1;
		else if (val > maxvpos / 2)
			v = 2;
		else if (val > 0)
			v = 1;
	}
	if (!mouse_deltanoreset[num][subnum]) {
		mouse_delta[num][subnum] -= v;
	}
	return v;
}

#define MOUSEXY_MAX 16384

static void mouseupdate (int pct, bool vsync)
{
	int v, i;
	int max = 120;
	static int mxd, myd;

	if (vsync) {
		mxd = 0;
		myd = 0;
	}

	for (i = 0; i < 2; i++) {

		if (mouse_port[i]) {

			v = getvelocity (i, 0, pct);
			mxd += v;
			mouse_x[i] += v;
			if (mouse_x[i] < 0) {
				mouse_x[i] += MOUSEXY_MAX;
				mouse_frame_x[i] = mouse_x[i] - v;
			}
			if (mouse_x[i] >= MOUSEXY_MAX) {
				mouse_x[i] -= MOUSEXY_MAX;
				mouse_frame_x[i] = mouse_x[i] - v;
			}

			v = getvelocity (i, 1, pct);
			myd += v;
			mouse_y[i] += v;
			if (mouse_y[i] < 0) {
				mouse_y[i] += MOUSEXY_MAX;
				mouse_frame_y[i] = mouse_y[i] - v;
			}
			if (mouse_y[i] >= MOUSEXY_MAX) {
				mouse_y[i] -= MOUSEXY_MAX;
				mouse_frame_y[i] = mouse_y[i] - v;
			}

#ifndef INPUTDEVICE_SIMPLE
			v = getvelocity (i, 2, pct);
			/* if v != 0, record mouse wheel key presses
			 * according to the NewMouse standard */
			if (v > 0)
				record_key (0x7a << 1);
			else if (v < 0)
				record_key (0x7b << 1);
#endif
			if (!mouse_deltanoreset[i][2])
				mouse_delta[i][2] = 0;

			if (mouse_frame_x[i] - mouse_x[i] > max) {
				mouse_x[i] = mouse_frame_x[i] - max;
				mouse_x[i] &= MOUSEXY_MAX - 1;
			}
			if (mouse_frame_x[i] - mouse_x[i] < -max) {
				mouse_x[i] = mouse_frame_x[i] + max;
				mouse_x[i] &= MOUSEXY_MAX - 1;
			}

			if (mouse_frame_y[i] - mouse_y[i] > max)
				mouse_y[i] = mouse_frame_y[i] - max;
			if (mouse_frame_y[i] - mouse_y[i] < -max)
				mouse_y[i] = mouse_frame_y[i] + max;
		}

		if (!vsync) {
			mouse_frame_x[i] = mouse_x[i];
			mouse_frame_y[i] = mouse_y[i];
		}

	}
}

static int input_vpos, input_frame;

static void readinput (void)
{
	uae_u32 totalvpos;
	int diff;

	totalvpos = input_frame * current_maxvpos () + vpos;
	diff = totalvpos - input_vpos;
	if (diff > 0) {
		if (diff < 10) {
			mouseupdate (0, false);
		} else {
			mouseupdate (diff * 1000 / current_maxvpos (), false);
		}
	}
	input_vpos = totalvpos;

}

static void joymousecounter (int joy)
{
	int left = 1, right = 1, top = 1, bot = 1;
	int b9, b8, b1, b0;
	int cntx, cnty, ocntx, ocnty;

	if (joydir[joy] & DIR_LEFT)
		left = 0;
	if (joydir[joy] & DIR_RIGHT)
		right = 0;
	if (joydir[joy] & DIR_UP)
		top = 0;
	if (joydir[joy] & DIR_DOWN)
		bot = 0;

	b0 = (bot ^ right) ? 1 : 0;
	b1 = (right ^ 1) ? 2 : 0;
	b8 = (top ^ left) ? 1 : 0;
	b9 = (left ^ 1) ? 2 : 0;

	cntx = b0 | b1;
	cnty = b8 | b9;
	ocntx = mouse_x[joy] & 3;
	ocnty = mouse_y[joy] & 3;

	if (cntx == 3 && ocntx == 0)
		mouse_x[joy] -= 4;
	else if (cntx == 0 && ocntx == 3)
		mouse_x[joy] += 4;
	mouse_x[joy] = (mouse_x[joy] & 0xfc) | cntx;

	if (cnty == 3 && ocnty == 0)
		mouse_y[joy] -= 4;
	else if (cnty == 0 && ocnty == 3)
		mouse_y[joy] += 4;
	mouse_y[joy] = (mouse_y[joy] & 0xfc) | cnty;

	if (!left || !right || !top || !bot) {
		mouse_frame_x[joy] = mouse_x[joy];
		mouse_frame_y[joy] = mouse_y[joy];
	}
}

static int inputdelay;

static void inputdevice_read (void)
{
	do {
		handle_msgpump ();
		idev[IDTYPE_MOUSE].read ();
		idev[IDTYPE_JOYSTICK].read ();
		idev[IDTYPE_KEYBOARD].read ();
	} while (handle_msgpump ());
}

static uae_u16 getjoystate (int joy)
{
	uae_u16 v;

	v = (uae_u8)mouse_x[joy] | (mouse_y[joy] << 8);
	return v;
}

uae_u16 JOY0DAT (void)
{
	uae_u16 v;
	readinput ();
	v = getjoystate (0);
  return v;
}

uae_u16 JOY1DAT (void)
{
	uae_u16 v;
	readinput ();
	v = getjoystate (1);
  return v;
}

uae_u16 JOYGET (int num)
{
	uae_u16 v;
	v = getjoystate (num);
	return v;
}

void JOYSET (int num, uae_u16 dat)
{
	mouse_x[num] = dat & 0xff;
	mouse_y[num] = (dat >> 8) & 0xff;
	mouse_frame_x[num] = mouse_x[num];
	mouse_frame_y[num] = mouse_y[num];
}

void JOYTEST (uae_u16 v)
{
	mouse_x[0] &= 3;
	mouse_y[0] &= 3;
	mouse_x[1] &= 3;
	mouse_y[1] &= 3;
	mouse_x[0] |= v & 0xFC;
	mouse_x[1] |= v & 0xFC;
	mouse_y[0] |= (v >> 8) & 0xFC;
	mouse_y[1] |= (v >> 8) & 0xFC;
	mouse_frame_x[0] = mouse_x[0];
	mouse_frame_y[0] = mouse_y[0];
	mouse_frame_x[1] = mouse_x[1];
	mouse_frame_y[1] = mouse_y[1];
}

#ifndef INPUTDEVICE_SIMPLE
static uae_u8 parconvert (uae_u8 v, int jd, int shift)
{
	if (jd & DIR_UP)
		v &= ~(1 << shift);
	if (jd & DIR_DOWN)
		v &= ~(2 << shift);
	if (jd & DIR_LEFT)
		v &= ~(4 << shift);
	if (jd & DIR_RIGHT)
		v &= ~(8 << shift);
	return v;
}

/* io-pins floating: dir=1 -> return data, dir=0 -> always return 1 */
uae_u8 handle_parport_joystick (int port, uae_u8 pra, uae_u8 dra)
{
	uae_u8 v;
	switch (port)
	{
	case 0:
		v = (pra & dra) | (dra ^ 0xff);
		if (parport_joystick_enabled) {
			v = parconvert (v, joydir[2], 0);
			v = parconvert (v, joydir[3], 4);
		}
		return v;
	case 1:
		v = ((pra & dra) | (dra ^ 0xff)) & 0x7;
		if (parport_joystick_enabled) {
			if (getbuttonstate (2, 0))
				v &= ~4;
			if (getbuttonstate (3, 0))
				v &= ~1;
			if (getbuttonstate (2, 1) || getbuttonstate (3, 1))
				v &= ~2; /* spare */
		}
		return v;
	default:
		return 0;
	}
}
#endif

/* p5 is 1 or floating = cd32 2-button mode */
static bool cd32padmode (uae_u16 p5dir, uae_u16 p5dat)
{
	if (!(potgo_value & p5dir) || ((potgo_value & p5dat) && (potgo_value & p5dir)))
		return false;
	return true;
}

#ifndef INPUTDEVICE_SIMPLE
static bool is_joystick_pullup (int joy)
{
	return joymodes[joy] == JSEM_MODE_GAMEPAD;
}

static void charge_cap (int joy, int idx, int charge)
{
	if (charge < -1 || charge > 1)
		charge = charge * 80;
	pot_cap[joy][idx] += charge;
	if (pot_cap[joy][idx] < 0)
		pot_cap[joy][idx] = 0;
	if (pot_cap[joy][idx] > 511)
		pot_cap[joy][idx] = 511;
}
#endif

static void cap_check (void)
{
	int joy, i;

	for (joy = 0; joy < 2; joy++) {
		for (i = 0; i < 2; i++) {
#ifndef INPUTDEVICE_SIMPLE
			int charge = 0, joypot;
#endif
			uae_u16 pdir = 0x0200 << (joy * 4 + i * 2); /* output enable */
			uae_u16 pdat = 0x0100 << (joy * 4 + i * 2); /* data */
			uae_u16 p5dir = 0x0200 << (joy * 4);
			uae_u16 p5dat = 0x0100 << (joy * 4);
			int isbutton = getbuttonstate (joy, i == 0 ? JOYBUTTON_3 : JOYBUTTON_2);

			if (cd32_pad_enabled[joy]) {
				// only red and blue can be read if CD32 pad and only if it is in normal pad mode
				isbutton |= getbuttonstate (joy, JOYBUTTON_CD32_BLUE);
				// CD32 pad 3rd button line (P5) is always floating
				if (i == 0)
					isbutton = 0;
				if (cd32padmode (p5dir, p5dat))
					continue;
			}

#ifndef INPUTDEVICE_SIMPLE
			joypot = joydirpot[joy][i];
			if (analog_port[joy][i] && pot_cap[joy][i] < joypot)
				charge = 1; // slow charge via pot variable resistor
			if ((is_joystick_pullup (joy) && digital_port[joy][i]) || (mouse_port[joy]))
				charge = 1; // slow charge via pull-up resistor
#endif
			if (!(potgo_value & pdir)) { // input?
				if (pot_dat_act[joy][i])
					pot_dat[joy][i]++;
				/* first 7 or 8 lines after potgo has been started = discharge cap */
				if (pot_dat_act[joy][i] == 1) {
					if (pot_dat[joy][i] < (currprefs.ntscmode ? POTDAT_DELAY_NTSC : POTDAT_DELAY_PAL)) {
#ifndef INPUTDEVICE_SIMPLE
						charge = -2; /* fast discharge delay */
#endif
          } else {
						pot_dat_act[joy][i] = 2;
						pot_dat[joy][i] = 0;
					}
				}
#ifndef INPUTDEVICE_SIMPLE
				if (analog_port[joy][i] && pot_dat_act[joy][i] == 2 && pot_cap[joy][i] >= joypot)
					pot_dat_act[joy][i] = 0;
#endif
				if ((digital_port[joy][i] || mouse_port[joy]) && pot_dat_act[joy][i] == 2) {
#ifdef INPUTDEVICE_SIMPLE
					if (!isbutton)
#else
					if (pot_cap[joy][i] >= 10 && !isbutton)
#endif
						pot_dat_act[joy][i] = 0;
				}
			} else { // output?
#ifndef INPUTDEVICE_SIMPLE
				charge = (potgo_value & pdat) ? 2 : -2; /* fast (dis)charge if output */
#endif
				if (potgo_value & pdat)
					pot_dat_act[joy][i] = 0; // instant stop if output+high
				if (isbutton)
					pot_dat[joy][i]++; // "free running" if output+low
			}

#ifndef INPUTDEVICE_SIMPLE
			if (isbutton)
				charge = -2; // button press overrides everything

			if (currprefs.cs_cdtvcd) {
				/* CDTV P9 is not floating */
				if (!(potgo_value & pdir) && i == 1 && charge == 0)
					charge = 2;
			}

			// CD32 pad in 2-button mode: blue button is not floating
			if (cd32_pad_enabled[joy] && i == 1 && charge == 0)
				charge = 2;
		
			/* official Commodore mouse has pull-up resistors in button lines
			* NOTE: 3rd party mice may not have pullups! */
			if ((mouse_port[joy] && digital_port[joy][i]) && charge == 0)
				charge = 2;
			/* emulate pullup resistor if button mapped because there too many broken
			* programs that read second button in input-mode (and most 2+ button pads have
			* pullups)
			*/
			if ((is_joystick_pullup (joy) && digital_port[joy][i]) && charge == 0)
				charge = 2;

			charge_cap (joy, i, charge);
#endif
		}
	}
}

uae_u8 handle_joystick_buttons (uae_u8 pra, uae_u8 dra)
{
  uae_u8 but = 0;
	int i;

	cap_check ();
	for (i = 0; i < 2; i++) {
		int mask = 0x40 << i;
		if (cd32_pad_enabled[i]) {
			uae_u16 p5dir = 0x0200 << (i * 4);
			uae_u16 p5dat = 0x0100 << (i * 4);
			but |= mask;
			if (!cd32padmode (p5dir, p5dat)) {
				if (getbuttonstate (i, JOYBUTTON_CD32_RED) || getbuttonstate (i, JOYBUTTON_1))
					but &= ~mask;
			}
		} else {
			if (!getbuttonstate (i, JOYBUTTON_1))
				but |= mask;
			if (dra & mask)
				but = (but & ~mask) | (pra & mask);
		}
	}

  return but;
}

/* joystick 1 button 1 is used as a output for incrementing shift register */
void handle_cd32_joystick_cia (uae_u8 pra, uae_u8 dra)
{
	static int oldstate[2];
	int i;

	cap_check ();
	for (i = 0; i < 2; i++) {
		uae_u8 but = 0x40 << i;
		uae_u16 p5dir = 0x0200 << (i * 4); /* output enable P5 */
		uae_u16 p5dat = 0x0100 << (i * 4); /* data P5 */
		if (cd32padmode (p5dir, p5dat)) {
			if ((dra & but) && (pra & but) != oldstate[i]) {
				if (!(pra & but)) {
					cd32_shifter[i]--;
					if (cd32_shifter[i] < 0)
						cd32_shifter[i] = 0;
				}
			}
		}
		oldstate[i] = dra & pra & but;
	}
}

/* joystick port 1 button 2 is input for button state */
static uae_u16 handle_joystick_potgor (uae_u16 potgor)
{
	int i;

	cap_check ();
	for (i = 0; i < 2; i++) {
		uae_u16 p9dir = 0x0800 << (i * 4); /* output enable P9 */
		uae_u16 p9dat = 0x0400 << (i * 4); /* data P9 */
		uae_u16 p5dir = 0x0200 << (i * 4); /* output enable P5 */
		uae_u16 p5dat = 0x0100 << (i * 4); /* data P5 */

		if (cd32_pad_enabled[i] && cd32padmode (p5dir, p5dat)) {

			/* p5 is floating in input-mode */
			potgor &= ~p5dat;
			potgor |= potgo_value & p5dat;
			if (!(potgo_value & p9dir))
				potgor |= p9dat;
			/* (P5 output and 1) or floating -> shift register is kept reset (Blue button) */
			if (!(potgo_value & p5dir) || ((potgo_value & p5dat) && (potgo_value & p5dir)))
				cd32_shifter[i] = 8;
			/* shift at 1 == return one, >1 = return button states */
			if (cd32_shifter[i] == 0)
				potgor &= ~p9dat; /* shift at zero == return zero */
			if (cd32_shifter[i] >= 2 && (joybutton[i] & ((1 << JOYBUTTON_CD32_PLAY) << (cd32_shifter[i] - 2))))
				potgor &= ~p9dat;

		} else  {

			potgor &= ~p5dat;
#ifdef INPUTDEVICE_SIMPLE
			if (getbuttonstate(i, JOYBUTTON_3) == 0)
#else
			if (pot_cap[i][0] > 100)
#endif
				potgor |= p5dat;

			if (!cd32_pad_enabled[i] || !cd32padmode (p5dir, p5dat)) {
				potgor &= ~p9dat;
#ifdef INPUTDEVICE_SIMPLE
        if(getbuttonstate(i, JOYBUTTON_2) == 0)
#else
				if (pot_cap[i][1] > 100)
#endif
				  potgor |= p9dat;
			}

		}
	}
	return potgor;
}

static void inject_events (const TCHAR *str)
{
	bool quot = false;
	bool first = true;
	uae_u8 keys[300];
	int keycnt = 0;

	for (;;) {
		TCHAR ch = *str++;
		if (!ch)
			break;

		if (ch == '\'') {
			first = false;
			quot = !quot;
			continue;
		}

		if (!quot && (ch == ' ' || first)) {
			const TCHAR *s = str;
			if (first)
				s--;
			while (*s == ' ')
				s++;
			const TCHAR *s2 = s;
			while (*s && *s != ' ')
				s++;
			int s2len = s - s2;
			if (!s2len)
				break;
			for (int i = 1; events[i].name; i++) {
				const TCHAR *cf = events[i].confname;
				if (!_tcsnicmp (cf, _T("KEY_"), 4))
					cf += 4;
				if (events[i].allow_mask == AM_K && !_tcsnicmp (cf, s2, _tcslen (cf)) && s2len == _tcslen (cf)) {
					int j;
					uae_u8 kc = events[i].data << 1;
					TCHAR tch = _totupper (s2[0]);
					if (tch != s2[0]) {
						// release
						for (j = 0; j < keycnt; j++) {
							if (keys[j] == kc)
								keys[j] = 0xff;
						}
						kc |= 0x01;
					} else {
						for (j = 0; j < keycnt; j++) {
							if (keys[j] == kc) {
								kc = 0xff;
							}
						}
						if (kc != 0xff) {
							for (j = 0; j < keycnt; j++) {
								if (keys[j] == 0xff) {
									keys[j] = kc;
									break;
								}
							}
							if (j == keycnt) {
								if (keycnt < sizeof keys)
									keys[keycnt++] = kc;
							}
						}
					}
					if (kc != 0xff) {
						//write_log (_T("%s\n"), cf);
						record_key (kc);
					}
				}
			}
		} else if (quot) {
			ch = _totupper (ch);
			if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
				for (int i = 1; events[i].name; i++) {
					if (events[i].allow_mask == AM_K && events[i].name[1] == 0 && events[i].name[0] == ch) {
						record_key (events[i].data << 1);
						record_key ((events[i].data << 1) | 0x01);
						//write_log (_T("%c\n"), ch);
					}
				}
			}
		}
		first = false;
	}
	while (--keycnt >= 0) {
		uae_u8 kc = keys[keycnt];
		if (kc != 0xff)
			record_key (kc | 0x01);
	}
}

void inputdevice_hsync (void)
{
	static int cnt;
	cap_check ();

	for (int i = 0; i < INPUT_QUEUE_SIZE; i++) {
		struct input_queue_struct *iq = &input_queue[i];
		if (iq->linecnt > 0) {
			iq->linecnt--;
			if (iq->linecnt == 0) {
				if (iq->state)
					iq->state = 0;
				else
					iq->state = iq->storedstate;
				if (iq->evt)
					handle_input_event (iq->evt, iq->state, iq->max, 0);
				iq->linecnt = iq->nextlinecnt;
			}
		}
	}

	if ((++cnt & 63) == 63 ) {
		inputdevice_read ();
	} else if (inputdelay > 0) {
		inputdelay--;
		if (inputdelay == 0)
			inputdevice_read ();
	}
}

static uae_u16 POTDAT (int joy)
{
	uae_u16 v = (pot_dat[joy][1] << 8) | pot_dat[joy][0];
	return v;
}

uae_u16 POT0DAT (void)
{
	return POTDAT (0);
}
uae_u16 POT1DAT (void)
{
	return POTDAT (1);
}

/* direction=input, data pin floating, last connected logic level or previous status
*                  written when direction was ouput
*                  otherwise it is currently connected logic level.
* direction=output, data pin is current value, forced to zero if joystick button is pressed
* it takes some tens of microseconds before data pin changes state
*/

void POTGO (uae_u16 v)
{
  int i, j;

	potgo_value = potgo_value & 0x5500; /* keep state of data bits */
	potgo_value |= v & 0xaa00; /* get new direction bits */
	for (i = 0; i < 8; i += 2) {
		uae_u16 dir = 0x0200 << i;
		if (v & dir) {
			uae_u16 data = 0x0100 << i;
			potgo_value &= ~data;
			potgo_value |= v & data;
		}
	}
	for (i = 0; i < 2; i++) {
		if (cd32_pad_enabled[i]) {
			uae_u16 p5dir = 0x0200 << (i * 4); /* output enable P5 */
			uae_u16 p5dat = 0x0100 << (i * 4); /* data P5 */
			if (!(potgo_value & p5dir) || ((potgo_value & p5dat) && (potgo_value & p5dir)))
				cd32_shifter[i] = 8;
		}
	}
	if (v & 1) {
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				pot_dat_act[i][j] = 1;
				pot_dat[i][j] = 0;
			}
		}
	}
}

uae_u16 POTGOR (void)
{
	uae_u16 v;

	v = handle_joystick_potgor (potgo_value) & 0x5500;
  return v;
}

static int check_input_queue (int evt)
{
	struct input_queue_struct *iq;
	int i;
	for (i = 0; i < INPUT_QUEUE_SIZE; i++) {
		iq = &input_queue[i];
		if (iq->evt == evt && iq->linecnt >= 0)
			return i;
	}
	return -1;
}

static void queue_input_event (int evt, int state, int max, int linecnt)
{
	struct input_queue_struct *iq;
	int idx;

	if (!evt)
		return;
	idx = check_input_queue (evt);
	if (state < 0 && idx >= 0) {
		iq = &input_queue[idx];
		iq->nextlinecnt = -1;
		iq->linecnt = -1;
		iq->evt = 0;
		if (iq->state == 0 && evt > 0)
			handle_input_event (evt, 0, 1, 0);
	} else if (state >= 0 && idx < 0) {
		if (evt == 0)
			return;
		for (idx = 0; idx < INPUT_QUEUE_SIZE; idx++) {
			iq = &input_queue[idx];
			if (iq->linecnt < 0)
				break;
		}
		if (idx == INPUT_QUEUE_SIZE) {
			write_log (_T("input queue overflow\n"));
			return;
		}
		iq->evt = evt;
		iq->state = iq->storedstate = state;
		iq->max = max;
		iq->linecnt = linecnt < 0 ? maxvpos + maxvpos / 2 : linecnt;
		iq->nextlinecnt = linecnt;
	}
}

static uae_u8 keybuf[256];
#define MAX_PENDING_EVENTS 20
static int inputcode_pending[MAX_PENDING_EVENTS], inputcode_pending_state[MAX_PENDING_EVENTS];

void inputdevice_add_inputcode (int code, int state)
{
	for (int i = 0; i < MAX_PENDING_EVENTS; i++) {
		if (inputcode_pending[i] == code && inputcode_pending_state[i] == state)
			return;
	}
	for (int i = 0; i < MAX_PENDING_EVENTS; i++) {
		if (inputcode_pending[i] == 0) {
		  inputcode_pending[i] = code;
		  inputcode_pending_state[i] = state;
			return;
		}
	}
}

void inputdevice_do_keyboard (int code, int state)
{
#ifdef CDTV
	if (code >= 0x72 && code <= 0x77) { // CDTV keys
		if (cdtv_front_panel (-1)) {
			// front panel active
			if (!state)
				return;
			cdtv_front_panel (code - 0x72);
			return;
		}
	}
#endif
	if (code < 0x80) {
		uae_u8 key = code | (state ? 0x00 : 0x80);
		keybuf[key & 0x7f] = (key & 0x80) ? 0 : 1;
		if (record_key ((uae_u8)((key << 1) | (key >> 7)))) {
		}
		return;
	}
	inputdevice_add_inputcode (code, state);
}

static bool inputdevice_handle_inputcode2 (int code, int state)
{
	if (code == 0)
		goto end;

	switch (code)
	{
	case AKS_ENTERGUI:
		gui_display (-1);
		break;
#ifdef ACTION_REPLAY
	case AKS_FREEZEBUTTON:
		action_replay_freeze ();
		break;
#endif
	case AKS_QUIT:
		uae_quit ();
		break;
	case AKS_SOFTRESET:
		uae_reset (0, 0);
		break;
	case AKS_HARDRESET:
		uae_reset (1, 1);
		break;
#ifdef CDTV
	case AKS_CDTV_FRONT_PANEL_STOP:
	case AKS_CDTV_FRONT_PANEL_PLAYPAUSE:
	case AKS_CDTV_FRONT_PANEL_PREV:
	case AKS_CDTV_FRONT_PANEL_NEXT:
	case AKS_CDTV_FRONT_PANEL_REW:
	case AKS_CDTV_FRONT_PANEL_FF:
		cdtv_front_panel (code - AKS_CDTV_FRONT_PANEL_STOP);
	break;
#endif
  }
end:
	return false;
}

void inputdevice_handle_inputcode (void)
{
	bool got = false;
	for (int i = 0; i < MAX_PENDING_EVENTS; i++) {
		int code = inputcode_pending[i];
		int state = inputcode_pending_state[i];
		if (code) {
			if (!inputdevice_handle_inputcode2 (code, state))
				inputcode_pending[i] = 0;
			got = true;
		}
	}
	if (!got)
		inputdevice_handle_inputcode2 (0, 0);
}


static int getqualid (int evt)
{
	if (evt > INPUTEVENT_SPC_QUALIFIER_START && evt < INPUTEVENT_SPC_QUALIFIER_END)
		return evt - INPUTEVENT_SPC_QUALIFIER1;
	return -1;
}

static uae_u64 isqual (int evt)
{
	int num = getqualid (evt);
	if (num < 0)
		return 0;
	return ID_FLAG_QUALIFIER1 << (num * 2);
}

static int handle_input_event (int nr, int state, int max, int autofire)
{
	const struct inputevent *ie;
	int joy;
	bool isaks = false;

	if (nr <= 0 || nr == INPUTEVENT_SPC_CUSTOM_EVENT)
		return 0;

#ifdef _WIN32
	// ignore normal GUI event if forced gui key is in use
	if (currprefs.win32_guikey >= 0 && nr == INPUTEVENT_SPC_ENTERGUI)
		return 0;
#endif

	ie = &events[nr];
	if (isqual (nr))
		return 0; // qualifiers do nothing
	if (ie->unit == 0 && ie->data >= AKS_FIRST) {
		isaks = true;
	}

	if (autofire) {
		if (state)
			queue_input_event (nr, state, max, currprefs.input_autofire_linecnt);
		else
			queue_input_event (nr, -1, 0, 0);
	}
	switch (ie->unit)
	{
	case 1: /* ->JOY1 */
	case 2: /* ->JOY2 */
	case 3: /* ->Parallel port joystick adapter port #1 */
	case 4: /* ->Parallel port joystick adapter port #2 */
		joy = ie->unit - 1;
		if (ie->type & 4) {
			int old = joybutton[joy] & (1 << ie->data);

			if (state) {
				joybutton[joy] |= 1 << ie->data;
			} else {
				joybutton[joy] &= ~(1 << ie->data);
			}

		} else if (ie->type & 8) {

			/* real mouse / analog stick mouse emulation */
			int delta;
			int deadzone = max < 0 ? 0 : currprefs.input_joymouse_deadzone * max / 100;
			int unit = ie->data & 0x7f;

			if (max) {
				if (state <= deadzone && state >= -deadzone) {
					state = 0;
					mouse_deltanoreset[joy][unit] = 0;
				} else if (state < 0) {
					state += deadzone;
					mouse_deltanoreset[joy][unit] = 1;
				} else {
					state -= deadzone;
					mouse_deltanoreset[joy][unit] = 1;
				}
				if (max > 0) {
				  max -= deadzone;
				  delta = state * currprefs.input_joymouse_multiplier / max;
			  } else {
				  delta = state;
				}
			} else {
				delta = state;
				mouse_deltanoreset[joy][unit] = 0;
			}
			if (ie->data & IE_CDTV) {
				delta = 0;
				if (state > 0)
					delta = JOYMOUSE_CDTV;
				else if (state < 0)
					delta = -JOYMOUSE_CDTV;
			}

			if (ie->data & IE_INVERT)
				delta = -delta;

			if (max)
				mouse_delta[joy][unit] = delta;
			else
				mouse_delta[joy][unit] += delta;

			max = 32;
		} else if (ie->type & 32) { /* button mouse emulation vertical */

			int speed = (ie->data & IE_CDTV) ? JOYMOUSE_CDTV : currprefs.input_joymouse_speed;

			if (state && (ie->data & DIR_UP)) {
				mouse_delta[joy][1] = -speed;
				mouse_deltanoreset[joy][1] = 1;
			} else if (state && (ie->data & DIR_DOWN)) {
				mouse_delta[joy][1] = speed;
				mouse_deltanoreset[joy][1] = 1;
			} else
				mouse_deltanoreset[joy][1] = 0;

		} else if (ie->type & 64) { /* button mouse emulation horizontal */

			int speed = (ie->data & IE_CDTV) ? JOYMOUSE_CDTV : currprefs.input_joymouse_speed;

			if (state && (ie->data & DIR_LEFT)) {
				mouse_delta[joy][0] = -speed;
				mouse_deltanoreset[joy][0] = 1;
			} else if (state && (ie->data & DIR_RIGHT)) {
				mouse_delta[joy][0] = speed;
				mouse_deltanoreset[joy][0] = 1;
			} else
				mouse_deltanoreset[joy][0] = 0;

		} else if (ie->type & 128) { /* analog joystick / paddle */

#ifndef INPUTDEVICE_SIMPLE
			int deadzone = currprefs.input_joymouse_deadzone * max / 100;
			int unit = ie->data & 0x7f;
			if (max) {
				if (state <= deadzone && state >= -deadzone) {
					state = 0;
				} else if (state < 0) {
					state += deadzone;
				} else {
					state -= deadzone;
				}
				state = state * max / (max - deadzone);
			} else {
				max = 100;
				relativecount[joy][unit] += state;
				state = relativecount[joy][unit];
				if (state < -max)
					state = -max;
				if (state > max)
					state = max;
				relativecount[joy][unit] = state;
			}

			if (ie->data & IE_INVERT)
				state = -state;

			state = state * currprefs.input_analog_joystick_mult / max;
			state += (128 * currprefs.input_analog_joystick_mult / 100) + currprefs.input_analog_joystick_offset;
			if (state < 0)
				state = 0;
			if (state > 255)
				state = 255;
			joydirpot[joy][unit] = state;
			mouse_deltanoreset[joy][0] = 1;
			mouse_deltanoreset[joy][1] = 1;
#endif

		} else {

			int left = oleft[joy], right = oright[joy], top = otop[joy], bot = obot[joy];
			if (ie->type & 16) {
				/* button to axis mapping */
				if (ie->data & DIR_LEFT) {
					left = oleft[joy] = state ? 1 : 0;
				}
				if (ie->data & DIR_RIGHT) {
					right = oright[joy] = state ? 1 : 0;
				}
				if (ie->data & DIR_UP) {
					top = otop[joy] = state ? 1 : 0;
				}
				if (ie->data & DIR_DOWN) {
					bot = obot[joy] = state ? 1 : 0;
				}
			} else {
				/* "normal" joystick axis */
				int deadzone = currprefs.input_joystick_deadzone * max / 100;
				int neg, pos;
				if (max == 0) {
					int cnt;
					int mmax = 50, mextra = 10;
					int unit = (ie->data & (4 | 8)) ? 1 : 0;
					// relative events
					relativecount[joy][unit] += state;
					cnt = relativecount[joy][unit];
					neg = cnt < -mmax;	
					pos = cnt > mmax;
					if (cnt < -(mmax + mextra))
						cnt = -(mmax + mextra);
					if (cnt > (mmax + mextra))
						cnt = (mmax + mextra);
					relativecount[joy][unit] = cnt;
				} else {
				  if (state < deadzone && state > -deadzone)
					  state = 0;
				  neg = state < 0 ? 1 : 0;
				  pos = state > 0 ? 1 : 0;
				}
				if (ie->data & DIR_LEFT) {
					left = oleft[joy] = neg;
				}
				if (ie->data & DIR_RIGHT) {
					right = oright[joy] = pos;
				}
				if (ie->data & DIR_UP) {
					top = otop[joy] = neg;
				}
				if (ie->data & DIR_DOWN) {
					bot = obot[joy] = pos;
				}
			}
			mouse_deltanoreset[joy][0] = 1;
			mouse_deltanoreset[joy][1] = 1;
			joydir[joy] = 0;
			if (left)
				joydir[joy] |= DIR_LEFT;
			if (right)
				joydir[joy] |= DIR_RIGHT;
			if (top)
				joydir[joy] |= DIR_UP;
			if (bot)
				joydir[joy] |= DIR_DOWN;
			if (joy == 0 || joy == 1)
				joymousecounter (joy); 
		}
		break;
	case 0: /* ->KEY */
		inputdevice_do_keyboard (ie->data, state);
		break;
  }
	return 1;
}

static void inputdevice_checkconfig (void)
{
	bool changed = false;
	for (int i = 0; i < MAX_JPORTS; i++) {
		if (currprefs.jports[i].id != changed_prefs.jports[i].id ||
			currprefs.jports[i].mode != changed_prefs.jports[i].mode)
				changed = true;
	}

	if (changed ||
		currprefs.input_selected_setting != changed_prefs.input_selected_setting ||
		currprefs.input_joymouse_multiplier != changed_prefs.input_joymouse_multiplier ||
		currprefs.input_joymouse_deadzone != changed_prefs.input_joymouse_deadzone ||
		currprefs.input_joystick_deadzone != changed_prefs.input_joystick_deadzone ||
		currprefs.input_joymouse_speed != changed_prefs.input_joymouse_speed ||
		currprefs.input_autofire_linecnt != changed_prefs.input_autofire_linecnt ||
		currprefs.input_mouse_speed != changed_prefs.input_mouse_speed) {

			currprefs.input_selected_setting = changed_prefs.input_selected_setting;
			currprefs.input_joymouse_multiplier = changed_prefs.input_joymouse_multiplier;
			currprefs.input_joymouse_deadzone = changed_prefs.input_joymouse_deadzone;
			currprefs.input_joystick_deadzone = changed_prefs.input_joystick_deadzone;
			currprefs.input_joymouse_speed = changed_prefs.input_joymouse_speed;
			currprefs.input_autofire_linecnt = changed_prefs.input_autofire_linecnt;
			currprefs.input_mouse_speed = changed_prefs.input_mouse_speed;

			inputdevice_updateconfig (&changed_prefs, &currprefs);
	}
}

void inputdevice_vsync (void)
{
	input_frame++;
	mouseupdate (0, true);

	inputdevice_read ();
	inputdelay = uaerand () % (maxvpos <= 1 ? 1 : maxvpos - 1);

	inputdevice_handle_inputcode ();
	if (mousehack_alive_cnt > 0) {
		mousehack_alive_cnt--;
	} else if (mousehack_alive_cnt < 0) {
		mousehack_alive_cnt++;
		if (mousehack_alive_cnt == 0) {
			mousehack_alive_cnt = 100;
		}
	}
	inputdevice_checkconfig ();
}

void inputdevice_reset (void)
{
  mousehack_reset ();
	if (inputdevice_is_tablet ())
		mousehack_enable ();
}

static int getoldport (struct uae_input_device *id)
{
	int i, j;

	for (i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			int evt = id->eventid[i][j];
			if (evt > 0) {
				int unit = events[evt].unit;
				if (unit >= 1 && unit <= 4)
					return unit;
			}
		}
	}
	return -1;
}

static int switchdevice (struct uae_input_device *id, int num, bool buttonmode)
{
	int ismouse = 0;
	int newport = 0;
	int newslot = -1;
	int flags = 0;
	TCHAR *name = NULL, *fname = NULL;
	int otherbuttonpressed = 0;
	int acc = input_acquired;

	if (num >= 4)
		return 0;
	if (!target_can_autoswitchdevice())
		return 0;

	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		if (id == &joysticks[i]) {
			name = idev[IDTYPE_JOYSTICK].get_uniquename (i);
			fname = idev[IDTYPE_JOYSTICK].get_friendlyname (i);
			newport = num == 0 ? 1 : 0;
			flags = idev[IDTYPE_JOYSTICK].get_flags (i);
			for (int j = 0; j < MAX_INPUT_DEVICES; j++) {
				if (j != i) {
					struct uae_input_device2 *id2 = &joysticks2[j];
					if (id2->buttonmask)
						otherbuttonpressed = 1;
				}
			}
		}
		if (id == &mice[i]) {
			ismouse = 1;
			name = idev[IDTYPE_MOUSE].get_uniquename (i);
			fname = idev[IDTYPE_MOUSE].get_friendlyname (i);
			newport = num == 0 ? 0 : 1;
			flags = idev[IDTYPE_MOUSE].get_flags (i);
		}
	}
	if (!name) {
		return 0;
	}
	if (buttonmode) {
		if (num == 0 && otherbuttonpressed)
			newport = newport ? 0 : 1;
	} else {
		newport = num ? 1 : 0;
	}
	/* "GamePorts" switch if in GamePorts mode or Input mode and GamePorts port was not NONE */
	if (currprefs.input_selected_setting == GAMEPORT_INPUT_SETTINGS || currprefs.jports[newport].id != JPORT_NONE) {
		if ((num == 0 || num == 1)) {
			bool issupermouse = false;
			int om = jsem_ismouse (num, &currprefs);
			int om1 = jsem_ismouse (0, &currprefs);
			int om2 = jsem_ismouse (1, &currprefs);
			if ((om1 >= 0 || om2 >= 0) && ismouse) {
				return 0;
			}
			if (flags) {
				return 0;
			}

#if 1
			if (ismouse) {
				int nummouse = 0; // count number of non-supermouse mice
				int supermouse = -1;
				for (int i = 0; i < idev[IDTYPE_MOUSE].get_num (); i++) {
					if (!idev[IDTYPE_MOUSE].get_flags (i))
						nummouse++;
					else
						supermouse = i;
				}
				if (supermouse >= 0 && nummouse == 1) {
					TCHAR *oldname = name;
					name = idev[IDTYPE_MOUSE].get_uniquename (supermouse);
					fname = idev[IDTYPE_MOUSE].get_friendlyname(supermouse);
					issupermouse = true;
				}
			}
#endif
			inputdevice_unacquire ();

			if (currprefs.input_selected_setting != GAMEPORT_INPUT_SETTINGS && currprefs.jports[newport].id > JPORT_NONE) {
				// disable old device
				int devnum;
				devnum = jsem_ismouse(newport, &currprefs);
				if (devnum >= 0) {
					if (changed_prefs.mouse_settings[currprefs.input_selected_setting][devnum].enabled) {
						changed_prefs.mouse_settings[currprefs.input_selected_setting][devnum].enabled = false;
					}
				}
				for (int l = 0; l < idev[IDTYPE_MOUSE].get_num(); l++) {
					if (changed_prefs.mouse_settings[currprefs.input_selected_setting][l].enabled) {
						if (idev[IDTYPE_MOUSE].get_flags(l)) {
							issupermouse = true;
						}
					}
				}
				if (issupermouse) {
					// new mouse is supermouse, disable all other mouse devices
					for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
						changed_prefs.mouse_settings[currprefs.input_selected_setting][l].enabled = false;
					}
				}

				devnum = jsem_isjoy(newport, &currprefs);
				if (devnum >= 0) {
					if (changed_prefs.joystick_settings[currprefs.input_selected_setting][devnum].enabled) {
						changed_prefs.joystick_settings[currprefs.input_selected_setting][devnum].enabled = false;
					}
				}
			}

			if (newslot >= 0) {
				TCHAR cust[100];
				_stprintf(cust, _T("custom%d"), newslot);
				inputdevice_joyport_config(&changed_prefs, cust, cust, newport, -1, 0, true);
			} else {
				inputdevice_joyport_config (&changed_prefs, name, name, newport, -1, 1, true);
			}
			inputdevice_validate_jports (&changed_prefs, -1, NULL);
			inputdevice_copyconfig (&changed_prefs, &currprefs);
			if (acc)
				inputdevice_acquire (TRUE);
			return 1;
		}
		return 0;

	} else {
		int oldport = getoldport (id);
		int k, evt;
		const struct inputevent *ie, *ie2;

		if (flags)
			return 0;
		if (oldport <= 0) {
			return 0;
		}
		newport++;
		/* do not switch if switching mouse and any "supermouse" mouse enabled */
		if (ismouse) {
			for (int i = 0; i < MAX_INPUT_SETTINGS; i++) {
				if (mice[i].enabled && idev[IDTYPE_MOUSE].get_flags (i)) {
					return 0;
			  }
		  }
		}
		for (int i = 0; i < MAX_INPUT_SETTINGS; i++) {
			if (getoldport (&joysticks[i]) == newport) {
				joysticks[i].enabled = 0;
      }
			if (getoldport (&mice[i]) == newport) {
				mice[i].enabled = 0;
      }
		}   
		id->enabled = 1;
		for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
			for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
				evt = id->eventid[i][j];
				if (evt <= 0)
					continue;
				ie = &events[evt];
				if (ie->unit == oldport) {
					k = 1;
					while (events[k].confname) {
						ie2 = &events[k];
						if (ie2->type == ie->type && ie2->data == ie->data && ie2->allow_mask == ie->allow_mask && ie2->unit == newport) {
							id->eventid[i][j] = k;
							break;
						}
						k++;
					}
				} else if (ie->unit == newport) {
					k = 1;
					while (events[k].confname) {
						ie2 = &events[k];
						if (ie2->type == ie->type && ie2->data == ie->data && ie2->allow_mask == ie->allow_mask && ie2->unit == oldport) {
							id->eventid[i][j] = k;
							break;
						}
						k++;
					}
				}
			}
		}
		write_log (_T("inputdevice input change '%s':%d->%d\n"), name, num, newport);
		inputdevice_unacquire ();
		inputdevice_copyconfig (&currprefs, &changed_prefs);
		inputdevice_validate_jports (&changed_prefs, -1, NULL);
		inputdevice_copyconfig (&changed_prefs, &currprefs);
		if (acc)
			inputdevice_acquire (TRUE);
		return 1;
	}
	return 0;
}

uae_u64 input_getqualifiers (void)
{
	return qualifiers;
}

static bool checkqualifiers (int evt, uae_u64 flags, uae_u64 *qualmask, uae_s16 events[MAX_INPUT_SUB_EVENT_ALL])
{
	int i, j;
	int qualid = getqualid (evt);
	int nomatch = 0;
	bool isspecial = (qualifiers & (ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SPECIAL_R)) != 0;

	flags &= ID_FLAG_QUALIFIER_MASK;
	if (qualid >= 0 && events)
		qualifiers_evt[qualid] = events;
	/* special set and new qualifier pressed? do not sent it to Amiga-side */
	if ((qualifiers & (ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SPECIAL_R)) && qualid >= 0)
		return false;

	for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		if (qualmask[i])
			break;
	}
	if (i == MAX_INPUT_SUB_EVENT) {
		 // no qualifiers in any slot and no special = always match
		return isspecial == false;
	}

	// do we have any subevents with qualifier set?
	for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		for (j = 0; j < MAX_INPUT_QUALIFIERS; j++) {
			uae_u64 mask = (ID_FLAG_QUALIFIER1 | ID_FLAG_QUALIFIER1_R) << (j * 2);
			bool isqualmask = (qualmask[i] & mask) != 0;
			bool isqual = (qualifiers & mask) != 0;
			if (isqualmask != isqual) {
				nomatch++;
				break;
			}
		}
	}
	if (nomatch == MAX_INPUT_SUB_EVENT) {
		// no matched qualifiers in any slot
		// allow all slots without qualifiers
		// special = never accept
		if (isspecial)
	    return false;
		return flags ? false : true;
  }

	for (i = 0; i < MAX_INPUT_QUALIFIERS; i++) {
		uae_u64 mask = (ID_FLAG_QUALIFIER1 | ID_FLAG_QUALIFIER1_R) << (i * 2);
		bool isflags = (flags & mask) != 0;
		bool isqual = (qualifiers & mask) != 0;
		if (isflags != isqual)
			return false;
	}
	return true;
}

static void setqualifiers (int evt, int state)
{
	uae_u64 mask = isqual (evt);
	if (!mask)
		return;
	if (state)
		qualifiers |= mask;
	else
		qualifiers &= ~mask;
}

static uae_u64 getqualmask (uae_u64 *qualmask, struct uae_input_device *id, int num, bool *qualonly)
{
	uae_u64 mask = 0, mask2 = 0;
	for (int i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		int evt = id->eventid[num][i];
		mask |= id->flags[num][i];
		qualmask[i] = id->flags[num][i] & ID_FLAG_QUALIFIER_MASK;
		mask2 |= isqual (evt);
	}
	mask &= ID_FLAG_QUALIFIER_MASK;
	*qualonly = false;
	if (qualifiers & ID_FLAG_QUALIFIER_SPECIAL) {
		// ID_FLAG_QUALIFIER_SPECIAL already active and this event has one or more qualifiers configured
		*qualonly = mask2 != 0;
	}
	return mask;
}

static void setbuttonstateall (struct uae_input_device *id, struct uae_input_device2 *id2, int button, int buttonstate)
{
	static frame_time_t switchdevice_timeout;
	int i;
	uae_u32 mask = 1 << button;
	uae_u32 omask = id2 ? id2->buttonmask & mask : 0;
	uae_u32 nmask = (buttonstate ? 1 : 0) << button;
	uae_u64 qualmask[MAX_INPUT_SUB_EVENT];
	bool qualonly;
	bool doit = true;

	if (!id->enabled) {
		frame_time_t t = read_processor_time ();
		if (!t)
			t++;

		if (buttonstate) {
			switchdevice_timeout = t;
		} else {
			if (switchdevice_timeout) {
			  int port = button;
			  if (t - switchdevice_timeout >= syncbase) // 1s
				  port ^= 1;
			  switchdevice (id, port, true);
			}
			switchdevice_timeout = 0;
	  }
		return;
	}
	if (button >= ID_BUTTON_TOTAL)
		return;

	if (doit) {
	  getqualmask (qualmask, id, ID_BUTTON_OFFSET + button, &qualonly);

	  for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		  int sub = sublevdir[buttonstate == 0 ? 1 : 0][i];
		  uae_u64 *flagsp = &id->flags[ID_BUTTON_OFFSET + button][sub];
		  int evt = id->eventid[ID_BUTTON_OFFSET + button][sub];
		  uae_u64 flags = flagsp[0];
		  int autofire = (flags & ID_FLAG_AUTOFIRE) ? 1 : 0;
		  int toggle = (flags & ID_FLAG_TOGGLE) ? 1 : 0;
		  int inverttoggle = (flags & ID_FLAG_INVERTTOGGLE) ? 1 : 0;
		  int invert = (flags & ID_FLAG_INVERT) ? 1 : 0;
		  int setmode = (flags & ID_FLAG_SET_ONOFF) ? 1: 0;
		  int setval = (flags & ID_FLAG_SET_ONOFF_VAL) ? SET_ONOFF_ON_VALUE : SET_ONOFF_OFF_VALUE;
		  int state;

		  if (buttonstate < 0) {
			  state = buttonstate;
		  } else if (invert) {
			  state = buttonstate ? 0 : 1;
		  } else {
			  state = buttonstate;
		  }
		  if (setmode) {
			  if (state)
				  state = setval;
		  }

		  setqualifiers (evt, state > 0);

		  if (qualonly)
			  continue;

#ifndef INPUTDEVICE_SIMPLE
		  if (state < 0) {
			  if (!checkqualifiers (evt, flags, qualmask, NULL))
				  continue;
			  handle_input_event (evt, 1, 1, 0);
		  } else if (inverttoggle) {
			  /* pressed = firebutton, not pressed = autofire */
			  if (state) {
				  queue_input_event (evt, -1, 0, 0);
					handle_input_event (evt, 2, 1, 0);
			  } else {
					handle_input_event (evt, 2, 1, autofire);
			  }
		  } else if (toggle) {
			  if (!state)
				  continue;
			  if (omask & mask)
				  continue;
			  if (!checkqualifiers (evt, flags, qualmask, NULL))
				  continue;
			  *flagsp ^= ID_FLAG_TOGGLED;
			  int toggled = (*flagsp & ID_FLAG_TOGGLED) ? 2 : 0;
				handle_input_event (evt, toggled, 1, autofire);
		  } else {
#endif
		    if (!checkqualifiers (evt, flags, qualmask, NULL)) {
			    if (!state && !(flags & ID_FLAG_CANRELEASE)) {
					  if (!invert)
				      continue;
			    } else if (state) {
				    continue;
			    }
		    }
		    if (!state)
			    *flagsp &= ~ID_FLAG_CANRELEASE;
		    else
			    *flagsp |= ID_FLAG_CANRELEASE;
		    if ((omask ^ nmask) & mask) {
  				handle_input_event (evt, state, 1, autofire);
  		  }
#ifndef INPUTDEVICE_SIMPLE
    	}
#endif
	  }

		queue_input_event (-1, -1, 0, 0);
	}

	if (id2 && ((omask ^ nmask) & mask)) {
		if (buttonstate)
			id2->buttonmask |= mask;
		else
			id2->buttonmask &= ~mask;
	}
}


/* - detect required number of joysticks and mice from configuration data
* - detect if CD32 pad emulation is needed
* - detect device type in ports (mouse or joystick)
*/

static int iscd32 (int ei)
{
	if (ei >= INPUTEVENT_JOY1_CD32_FIRST && ei <= INPUTEVENT_JOY1_CD32_LAST) {
		cd32_pad_enabled[0] = 1;
		return 1;
	}
	if (ei >= INPUTEVENT_JOY2_CD32_FIRST && ei <= INPUTEVENT_JOY2_CD32_LAST) {
		cd32_pad_enabled[1] = 1;
		return 2;
	}
	return 0;
}

static int isparport (int ei)
{
#ifndef INPUTDEVICE_SIMPLE
	if (ei > INPUTEVENT_PAR_JOY1_START && ei < INPUTEVENT_PAR_JOY_END) {
		parport_joystick_enabled = 1;
		return 1;
	}
#endif
	return 0;
}

static int ismouse (int ei)
{
	if (ei >= INPUTEVENT_MOUSE1_FIRST && ei <= INPUTEVENT_MOUSE1_LAST) {
		mouse_port[0] = 1;
		return 1;
	}
	if (ei >= INPUTEVENT_MOUSE2_FIRST && ei <= INPUTEVENT_MOUSE2_LAST) {
		mouse_port[1] = 1;
		return 2;
	}
	return 0;
}

static int isanalog (int ei)
{
#ifndef INPUTDEVICE_SIMPLE
	if (ei == INPUTEVENT_JOY1_HORIZ_POT || ei == INPUTEVENT_JOY1_HORIZ_POT_INV) {
		analog_port[0][0] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY1_VERT_POT || ei == INPUTEVENT_JOY1_VERT_POT_INV) {
		analog_port[0][1] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY2_HORIZ_POT || ei == INPUTEVENT_JOY2_HORIZ_POT_INV) {
		analog_port[1][0] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY2_VERT_POT || ei == INPUTEVENT_JOY2_VERT_POT_INV) {
		analog_port[1][1] = 1;
		return 1;
	}
#endif
	return 0;
}

static int isdigitalbutton (int ei)
{
	if (ei == INPUTEVENT_JOY1_2ND_BUTTON) {
		digital_port[0][1] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY1_3RD_BUTTON) {
		digital_port[0][0] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY2_2ND_BUTTON) {
		digital_port[1][1] = 1;
		return 1;
	}
	if (ei == INPUTEVENT_JOY2_3RD_BUTTON) {
		digital_port[1][0] = 1;
		return 1;
	}
	return 0;
}

static void isqualifier (int ei)
{
}

static void check_enable(int ei)
{
	iscd32(ei);
	isparport(ei);
	ismouse(ei);
	isdigitalbutton(ei);
	isqualifier(ei);
}

static void scanevents (struct uae_prefs *p)
{
	int i, j, k, ei;
	const struct inputevent *e;
	int n_joy = idev[IDTYPE_JOYSTICK].get_num ();
	int n_mouse = idev[IDTYPE_MOUSE].get_num ();

	cd32_pad_enabled[0] = cd32_pad_enabled[1] = 0;
#ifndef INPUTDEVICE_SIMPLE
	parport_joystick_enabled = 0;
#endif
	mouse_port[0] = mouse_port[1] = 0;
	qualifiers = 0;

	for (i = 0; i < NORMAL_JPORTS; i++) {
		for (j = 0; j < 2; j++) {
			digital_port[i][j] = 0;
#ifndef INPUTDEVICE_SIMPLE
			analog_port[i][j] = 0;
			joydirpot[i][j] = 128 / (312 * 100 / currprefs.input_analog_joystick_mult) + (128 * currprefs.input_analog_joystick_mult / 100) + currprefs.input_analog_joystick_offset;
#endif
		}
	}

	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		use_joysticks[i] = 0;
		use_mice[i] = 0;
		for (k = 0; k < MAX_INPUT_SUB_EVENT; k++) {
			for (j = 0; j < ID_BUTTON_TOTAL; j++) {

				if ((joysticks[i].enabled && i < n_joy) || joysticks[i].enabled < 0) {
					ei = joysticks[i].eventid[ID_BUTTON_OFFSET + j][k];
					e = &events[ei];
					iscd32 (ei);
					isparport (ei);
					ismouse (ei);
					isdigitalbutton (ei);
					isqualifier (ei);
					if (joysticks[i].eventid[ID_BUTTON_OFFSET + j][k] > 0)
						use_joysticks[i] = 1;
				}
				if ((mice[i].enabled && i < n_mouse) || mice[i].enabled < 0) {
					ei = mice[i].eventid[ID_BUTTON_OFFSET + j][k];
					e = &events[ei];
					iscd32 (ei);
					isparport (ei);
					ismouse (ei);
					isdigitalbutton (ei);
					isqualifier (ei);
					if (mice[i].eventid[ID_BUTTON_OFFSET + j][k] > 0)
						use_mice[i] = 1;
				}

			}

			for (j = 0; j < ID_AXIS_TOTAL; j++) {

				if ((joysticks[i].enabled && i < n_joy) || joysticks[i].enabled < 0) {
					ei = joysticks[i].eventid[ID_AXIS_OFFSET + j][k];
					iscd32 (ei);
					isparport (ei);
					ismouse (ei);
					isanalog (ei);
					isdigitalbutton (ei);
					isqualifier (ei);
					if (ei > 0)
						use_joysticks[i] = 1;
				}
				if ((mice[i].enabled && i < n_mouse) || mice[i].enabled < 0) {
					ei = mice[i].eventid[ID_AXIS_OFFSET + j][k];
					iscd32 (ei);
					isparport (ei);
					ismouse (ei);
					isanalog (ei);
					isdigitalbutton (ei);
					isqualifier (ei);
					if (ei > 0)
						use_mice[i] = 1;
				}
			}
		}
	}
	memset (scancodeused, 0, sizeof scancodeused);
	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		use_keyboards[i] = 0;
		if (keyboards[i].enabled && i < idev[IDTYPE_KEYBOARD].get_num ()) {
			j = 0;
			while (j < MAX_INPUT_DEVICE_EVENTS && keyboards[i].extra[j] >= 0) {
				use_keyboards[i] = 1;
				for (k = 0; k < MAX_INPUT_SUB_EVENT; k++) {
					ei = keyboards[i].eventid[j][k];
					iscd32 (ei);
					isparport (ei);
					ismouse (ei);
					isdigitalbutton (ei);
					isqualifier (ei);
					if (ei > 0)
						scancodeused[i][keyboards[i].extra[j]] = ei;
				}
				j++;
			}
		}
	}
}

static const int axistable[] = {
	INPUTEVENT_MOUSE1_HORIZ, INPUTEVENT_MOUSE1_LEFT, INPUTEVENT_MOUSE1_RIGHT,
	INPUTEVENT_MOUSE1_VERT, INPUTEVENT_MOUSE1_UP, INPUTEVENT_MOUSE1_DOWN,
	INPUTEVENT_MOUSE2_HORIZ, INPUTEVENT_MOUSE2_LEFT, INPUTEVENT_MOUSE2_RIGHT,
	INPUTEVENT_MOUSE2_VERT, INPUTEVENT_MOUSE2_UP, INPUTEVENT_MOUSE2_DOWN,
	INPUTEVENT_JOY1_HORIZ, INPUTEVENT_JOY1_LEFT, INPUTEVENT_JOY1_RIGHT,
	INPUTEVENT_JOY1_VERT, INPUTEVENT_JOY1_UP, INPUTEVENT_JOY1_DOWN,
	INPUTEVENT_JOY2_HORIZ, INPUTEVENT_JOY2_LEFT, INPUTEVENT_JOY2_RIGHT,
	INPUTEVENT_JOY2_VERT, INPUTEVENT_JOY2_UP, INPUTEVENT_JOY2_DOWN,
	INPUTEVENT_PAR_JOY1_HORIZ, INPUTEVENT_PAR_JOY1_LEFT, INPUTEVENT_PAR_JOY1_RIGHT,
	INPUTEVENT_PAR_JOY1_VERT, INPUTEVENT_PAR_JOY1_UP, INPUTEVENT_PAR_JOY1_DOWN,
	INPUTEVENT_PAR_JOY2_HORIZ, INPUTEVENT_PAR_JOY2_LEFT, INPUTEVENT_PAR_JOY2_RIGHT,
	INPUTEVENT_PAR_JOY2_VERT, INPUTEVENT_PAR_JOY2_UP, INPUTEVENT_PAR_JOY2_DOWN,
	INPUTEVENT_MOUSE_CDTV_HORIZ, INPUTEVENT_MOUSE_CDTV_LEFT, INPUTEVENT_MOUSE_CDTV_RIGHT,
	INPUTEVENT_MOUSE_CDTV_VERT, INPUTEVENT_MOUSE_CDTV_UP, INPUTEVENT_MOUSE_CDTV_DOWN,
	-1
};

int intputdevice_compa_get_eventtype (int evt, const int **axistablep)
{
	for (int i = 0; axistable[i] >= 0; i += 3) {
		*axistablep = &axistable[i];
		if (axistable[i] == evt)
			return IDEV_WIDGET_AXIS;
		if (axistable[i + 1] == evt)
			return IDEV_WIDGET_BUTTONAXIS;
		if (axistable[i + 2] == evt)
			return IDEV_WIDGET_BUTTONAXIS;
	}
	*axistablep = NULL;
	return IDEV_WIDGET_BUTTON;
}

static const int rem_port1[] = {
	INPUTEVENT_MOUSE1_HORIZ, INPUTEVENT_MOUSE1_VERT,
	INPUTEVENT_JOY1_HORIZ, INPUTEVENT_JOY1_VERT,
#ifndef INPUTDEVICE_SIMPLE
	INPUTEVENT_JOY1_HORIZ_POT, INPUTEVENT_JOY1_VERT_POT,
#endif
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_2ND_BUTTON, INPUTEVENT_JOY1_3RD_BUTTON,
	INPUTEVENT_JOY1_CD32_RED, INPUTEVENT_JOY1_CD32_BLUE, INPUTEVENT_JOY1_CD32_GREEN, INPUTEVENT_JOY1_CD32_YELLOW,
	INPUTEVENT_JOY1_CD32_RWD, INPUTEVENT_JOY1_CD32_FFW, INPUTEVENT_JOY1_CD32_PLAY,
	INPUTEVENT_MOUSE_CDTV_HORIZ, INPUTEVENT_MOUSE_CDTV_VERT,
	-1
};
static const int rem_port2[] = {
	INPUTEVENT_MOUSE2_HORIZ, INPUTEVENT_MOUSE2_VERT,
	INPUTEVENT_JOY2_HORIZ, INPUTEVENT_JOY2_VERT,
#ifndef INPUTDEVICE_SIMPLE
	INPUTEVENT_JOY2_HORIZ_POT, INPUTEVENT_JOY2_VERT_POT,
#endif
	INPUTEVENT_JOY2_FIRE_BUTTON, INPUTEVENT_JOY2_2ND_BUTTON, INPUTEVENT_JOY2_3RD_BUTTON,
	INPUTEVENT_JOY2_CD32_RED, INPUTEVENT_JOY2_CD32_BLUE, INPUTEVENT_JOY2_CD32_GREEN, INPUTEVENT_JOY2_CD32_YELLOW,
	INPUTEVENT_JOY2_CD32_RWD, INPUTEVENT_JOY2_CD32_FFW, INPUTEVENT_JOY2_CD32_PLAY,
	-1, -1,
	-1, -1,
	-1
};
static const int rem_port3[] = {
	INPUTEVENT_PAR_JOY1_LEFT, INPUTEVENT_PAR_JOY1_RIGHT, INPUTEVENT_PAR_JOY1_UP, INPUTEVENT_PAR_JOY1_DOWN,
	INPUTEVENT_PAR_JOY1_FIRE_BUTTON, INPUTEVENT_PAR_JOY1_2ND_BUTTON,
	-1
};
static const int rem_port4[] = {
	INPUTEVENT_PAR_JOY2_LEFT, INPUTEVENT_PAR_JOY2_RIGHT, INPUTEVENT_PAR_JOY2_UP, INPUTEVENT_PAR_JOY2_DOWN,
	INPUTEVENT_PAR_JOY2_FIRE_BUTTON, INPUTEVENT_PAR_JOY2_2ND_BUTTON,
	-1
};

static const int *rem_ports[] = { rem_port1, rem_port2, rem_port3, rem_port4 };
static const int ip_joy1[] = {
	INPUTEVENT_JOY1_LEFT, INPUTEVENT_JOY1_RIGHT, INPUTEVENT_JOY1_UP, INPUTEVENT_JOY1_DOWN,
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_2ND_BUTTON,
	-1
};
static const int ip_joy2[] = {
	INPUTEVENT_JOY2_LEFT, INPUTEVENT_JOY2_RIGHT, INPUTEVENT_JOY2_UP, INPUTEVENT_JOY2_DOWN,
	INPUTEVENT_JOY2_FIRE_BUTTON, INPUTEVENT_JOY2_2ND_BUTTON,
	-1
};
static const int ip_joypad1[] = {
	INPUTEVENT_JOY1_LEFT, INPUTEVENT_JOY1_RIGHT, INPUTEVENT_JOY1_UP, INPUTEVENT_JOY1_DOWN,
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_2ND_BUTTON, INPUTEVENT_JOY1_3RD_BUTTON,
	-1
};
static const int ip_joypad2[] = {
	INPUTEVENT_JOY2_LEFT, INPUTEVENT_JOY2_RIGHT, INPUTEVENT_JOY2_UP, INPUTEVENT_JOY2_DOWN,
	INPUTEVENT_JOY2_FIRE_BUTTON, INPUTEVENT_JOY2_2ND_BUTTON, INPUTEVENT_JOY2_3RD_BUTTON,
	-1
};
static const int ip_joycd321[] = {
	INPUTEVENT_JOY1_LEFT, INPUTEVENT_JOY1_RIGHT, INPUTEVENT_JOY1_UP, INPUTEVENT_JOY1_DOWN,
	INPUTEVENT_JOY1_CD32_RED, INPUTEVENT_JOY1_CD32_BLUE, INPUTEVENT_JOY1_CD32_GREEN, INPUTEVENT_JOY1_CD32_YELLOW,
	INPUTEVENT_JOY1_CD32_RWD, INPUTEVENT_JOY1_CD32_FFW, INPUTEVENT_JOY1_CD32_PLAY,
	-1
};
static const int ip_joycd322[] = {
	INPUTEVENT_JOY2_LEFT, INPUTEVENT_JOY2_RIGHT, INPUTEVENT_JOY2_UP, INPUTEVENT_JOY2_DOWN,
	INPUTEVENT_JOY2_CD32_RED, INPUTEVENT_JOY2_CD32_BLUE, INPUTEVENT_JOY2_CD32_GREEN, INPUTEVENT_JOY2_CD32_YELLOW,
	INPUTEVENT_JOY2_CD32_RWD, INPUTEVENT_JOY2_CD32_FFW, INPUTEVENT_JOY2_CD32_PLAY,
	-1
};
static const int ip_parjoy1[] = {
	INPUTEVENT_PAR_JOY1_LEFT, INPUTEVENT_PAR_JOY1_RIGHT, INPUTEVENT_PAR_JOY1_UP, INPUTEVENT_PAR_JOY1_DOWN,
	INPUTEVENT_PAR_JOY1_FIRE_BUTTON, INPUTEVENT_PAR_JOY1_2ND_BUTTON,
	-1
};
static const int ip_parjoy2[] = {
	INPUTEVENT_PAR_JOY2_LEFT, INPUTEVENT_PAR_JOY2_RIGHT, INPUTEVENT_PAR_JOY2_UP, INPUTEVENT_PAR_JOY2_DOWN,
	INPUTEVENT_PAR_JOY2_FIRE_BUTTON, INPUTEVENT_PAR_JOY2_2ND_BUTTON,
	-1
};
static const int ip_parjoy1default[] = {
	INPUTEVENT_PAR_JOY1_LEFT, INPUTEVENT_PAR_JOY1_RIGHT, INPUTEVENT_PAR_JOY1_UP, INPUTEVENT_PAR_JOY1_DOWN,
	INPUTEVENT_PAR_JOY1_FIRE_BUTTON,
	-1
};
static const int ip_parjoy2default[] = {
	INPUTEVENT_PAR_JOY2_LEFT, INPUTEVENT_PAR_JOY2_RIGHT, INPUTEVENT_PAR_JOY2_UP, INPUTEVENT_PAR_JOY2_DOWN,
	INPUTEVENT_PAR_JOY2_FIRE_BUTTON,
	-1
};
static const int ip_mouse1[] = {
	INPUTEVENT_MOUSE1_LEFT, INPUTEVENT_MOUSE1_RIGHT, INPUTEVENT_MOUSE1_UP, INPUTEVENT_MOUSE1_DOWN,
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_2ND_BUTTON,
	-1
};
static const int ip_mouse2[] = {
	INPUTEVENT_MOUSE2_LEFT, INPUTEVENT_MOUSE2_RIGHT, INPUTEVENT_MOUSE2_UP, INPUTEVENT_MOUSE2_DOWN,
	INPUTEVENT_JOY2_FIRE_BUTTON, INPUTEVENT_JOY2_2ND_BUTTON,
	-1
};
static const int ip_mousecdtv[] =
{
	INPUTEVENT_MOUSE_CDTV_LEFT, INPUTEVENT_MOUSE_CDTV_RIGHT, INPUTEVENT_MOUSE_CDTV_UP, INPUTEVENT_MOUSE_CDTV_DOWN,
	INPUTEVENT_JOY1_FIRE_BUTTON, INPUTEVENT_JOY1_2ND_BUTTON,
	-1
};
static const int ip_mediacdtv[] =
{
	INPUTEVENT_KEY_CDTV_PLAYPAUSE, INPUTEVENT_KEY_CDTV_STOP, INPUTEVENT_KEY_CDTV_PREV, INPUTEVENT_KEY_CDTV_NEXT,
	-1
};
static const int ip_analog1[] = {
	INPUTEVENT_JOY1_HORIZ_POT, INPUTEVENT_JOY1_VERT_POT, INPUTEVENT_JOY1_LEFT, INPUTEVENT_JOY1_RIGHT,
	-1
};
static const int ip_analog2[] = {
	INPUTEVENT_JOY2_HORIZ_POT, INPUTEVENT_JOY2_VERT_POT, INPUTEVENT_JOY2_LEFT, INPUTEVENT_JOY2_RIGHT,
	-1
};

static void checkcompakb (int *kb, const int *srcmap)
{
	int found = 0, avail = 0;
	int j, k;

	k = j = 0;
	while (kb[j] >= 0) {
		struct uae_input_device *uid = &keyboards[0];
		while (kb[j] >= 0 && srcmap[k] >= 0) {
			int id = kb[j];
			for (int l = 0; l < MAX_INPUT_DEVICE_EVENTS; l++) {
				if (uid->extra[l] == id) {
					avail++;
					if (uid->eventid[l][0] == srcmap[k])
						found++;
					break;
				}
			}
			j++;
		}
		if (srcmap[k] < 0)
			break;
		j++;
		k++;
	}
	if (avail != found || avail == 0)
		return;
	k = j = 0;
	while (kb[j] >= 0) {
		struct uae_input_device *uid = &keyboards[0];
		while (kb[j] >= 0) {
			int id = kb[j];
			k = 0;
			while (keyboard_default[k].scancode >= 0) {
				if (keyboard_default[k].scancode == kb[j]) {
					for (int l = 0; l < MAX_INPUT_DEVICE_EVENTS; l++) {
						if (uid->extra[l] == id && uid->port[l][0] == 0) {
							for (int m = 0; m < MAX_INPUT_SUB_EVENT && keyboard_default[k].node[m].evt; m++) {
								uid->eventid[l][m] = keyboard_default[k].node[m].evt;
								uid->port[l][m] = 0;
								uid->flags[l][m] = 0;
							}
							break;
						}
					}
					break;
				}
				k++;
			}
			j++;
		}
		j++;
	}
}

static void inputdevice_sparerestore (struct uae_input_device *uid, int num, int sub)
{
	if (uid->port[num][SPARE_SUB_EVENT]) {
	  uid->eventid[num][sub] = uid->eventid[num][SPARE_SUB_EVENT];
	  uid->flags[num][sub] = uid->flags[num][SPARE_SUB_EVENT];
	  uid->custom[num][sub] = uid->custom[num][SPARE_SUB_EVENT];
	} else {
		uid->eventid[num][sub] = 0;
		uid->flags[num][sub] = 0;
		xfree (uid->custom[num][sub]);
		uid->custom[num][sub] = 0;
	}
	uid->eventid[num][SPARE_SUB_EVENT] = 0;
	uid->flags[num][SPARE_SUB_EVENT] = 0;
	uid->port[num][SPARE_SUB_EVENT] = 0;
	uid->custom[num][SPARE_SUB_EVENT] = 0;
	if (uid->flags[num][MAX_INPUT_SUB_EVENT - 1] & ID_FLAG_RESERVEDGAMEPORTSCUSTOM) {
		uid->eventid[num][MAX_INPUT_SUB_EVENT - 1] = 0;
		uid->flags[num][MAX_INPUT_SUB_EVENT - 1] = 0;
		uid->port[num][MAX_INPUT_SUB_EVENT - 1] = 0;
		uid->custom[num][MAX_INPUT_SUB_EVENT - 1] = 0;
	}
}

void inputdevice_sparecopy (struct uae_input_device *uid, int num, int sub)
{
	if (uid->port[num][SPARE_SUB_EVENT] != 0)
		return;
	if (uid->eventid[num][sub] <= 0 && uid->custom[num][sub] == NULL) {
		uid->eventid[num][SPARE_SUB_EVENT] = 0;
		uid->flags[num][SPARE_SUB_EVENT] = 0;
		uid->port[num][SPARE_SUB_EVENT] = 0;
		xfree (uid->custom[num][SPARE_SUB_EVENT]);
		uid->custom[num][SPARE_SUB_EVENT] = NULL;
	} else {
	  uid->eventid[num][SPARE_SUB_EVENT] = uid->eventid[num][sub];
	  uid->flags[num][SPARE_SUB_EVENT] = uid->flags[num][sub];
	  uid->port[num][SPARE_SUB_EVENT] = MAX_JPORTS + 1;
	  xfree (uid->custom[num][SPARE_SUB_EVENT]);
	  uid->custom[num][SPARE_SUB_EVENT] = uid->custom[num][sub];
	  uid->custom[num][sub] = NULL;
  }
}

static void setcompakb (struct uae_prefs *p, int *kb, const int *srcmap, int index, int af)
{
	int j, k;
	k = j = 0;
	while (kb[j] >= 0 && srcmap[k] >= 0) {
		while (kb[j] >= 0) {
			int id = kb[j];
			// default and active KB only
			for (int m = 0; m < MAX_INPUT_DEVICES; m++) {
				struct uae_input_device *uid = &keyboards[m];
				if (m == 0 || uid->enabled) {
				  for (int l = 0; l < MAX_INPUT_DEVICE_EVENTS; l++) {
					  if (uid->extra[l] == id) {
							  setcompakbevent(p, uid, l, srcmap[k], index, af);
						  break;
						}
					}
				}
			}
			j++;
		}
		j++;
		k++;
	}
}

int inputdevice_get_compatibility_input (struct uae_prefs *prefs, int index, int *typelist, int *inputlist, const int **at)
{
	if (index >= MAX_JPORTS || joymodes[index] < 0)
		return -1;
	if (typelist != NULL)
	  *typelist = joymodes[index];
	if (at != NULL)
	  *at = axistable;
	if (inputlist == NULL)
		return -1;
	
	//write_log (_T("%d %p %p\n"), *typelist, *inputlist, *at);
	int cnt;
	for (cnt = 0; joyinputs[index] && joyinputs[index][cnt] >= 0; cnt++) {
		inputlist[cnt] = joyinputs[index][cnt];
	}
	inputlist[cnt] = -1;

	// find custom events (custom event = event that is mapped to same port but not included in joyinputs[]
	int devnum = 0;
	while (inputdevice_get_device_status (devnum) >= 0) {
		for (int j = 0; j < inputdevice_get_widget_num (devnum); j++) {
			for (int sub = 0; sub < MAX_INPUT_SUB_EVENT; sub++) {
				int port, k, l;
				uae_u64 flags;
				bool ignore = false;
				int evtnum2 = inputdevice_get_mapping (devnum, j, &flags, &port, NULL, NULL, sub);
				if (port - 1 != index)
					continue;
				for (k = 0; axistable[k] >= 0; k += 3) {
					if (evtnum2 == axistable[k] || evtnum2 == axistable[k + 1] || evtnum2 == axistable[k + 2]) {
						for (l = 0; inputlist[l] >= 0; l++) {
							if (inputlist[l] == axistable[k] || inputlist[l] == axistable[k + 1] || inputlist[l] == axistable[k + 1]) {
								ignore = true;
							}
						}
					}
				}
				if (!ignore) {
					for (k = 0; inputlist[k] >= 0; k++) {
						if (evtnum2 == inputlist[k])
							break;
					}
					if (inputlist[k] < 0) {
						inputlist[k] = evtnum2;
						inputlist[k + 1] = -1;
						cnt++;
					}
				}
			}
		}
		devnum++;
	}
	return cnt;
}

static void clearevent (struct uae_input_device *uid, int evt)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (uid->eventid[i][j] == evt) {
				uid->eventid[i][j] = 0;
				uid->flags[i][j] &= COMPA_RESERVED_FLAGS;
				xfree (uid->custom[i][j]);
				uid->custom[i][j] = NULL;
			}
		}
	}
}
static void clearkbrevent (struct uae_input_device *uid, int evt)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (uid->eventid[i][j] == evt) {
				uid->eventid[i][j] = 0;
				uid->flags[i][j] &= COMPA_RESERVED_FLAGS;
				xfree (uid->custom[i][j]);
				uid->custom[i][j] = NULL;
				if (j == 0)
					set_kbr_default_event (uid, keyboard_default, i);
			}
		}
	}
}

static void resetjport (struct uae_prefs *prefs, int index)
{
	const int *p = rem_ports[index];
	while (*p >= 0) {
		int evtnum = *p++;
		for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
			clearevent (&prefs->joystick_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
			clearevent (&prefs->mouse_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
			clearkbrevent (&prefs->keyboard_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
		}
		for (int i = 0; axistable[i] >= 0; i += 3) {
			if (evtnum == axistable[i] || evtnum == axistable[i + 1] || evtnum == axistable[i + 2]) {
				for (int j = 0; j < 3; j++) {
					int evtnum2 = axistable[i + j];
					for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
						clearevent (&prefs->joystick_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum2);
						clearevent (&prefs->mouse_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum2);
						clearkbrevent (&prefs->keyboard_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum2);
					}
				}
				break;
			}
		}
	}
}

static void remove_compa_config (struct uae_prefs *prefs, int index)
{
	int typelist;
	const int *atpp;
	int inputlist[MAX_COMPA_INPUTLIST];

	if (inputdevice_get_compatibility_input (prefs, index, &typelist, inputlist, &atpp) <= 0)
		return;
	for (int i = 0; inputlist[i] >= 0; i++) {
		int evtnum = inputlist[i];
		const int *atp = atpp;

		int atpidx = 0;
		while (*atp >= 0) {
			if (*atp == evtnum) {
				atp++;
				atpidx = 2;
				break;
			}
			if (atp[1] == evtnum || atp[2] == evtnum) {
				atpidx = 1;
				break;
			}
			atp += 3;
		}
		while (atpidx >= 0) {
			for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
				clearevent (&prefs->joystick_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
				clearevent (&prefs->mouse_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
				clearkbrevent (&prefs->keyboard_settings[GAMEPORT_INPUT_SETTINGS][l], evtnum);
			}
			evtnum = *atp++;
			atpidx--;
		}
	}
}

static void cleardev_custom (struct uae_input_device *uid, int num, int index)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (uid[num].port[i][j] == index + 1) {
				uid[num].eventid[i][j] = 0;
				uid[num].flags[i][j] &= COMPA_RESERVED_FLAGS;
				xfree (uid[num].custom[i][j]);
				uid[num].custom[i][j] = NULL;
				uid[num].port[i][j] = 0;
				if (uid[num].port[i][SPARE_SUB_EVENT])
					inputdevice_sparerestore (&uid[num], i, j);
			}
		}
	}
}
static void cleardevkbr_custom(struct uae_input_device *uid, int num, int index)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (uid[num].port[i][j] == index + 1) {
				uid[num].eventid[i][j] = 0;
				uid[num].flags[i][j] &= COMPA_RESERVED_FLAGS;
				xfree (uid[num].custom[i][j]);
				uid[num].custom[i][j] = NULL;
				uid[num].port[i][j] = 0;
				if (uid[num].port[i][SPARE_SUB_EVENT]) {
					inputdevice_sparerestore (&uid[num], i, j);
				} else if (j == 0) {
					set_kbr_default_event (&uid[num], keyboard_default, i);
				}
			}
		}
	}
}

// remove all gameports mappings mapped to port 'index'
static void remove_custom_config (struct uae_prefs *prefs, int index)
{
	for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
		cleardev_custom(joysticks, l, index);
		cleardev_custom(mice, l, index);
		cleardevkbr_custom (keyboards, l, index);
	}
}

// prepare port for custom mapping, remove all current Amiga side device mappings
void inputdevice_compa_prepare_custom (struct uae_prefs *prefs, int index, int newmode, bool removeold)
{
	int mode = prefs->jports[index].mode;
	if (newmode >= 0) {
		mode = newmode;
	} else if (mode == 0) {
		mode = index == 0 ? JSEM_MODE_WHEELMOUSE : (prefs->cs_cd32cd ? JSEM_MODE_JOYSTICK_CD32 : JSEM_MODE_JOYSTICK);
	}
	prefs->jports[index].mode = mode;

	if (removeold) {
	  remove_compa_config (prefs, index);
		remove_custom_config (prefs, index);
  }
}
// clear device before switching to new one
void inputdevice_compa_clear (struct uae_prefs *prefs, int index)
{
	freejport (prefs, index);
	resetjport (prefs, index);
	remove_compa_config (prefs, index);
}

static void cleardev (struct uae_input_device *uid, int num)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		inputdevice_sparecopy (&uid[num], i, 0);
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			uid[num].eventid[i][j] = 0;
			uid[num].flags[i][j] = 0;
			xfree (uid[num].custom[i][j]);
			uid[num].custom[i][j] = NULL;
		}
	}
}

static void enablejoydevice (struct uae_input_device *uid, bool gameportsmode, int evtnum)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if ((gameportsmode && uid->eventid[i][j] == evtnum) || uid->port[i][j] > 0) {
				if (!uid->enabled)
					uid->enabled = -1;
			}
		}
	}
}

static void setjoydevices (struct uae_prefs *prefs, bool gameportsmode, int port)
{
	for (int i = 0; joyinputs[port] && joyinputs[port][i] >= 0; i++) {
		int evtnum = joyinputs[port][i];
		for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
			enablejoydevice (&joysticks[l], gameportsmode, evtnum);
			enablejoydevice (&mice[l], gameportsmode, evtnum);
			//enablejoydevice (&keyboards[l], gameportsmode, evtnum);
		}
		for (int k = 0; axistable[k] >= 0; k += 3) {
			if (evtnum == axistable[k] || evtnum == axistable[k + 1] || evtnum == axistable[k + 2]) {
				for (int j = 0; j < 3; j++) {
					int evtnum2 = axistable[k + j];
					for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
						enablejoydevice (&joysticks[l], gameportsmode, evtnum2);
						enablejoydevice (&mice[l], gameportsmode, evtnum2);
						//enablejoydevice (&keyboards[l], gameportsmode, evtnum2);
					}
				}
				break;
			}
		}

	}
}

static void setjoyinputs (struct uae_prefs *prefs, int port)
{
	joyinputs[port] = NULL;
	switch (joymodes[port])
	{
		case JSEM_MODE_JOYSTICK:
			if (port >= 2)
				joyinputs[port] = port == 3 ? ip_parjoy2 : ip_parjoy1;
			else
			  joyinputs[port] = port == 1 ? ip_joy2 : ip_joy1;
		  break;
		case JSEM_MODE_GAMEPAD:
			joyinputs[port] = port ? ip_joypad2 : ip_joypad1;
		  break;
		case JSEM_MODE_JOYSTICK_CD32:
			joyinputs[port] = port ? ip_joycd322 : ip_joycd321;
		  break;
		case JSEM_MODE_JOYSTICK_ANALOG:
			joyinputs[port] = port ? ip_analog2 : ip_analog1;
		  break;
		case JSEM_MODE_WHEELMOUSE:
		case JSEM_MODE_MOUSE:
			joyinputs[port] = port ? ip_mouse2 : ip_mouse1;
		  break;
		case JSEM_MODE_MOUSE_CDTV:
			joyinputs[port] = ip_mousecdtv;
		  break;
	}
	//write_log (_T("joyinput %d = %p\n"), port, joyinputs[port]);
}

static void setautofire (struct uae_input_device *uid, int port, int af)
{
	const int *afp = af_ports[port];
	for (int k = 0; afp[k] >= 0; k++) {
		for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
			for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
				if (uid->eventid[i][j] == afp[k]) {
					uid->flags[i][j] &= ~ID_FLAG_AUTOFIRE_MASK;
					if (af >= JPORT_AF_NORMAL)
						uid->flags[i][j] |= ID_FLAG_AUTOFIRE;
#ifndef INPUTDEVICE_SIMPLE
					if (af == JPORT_AF_TOGGLE)
						uid->flags[i][j] |= ID_FLAG_TOGGLE;
					if (af == JPORT_AF_ALWAYS)
						uid->flags[i][j] |= ID_FLAG_INVERTTOGGLE;
#endif
				}
			}
		}
	}
}

static void setautofires (struct uae_prefs *prefs, int port, int af)
{
	for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
		setautofire (&joysticks[l], port, af);
		setautofire (&mice[l], port, af);
		setautofire (&keyboards[l], port, af);
	}
}

// merge gameport settings with current input configuration
static void compatibility_copy (struct uae_prefs *prefs, bool gameports)
{
	int used[MAX_INPUT_DEVICES] = { 0 };
	int i, joy;

	for (i = 0; i < MAX_JPORTS; i++) {
		joymodes[i] = prefs->jports[i].mode;
		joyinputs[i]= NULL;
		// remove all mappings from this port
		if (gameports)
			remove_compa_config (prefs, i);
		remove_custom_config (prefs, i);
		setjoyinputs (prefs, i);
	}

	for (i = 0; i < 2; i++) {
		if (prefs->jports[i].id >= 0 && joymodes[i] <= 0) {
			int mode = prefs->jports[i].mode;
			if (jsem_ismouse (i, prefs) >= 0) {
				switch (mode)
				{
					case JSEM_MODE_DEFAULT:
					case JSEM_MODE_MOUSE:
					case JSEM_MODE_WHEELMOUSE:
					default:
					joymodes[i] = JSEM_MODE_WHEELMOUSE;
					joyinputs[i] = i ? ip_mouse2 : ip_mouse1;
					break;
					case JSEM_MODE_MOUSE_CDTV:
					joymodes[i] = JSEM_MODE_MOUSE_CDTV;
					joyinputs[i] = ip_mousecdtv;
					break;
				}
			} else if (jsem_isjoy (i, prefs) >= 0) {
				switch (mode)
				{
					case JSEM_MODE_DEFAULT:
					case JSEM_MODE_JOYSTICK:
#ifndef INPUTDEVICE_SIMPLE
					case JSEM_MODE_GAMEPAD:
#endif
					case JSEM_MODE_JOYSTICK_CD32:
					default:
					{
						bool iscd32 = mode == JSEM_MODE_JOYSTICK_CD32 || (mode == JSEM_MODE_DEFAULT && prefs->cs_cd32cd);
						if (iscd32) {
							joymodes[i] = JSEM_MODE_JOYSTICK_CD32;
							joyinputs[i] = i ? ip_joycd322 : ip_joycd321;
#ifndef INPUTDEVICE_SIMPLE
						} else if (mode == JSEM_MODE_GAMEPAD) {
							joymodes[i] = JSEM_MODE_GAMEPAD;
							joyinputs[i] = i ? ip_joypad2 : ip_joypad1;
#endif
						} else {
							joymodes[i] = JSEM_MODE_JOYSTICK;
							joyinputs[i] = i ? ip_joy2 : ip_joy1;
						}
						break;
					}
#ifndef INPUTDEVICE_SIMPLE
					case JSEM_MODE_JOYSTICK_ANALOG:
						joymodes[i] = JSEM_MODE_JOYSTICK_ANALOG;
						joyinputs[i] = i ? ip_analog2 : ip_analog1;
						break;
#endif
					case JSEM_MODE_MOUSE:
					case JSEM_MODE_WHEELMOUSE:
						joymodes[i] = JSEM_MODE_WHEELMOUSE;
						joyinputs[i] = i ? ip_mouse2 : ip_mouse1;
						break;
					case JSEM_MODE_MOUSE_CDTV:
						joymodes[i] = JSEM_MODE_MOUSE_CDTV;
						joyinputs[i] = ip_mousecdtv;
						break;
				}
			} else if (prefs->jports[i].id >= 0) {
				joymodes[i] = i ? JSEM_MODE_JOYSTICK : JSEM_MODE_WHEELMOUSE;
				joyinputs[i] = i ? ip_joy2 : ip_mouse1;
			}
		}
	}

#ifndef INPUTDEVICE_SIMPLE
	for (i = 2; i < MAX_JPORTS; i++) {
		if (prefs->jports[i].id >= 0 && joymodes[i] <= 0) {
			if (jsem_isjoy (i, prefs) >= 0) {
				joymodes[i] = JSEM_MODE_JOYSTICK;
				joyinputs[i] = i == 3 ? ip_parjoy2 : ip_parjoy1;
			} else if (prefs->jports[i].id >= 0) {
				prefs->jports[i].mode = joymodes[i] = JSEM_MODE_JOYSTICK;
				joyinputs[i] = i == 3 ? ip_parjoy2 : ip_parjoy1;
			}
		}
	}
#endif

	for (i = 0; i < 2; i++) {
		int af = prefs->jports[i].autofire;
		if (prefs->jports[i].id >= 0) {
			int mode = prefs->jports[i].mode;
			if ((joy = jsem_ismouse (i, prefs)) >= 0) {
				if (gameports)
					cleardev (mice, joy);
				switch (mode)
				{
				case JSEM_MODE_DEFAULT:
				case JSEM_MODE_MOUSE:
				case JSEM_MODE_WHEELMOUSE:
				default:
					input_get_default_mouse (mice, joy, i, af, !gameports, mode != JSEM_MODE_MOUSE, false);
					joymodes[i] = JSEM_MODE_WHEELMOUSE;
					break;
				case JSEM_MODE_JOYSTICK:
				case JSEM_MODE_GAMEPAD:
				case JSEM_MODE_JOYSTICK_CD32:
					input_get_default_joystick (mice, joy, i, af, mode, !gameports, true);
					joymodes[i] = mode;
					break;
#ifndef INPUTDEVICE_SIMPLE
				case JSEM_MODE_JOYSTICK_ANALOG:
					input_get_default_joystick_analog (mice, joy, i, af, !gameports, true);
					joymodes[i] = JSEM_MODE_JOYSTICK_ANALOG;
					break;
#endif
				}
				_tcsncpy (prefs->jports[i].idc.name, idev[IDTYPE_MOUSE].get_friendlyname (joy), MAX_JPORTNAME - 1);
				_tcsncpy (prefs->jports[i].idc.configname, idev[IDTYPE_MOUSE].get_uniquename (joy), MAX_JPORTNAME - 1);
			}
		}
	}

	for (i = 1; i >= 0; i--) {
		int af = prefs->jports[i].autofire;
		if (prefs->jports[i].id >= 0) {
			int mode = prefs->jports[i].mode;
			joy = jsem_isjoy (i, prefs);
			if (joy >= 0) {
				if (gameports)
					cleardev (joysticks, joy);
				switch (mode)
				{
				case JSEM_MODE_DEFAULT:
				case JSEM_MODE_JOYSTICK:
#ifndef INPUTDEVICE_SIMPLE
				case JSEM_MODE_GAMEPAD:
#endif
				case JSEM_MODE_JOYSTICK_CD32:
				default:
				{
					bool iscd32 = mode == JSEM_MODE_JOYSTICK_CD32 || (mode == JSEM_MODE_DEFAULT && prefs->cs_cd32cd);
					input_get_default_joystick (joysticks, joy, i, af, mode, !gameports, false);
					if (iscd32)
						joymodes[i] = JSEM_MODE_JOYSTICK_CD32;
#ifndef INPUTDEVICE_SIMPLE
					else if (mode == JSEM_MODE_GAMEPAD)
						joymodes[i] = JSEM_MODE_GAMEPAD;
#endif
					else
						joymodes[i] = JSEM_MODE_JOYSTICK;
					break;
				}
#ifndef INPUTDEVICE_SIMPLE
				case JSEM_MODE_JOYSTICK_ANALOG:
					input_get_default_joystick_analog (joysticks, joy, i, af, !gameports, false);
					joymodes[i] = JSEM_MODE_JOYSTICK_ANALOG;
					break;
#endif
				case JSEM_MODE_MOUSE:
				case JSEM_MODE_WHEELMOUSE:
					input_get_default_mouse (joysticks, joy, i, af, !gameports, mode == JSEM_MODE_WHEELMOUSE, true);
					joymodes[i] = JSEM_MODE_WHEELMOUSE;
					break;
				case JSEM_MODE_MOUSE_CDTV:
					joymodes[i] = JSEM_MODE_MOUSE_CDTV;
					input_get_default_joystick (joysticks, joy, i, af, mode, !gameports, false);
					break;
				}
				_tcsncpy (prefs->jports[i].idc.name, idev[IDTYPE_JOYSTICK].get_friendlyname (joy), MAX_JPORTNAME - 1);
				_tcsncpy (prefs->jports[i].idc.configname, idev[IDTYPE_JOYSTICK].get_uniquename (joy), MAX_JPORTNAME - 1);
				used[joy] = 1;
			}
		}
	}

	if (gameports) {
		// replace possible old mappings with default keyboard mapping
		for (i = KBR_DEFAULT_MAP_FIRST; i <= KBR_DEFAULT_MAP_LAST; i++) {
			checkcompakb (keyboard_default_kbmaps[i], ip_joy2);
			checkcompakb (keyboard_default_kbmaps[i], ip_joy1);
#ifndef INPUTDEVICE_SIMPLE
			checkcompakb (keyboard_default_kbmaps[i], ip_joypad2);
			checkcompakb (keyboard_default_kbmaps[i], ip_joypad1);
			checkcompakb (keyboard_default_kbmaps[i], ip_parjoy2);
			checkcompakb (keyboard_default_kbmaps[i], ip_parjoy1);
#endif
			checkcompakb (keyboard_default_kbmaps[i], ip_mouse2);
			checkcompakb (keyboard_default_kbmaps[i], ip_mouse1);
		}
		for (i = KBR_DEFAULT_MAP_CD32_FIRST; i <= KBR_DEFAULT_MAP_CD32_LAST; i++) {
			checkcompakb (keyboard_default_kbmaps[i], ip_joycd321);
			checkcompakb (keyboard_default_kbmaps[i], ip_joycd322);
		}
	}

	for (i = 0; i < 2; i++) {
		if (prefs->jports[i].id >= 0) {
			int *kb = NULL;
			int mode = prefs->jports[i].mode;
			int af = prefs->jports[i].autofire;
			for (joy = 0; used[joy]; joy++);
			if (JSEM_ISANYKBD (i, prefs)) {
				bool cd32 = mode == JSEM_MODE_JOYSTICK_CD32 || (mode == JSEM_MODE_DEFAULT && prefs->cs_cd32cd);
				if (JSEM_ISNUMPAD (i, prefs)) {
					if (cd32)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CD32_NP];
#ifndef INPUTDEVICE_SIMPLE
					else if (mode == JSEM_MODE_GAMEPAD)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_NP3];
#endif
					else
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_NP];
				} else if (JSEM_ISCURSOR (i, prefs)) {
					if (cd32)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CD32_CK];
#ifndef INPUTDEVICE_SIMPLE
					else if (mode == JSEM_MODE_GAMEPAD)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CK3];
#endif
					else
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CK];
				} else if (JSEM_ISSOMEWHEREELSE (i, prefs)) {
					if (cd32)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CD32_SE];
#ifndef INPUTDEVICE_SIMPLE
					else if (mode == JSEM_MODE_GAMEPAD)
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_SE3];
#endif
					else
						kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_SE];
				}
				if (kb) {
					switch (mode)
					{
					case JSEM_MODE_JOYSTICK:
#ifndef INPUTDEVICE_SIMPLE
					case JSEM_MODE_GAMEPAD:
#endif
					case JSEM_MODE_JOYSTICK_CD32:
					case JSEM_MODE_DEFAULT:
						if (cd32) {
							setcompakb (prefs, kb, i ? ip_joycd322 : ip_joycd321, i, af);
							joymodes[i] = JSEM_MODE_JOYSTICK_CD32;
#ifndef INPUTDEVICE_SIMPLE
						} else if (mode == JSEM_MODE_GAMEPAD) {
							setcompakb (prefs, kb, i ? ip_joypad2 : ip_joypad1, i, af);
							joymodes[i] = JSEM_MODE_GAMEPAD;
#endif
						} else {
							setcompakb (prefs, kb, i ? ip_joy2 : ip_joy1, i, af);
							joymodes[i] = JSEM_MODE_JOYSTICK;
						}
						break;
					case JSEM_MODE_MOUSE:
					case JSEM_MODE_WHEELMOUSE:
						setcompakb (prefs, kb, i ? ip_mouse2 : ip_mouse1, i, af);
						joymodes[i] = JSEM_MODE_WHEELMOUSE;
						break;
					}
					used[joy] = 1;
				}
			}
		}
	}
	if (0 && currprefs.cs_cdtvcd) {
		setcompakb (prefs, keyboard_default_kbmaps[KBR_DEFAULT_MAP_CDTV], ip_mediacdtv, 0, 0);
	}

#ifndef INPUTDEVICE_SIMPLE
	// parport
	for (i = 2; i < MAX_JPORTS; i++) {
		int af = prefs->jports[i].autofire;
		if (prefs->jports[i].id >= 0) {
			joy = jsem_isjoy (i, prefs);
			if (joy >= 0) {
				if (gameports)
					cleardev (joysticks, joy);
				input_get_default_joystick (joysticks, joy, i, af, 0, !gameports, false);
				_tcsncpy (prefs->jports[i].idc.name, idev[IDTYPE_JOYSTICK].get_friendlyname (joy), MAX_JPORTNAME - 1);
				_tcsncpy (prefs->jports[i].idc.configname, idev[IDTYPE_JOYSTICK].get_uniquename (joy), MAX_JPORTNAME - 1);
				used[joy] = 1;
				joymodes[i] = JSEM_MODE_JOYSTICK;
			}
		}
	}
	for (i = 2; i < MAX_JPORTS; i++) {
		if (prefs->jports[i].id >= 0) {
			int *kb = NULL;
			for (joy = 0; used[joy]; joy++);
			if (JSEM_ISANYKBD (i, prefs)) {
				if (JSEM_ISNUMPAD (i, prefs))
					kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_NP];
				else if (JSEM_ISCURSOR (i, prefs))
					kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_CK];
				else if (JSEM_ISSOMEWHEREELSE (i, prefs))
					kb = keyboard_default_kbmaps[KBR_DEFAULT_MAP_SE];
				if (kb) {
					setcompakb (prefs, kb, i == 3 ? ip_parjoy2default : ip_parjoy1default, i, prefs->jports[i].autofire);
					used[joy] = 1;
					joymodes[i] = JSEM_MODE_JOYSTICK;
				}
			}
		}
	}
#endif

	for (i = 0; i < MAX_JPORTS; i++) {
		if (gameports)
			setautofires (prefs, i, prefs->jports[i].autofire);
	}

	for (i = 0; i < MAX_JPORTS; i++) {
		setjoyinputs (prefs, i);
		setjoydevices (prefs, gameports, i);
	}
}

static void disableifempty2 (struct uae_input_device *uid)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			if (uid->eventid[i][j] > 0 || uid->custom[i][j] != NULL)
				return;
		}
	}
	if (uid->enabled < 0)
		uid->enabled = 0;
}
static void disableifempty (struct uae_prefs *prefs)
{
	for (int l = 0; l < MAX_INPUT_DEVICES; l++) {
		disableifempty2 (&joysticks[l]);
		disableifempty2 (&mice[l]);
		if (!input_get_default_keyboard(l))
		  disableifempty2 (&keyboards[l]);
	}
}

static void matchdevices (struct inputdevice_functions *inf, struct uae_input_device *uid)
{
	int i, j;

	for (int l = 0; l < 2; l++) {
		bool fullmatch = l == 0;
		int match = -1;
		for (i = 0; i < inf->get_num (); i++) {
			TCHAR *aname1 = inf->get_friendlyname (i);
			TCHAR *aname2 = inf->get_uniquename (i);
			for (j = 0; j < MAX_INPUT_DEVICES; j++) {
				if (aname2 && uid[j].configname) {
					bool matched = false;
					TCHAR bname[MAX_DPATH];
					TCHAR bname2[MAX_DPATH];
					TCHAR *p1 ,*p2;
					TCHAR *bname1 = uid[j].name;

					if (fullmatch && (!bname1 || aname1))
						continue;
					_tcscpy (bname, uid[j].configname);
					_tcscpy (bname2, aname2);
					// strip possible local guid part
					p1 = _tcschr (bname, '{');
					p2 = _tcschr (bname2, '{');
					if (!p1 && !p2) {
						// check possible directinput names too
						p1 = _tcschr (bname, ' ');
						p2 = _tcschr (bname2, ' ');
					}
					if (!_tcscmp (bname, bname2)) {
						matched = true;
					} else if (p1 && p2 && p1 - bname == p2 - bname2) {
						*p1 = 0;
						*p2 = 0;
						if (bname[0] && !_tcscmp (bname2, bname))
							matched = true;
					}
					if (matched && fullmatch && _tcscmp(aname1, bname1) != 0)
						matched = false;
					if (matched) {
						if (match >= 0)
							match = -2;
						else
							match = j;
					}
					if (match == -2)
						break;
				}
			}
			if (!fullmatch) {
				// multiple matches -> use complete local-only id string for comparisons
				if (match == -2) {
					for (j = 0; j < MAX_INPUT_DEVICES; j++) {
						TCHAR *bname2 = uid[j].configname;
						if (aname2 && bname2 && !_tcscmp (aname2, bname2)) {
							match = j;
							break;
						}
					}
				}
				if (match < 0) {
					// no match, try friendly names only
					for (j = 0; j < MAX_INPUT_DEVICES; j++) {
						TCHAR *bname1 = uid[j].name;
						if (aname1 && bname1 && !_tcscmp (aname1, bname1)) {
							match = j;
							break;
						}
					}
				}
			}
			if (match >= 0) {
				j = match;
				if (j != i) {
					struct uae_input_device *tmp = xmalloc (struct uae_input_device, 1);
					memcpy (tmp, &uid[j], sizeof (struct uae_input_device));
					memcpy (&uid[j], &uid[i], sizeof (struct uae_input_device));
					memcpy (&uid[i], tmp, sizeof (struct uae_input_device));
					xfree (tmp);
				}
			}
		}
		if (match >= 0)
			break;
	}
	for (i = 0; i < inf->get_num (); i++) {
		if (uid[i].name == NULL)
			uid[i].name = my_strdup (inf->get_friendlyname (i));
		if (uid[i].configname == NULL)
			uid[i].configname = my_strdup (inf->get_uniquename (i));
	}
}

static void matchdevices_all (struct uae_prefs *prefs)
{
	int i;
	for (i = 0; i < MAX_INPUT_SETTINGS; i++) {
		matchdevices (&idev[IDTYPE_MOUSE], prefs->mouse_settings[i]);
		matchdevices (&idev[IDTYPE_JOYSTICK], prefs->joystick_settings[i]);
		matchdevices (&idev[IDTYPE_KEYBOARD], prefs->keyboard_settings[i]);
	}
}

bool inputdevice_set_gameports_mapping (struct uae_prefs *prefs, int devnum, int num, int evtnum, uae_u64 flags, int port, int input_selected_setting)
{
	TCHAR name[256];
	const struct inputevent *ie;
	int sub;

	if (evtnum < 0) {
		joysticks = prefs->joystick_settings[input_selected_setting];
		mice = prefs->mouse_settings[input_selected_setting];
		keyboards = prefs->keyboard_settings[input_selected_setting];
		for (sub = 0; sub < MAX_INPUT_SUB_EVENT; sub++) {
			int port2 = 0;
			inputdevice_get_mapping (devnum, num, NULL, &port2, NULL, NULL, sub);
			if (port2 == port + 1) {
				inputdevice_set_mapping (devnum, num, NULL, NULL, 0, 0, sub);
			}
		}
		return true;
	}
	ie = inputdevice_get_eventinfo (evtnum);
	if (!inputdevice_get_eventname (ie, name))
		return false;
	joysticks = prefs->joystick_settings[input_selected_setting];
	mice = prefs->mouse_settings[input_selected_setting];
	keyboards = prefs->keyboard_settings[input_selected_setting];

	sub = 0;
	if (inputdevice_get_widget_type (devnum, num, NULL, false) != IDEV_WIDGET_KEY) {
		for (sub = 0; sub < MAX_INPUT_SUB_EVENT; sub++) {
			int port2 = 0;
			int evt = inputdevice_get_mapping (devnum, num, NULL, &port2, NULL, NULL, sub);
			if (port2 == port + 1 && evt == evtnum)
				break;
			if (!inputdevice_get_mapping (devnum, num, NULL, NULL, NULL, NULL, sub))
				break;
		}
	}
	if (sub >= MAX_INPUT_SUB_EVENT)
		sub = MAX_INPUT_SUB_EVENT - 1;
	inputdevice_set_mapping (devnum, num, name, NULL, IDEV_MAPPED_GAMEPORTSCUSTOM1 | flags, port + 1, sub);

	joysticks = prefs->joystick_settings[prefs->input_selected_setting];
	mice = prefs->mouse_settings[prefs->input_selected_setting];
	keyboards = prefs->keyboard_settings[prefs->input_selected_setting];

	if (prefs->input_selected_setting != GAMEPORT_INPUT_SETTINGS) {
		int xport;
		uae_u64 xflags;
		TCHAR xname[MAX_DPATH], xcustom[MAX_DPATH];
		inputdevice_get_mapping (devnum, num, &xflags, &xport, xname, xcustom, 0);
		if (xport == 0)
			inputdevice_set_mapping (devnum, num, xname, xcustom, xflags, MAX_JPORTS + 1, SPARE_SUB_EVENT);
		inputdevice_set_mapping (devnum, num, name, NULL, IDEV_MAPPED_GAMEPORTSCUSTOM1 | flags, port + 1, 0);
	}
	return true;
}

static void resetinput (void)
{
	cd32_shifter[0] = cd32_shifter[1] = 8;
	for (int i = 0; i < MAX_JPORTS; i++) {
		oleft[i] = 0;
		oright[i] = 0;
		otop[i] = 0;
		obot[i] = 0;
		joybutton[i] = 0;
		joydir[i] = 0;
		mouse_deltanoreset[i][0] = 0;
		mouse_delta[i][0] = 0;
		mouse_deltanoreset[i][1] = 0;
		mouse_delta[i][1] = 0;
		mouse_deltanoreset[i][2] = 0;
		mouse_delta[i][2] = 0;
  }
	memset (keybuf, 0, sizeof keybuf);
	for (int i = 0; i < INPUT_QUEUE_SIZE; i++)
		input_queue[i].linecnt = input_queue[i].nextlinecnt = -1;

	for (int i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		sublevdir[0][i] = i;
		sublevdir[1][i] = MAX_INPUT_SUB_EVENT - i - 1;
	}
}

void inputdevice_copyjports(struct uae_prefs *srcprefs, struct uae_prefs *dstprefs)
{
	for (int i = 0; i < MAX_JPORTS; i++) {
		copyjport(srcprefs, dstprefs, i);
	}
}

void inputdevice_updateconfig_internal (struct uae_prefs *srcprefs, struct uae_prefs *dstprefs)
{
	keyboard_default = keyboard_default_table[currprefs.input_keyboard_type];

	inputdevice_copyjports(srcprefs, dstprefs);
	resetinput ();

	joysticks = dstprefs->joystick_settings[dstprefs->input_selected_setting];
	mice = dstprefs->mouse_settings[dstprefs->input_selected_setting];
	keyboards = dstprefs->keyboard_settings[dstprefs->input_selected_setting];

	matchdevices_all (dstprefs);

	memset (joysticks2, 0, sizeof joysticks2);
	memset (mice2, 0, sizeof mice2);

	int input_selected_setting = dstprefs->input_selected_setting;

	joysticks = dstprefs->joystick_settings[GAMEPORT_INPUT_SETTINGS];
	mice = dstprefs->mouse_settings[GAMEPORT_INPUT_SETTINGS];
	keyboards = dstprefs->keyboard_settings[GAMEPORT_INPUT_SETTINGS];
	dstprefs->input_selected_setting = GAMEPORT_INPUT_SETTINGS;

	for (int i = 0; i < MAX_INPUT_SETTINGS; i++) {
		joysticks[i].enabled = 0;
		mice[i].enabled = 0;
	}

	compatibility_copy (dstprefs, true);

	dstprefs->input_selected_setting = input_selected_setting;

	joysticks = dstprefs->joystick_settings[dstprefs->input_selected_setting];
	mice = dstprefs->mouse_settings[dstprefs->input_selected_setting];
	keyboards = dstprefs->keyboard_settings[dstprefs->input_selected_setting];

	if (dstprefs->input_selected_setting != GAMEPORT_INPUT_SETTINGS) {
		compatibility_copy (dstprefs, false);
	}

	disableifempty (dstprefs);
	scanevents (dstprefs);

	if (srcprefs) {
		for (int i = 0; i < MAX_JPORTS; i++) {
			copyjport(dstprefs, srcprefs, i);
		}
	}
}

void inputdevice_updateconfig (struct uae_prefs *srcprefs, struct uae_prefs *dstprefs)
{
	inputdevice_updateconfig_internal (srcprefs, dstprefs);

	set_config_changed ();

	for (int i = 0; i < MAX_JPORTS; i++) {
		inputdevice_store_used_device(&dstprefs->jports[i], i, false);
	}
}

#ifndef INPUTDEVICE_SIMPLE
/* called when devices get inserted or removed
* store old devices temporarily, enumerate all devices
* restore old devices back (order may have changed)
*/
void inputdevice_devicechange (struct uae_prefs *prefs)
{
	int acc = input_acquired;
	int i, idx;
	TCHAR *jports_name[MAX_JPORTS];
	TCHAR *jports_configname[MAX_JPORTS];
	int jportskb[MAX_JPORTS], jportscustom[MAX_JPORTS];
	int jportsmode[MAX_JPORTS];
	int jportid[MAX_JPORTS], jportaf[MAX_JPORTS];

	for (i = 0; i < MAX_JPORTS; i++) {
		jportskb[i] = -1;
		jportscustom[i] = -1;
		jportid[i] = prefs->jports[i].id;
		jportaf[i] = prefs->jports[i].autofire;
		jports_name[i] = NULL;
		jports_configname[i] = NULL;
		idx = inputdevice_getjoyportdevice (i, prefs->jports[i].id);
		if (idx >= JSEM_LASTKBD + inputdevice_get_device_total(IDTYPE_MOUSE) + inputdevice_get_device_total(IDTYPE_JOYSTICK)) {
			jportscustom[i] = idx - (JSEM_LASTKBD + inputdevice_get_device_total(IDTYPE_MOUSE) + inputdevice_get_device_total(IDTYPE_JOYSTICK));
		} else if (idx >= JSEM_LASTKBD) {
			if (prefs->jports[i].idc.name[0] == 0 && prefs->jports[i].idc.configname[0] == 0) {
			  struct inputdevice_functions *idf;
			  int devidx;
			  idx -= JSEM_LASTKBD;
			  idf = getidf (idx);
			  devidx = inputdevice_get_device_index (idx);
				jports_name[i] = my_strdup (idf->get_friendlyname (devidx));
				jports_configname[i] = my_strdup (idf->get_uniquename (devidx));
			}
		} else {
			jportskb[i] = idx;
		}
		jportsmode[i] = prefs->jports[i].mode;
		if (jports_name[i] == NULL)
			jports_name[i] = my_strdup(prefs->jports[i].idc.name);
		if (jports_configname[i] == NULL)
			jports_configname[i] = my_strdup(prefs->jports[i].idc.configname);
	}

	// store old devices
	struct inputdevconfig devcfg[MAX_INPUT_DEVICES][IDTYPE_MAX] = { 0 };
	int dev_nums[IDTYPE_MAX];
	for (int j = 0; j <= IDTYPE_KEYBOARD; j++) {
		struct inputdevice_functions *inf = &idev[j];
		dev_nums[j] = inf->get_num();
		for (i = 0; i < dev_nums[j]; i++) {
			TCHAR *un = inf->get_uniquename(i);
			TCHAR *fn = inf->get_friendlyname(i);
			_tcscpy(devcfg[i][j].name, fn);
			_tcscpy(devcfg[i][j].configname, un);
		}
	}

	inputdevice_unacquire ();
	idev[IDTYPE_JOYSTICK].close ();
	idev[IDTYPE_MOUSE].close ();
	idev[IDTYPE_KEYBOARD].close ();
	idev[IDTYPE_JOYSTICK].init ();
	idev[IDTYPE_MOUSE].init ();
	idev[IDTYPE_KEYBOARD].init ();
	matchdevices (&idev[IDTYPE_MOUSE], mice);
	matchdevices (&idev[IDTYPE_JOYSTICK], joysticks);
	matchdevices (&idev[IDTYPE_KEYBOARD], keyboards);

	// find out which one was removed or inserted
	for (int j = 0; j <= IDTYPE_KEYBOARD; j++) {
		struct inputdevice_functions *inf = &idev[j];
		int num = inf->get_num();
		bool df[MAX_INPUT_DEVICES];
		for (i = 0; i < MAX_INPUT_DEVICES; i++) {
			TCHAR *fn2 = devcfg[i][j].name;
			TCHAR *un2 = devcfg[i][j].configname;
			df[i] = false;
			if (fn2[0] && un2[0]) {
				for (int k = 0; k < num; k++) {
					TCHAR *un = inf->get_uniquename(k);
					TCHAR *fn = inf->get_friendlyname(k);
					if (!_tcscmp(fn2, fn) && !_tcscmp(un2, un)) {
						devcfg[i][j].name[0] = 0;
						devcfg[i][j].configname[0] = 0;
						df[k] = true;
					}
				}
			}
		}
		for (i = 0; i < MAX_INPUT_DEVICES; i++) {
			if (devcfg[i][j].name[0]) {
				write_log(_T("REMOVED: %s (%s)\n"), devcfg[i][j].name, devcfg[i][j].configname);
				inputdevice_store_unplugged_port(prefs, &devcfg[i][j]);
			}
			if (i < num && df[i] == false) {
				struct inputdevconfig idc;
				_tcscpy(idc.configname, inf->get_uniquename(i));
				_tcscpy(idc.name, inf->get_friendlyname(i));
				write_log(_T("INSERTED: %s (%s)\n"), idc.name, idc.configname);
				int portnum = inputdevice_get_unplugged_device(&idc);
				if (portnum >= 0) {
					write_log(_T("Inserting to port %d\n"), portnum);
					jportscustom[i] = -1;
					xfree(jports_name[portnum]);
					xfree(jports_configname[portnum]);
					jports_name[portnum] = my_strdup(idc.name);
					jports_configname[portnum] = my_strdup(idc.configname);
				} else {
					write_log(_T("Not inserted to any port\n"));
				}
			}
		}
	}

	bool fixedports[MAX_JPORTS];
	for (i = 0; i < MAX_JPORTS; i++) {
		freejport (prefs, i);
		fixedports[i] = false;
	}
	for (i = 0; i < MAX_JPORTS; i++) {
		bool found = true;
		if (jportscustom[i] >= 0) {
			TCHAR tmp[10];
			_stprintf(tmp, _T("custom%d"), jportscustom[i]);
			found = inputdevice_joyport_config(prefs, tmp, NULL, i, jportsmode[i], 0, true) != 0;
		} else if (jports_name[i][0] || jports_configname[i][0]) {
			if (!inputdevice_joyport_config (prefs, jports_name[i], jports_configname[i], i, jportsmode[i], 1, true)) {
				found = inputdevice_joyport_config (prefs, jports_name[i], NULL, i, jportsmode[i], 1, true) != 0;
			}
			if (!found) {
				inputdevice_joyport_config(prefs, _T("joydefault"), NULL, i, jportsmode[i], 0, true);
			}
		} else if (jportskb[i] >= 0) {
			TCHAR tmp[10];
			_stprintf (tmp, _T("kbd%d"), jportskb[i] + 1);
			found = inputdevice_joyport_config (prefs, tmp, NULL, i, jportsmode[i], 0, true) != 0;
			
		}
		fixedports[i] = found;
		prefs->jports[i].autofire = jportaf[i];
		xfree (jports_name[i]);
		xfree (jports_configname[i]);
		inputdevice_validate_jports(prefs, i, fixedports);
	}

	if (prefs == &changed_prefs)
		inputdevice_copyconfig (&changed_prefs, &currprefs);
	if (acc)
		inputdevice_acquire (TRUE);
  set_config_changed ();
}
#endif

// set default prefs to all input configuration settings
void inputdevice_default_prefs (struct uae_prefs *p)
{
	inputdevice_init ();

	p->input_selected_setting = GAMEPORT_INPUT_SETTINGS;
#ifdef PANDORA_SPECIFIC
  p->input_joymouse_multiplier = 20;
#else
  p->input_joymouse_multiplier = 2;
#endif
	p->input_joymouse_deadzone = 33;
	p->input_joystick_deadzone = 33;
	p->input_joymouse_speed = 10;
	p->input_analog_joystick_mult = 15;
	p->input_analog_joystick_offset = -1;
	p->input_mouse_speed = 100;
#ifdef __LIBRETRO__
  p->input_autofire_linecnt = 0;
#else
  p->input_autofire_linecnt = 8 * 312;
#endif
	p->input_keyboard_type = 0;
	keyboard_default = keyboard_default_table[p->input_keyboard_type];
	inputdevice_default_kb_all (p);
}

// set default keyboard and keyboard>joystick layouts
void inputdevice_setkeytranslation (struct uae_input_device_kbr_default **trans, int **kbmaps)
{
	keyboard_default_table = trans;
	keyboard_default_kbmaps = kbmaps;
}

// return true if keyboard/scancode pair is mapped
int inputdevice_iskeymapped (int keyboard, int scancode)
{
	if (!keyboards || scancode < 0)
		return 0;
	return scancodeused[keyboard][scancode];
}

static void rqualifiers (uae_u64 flags, bool release)
{
	uae_u64 mask = ID_FLAG_QUALIFIER1 << 1;
	for (int i = 0; i < MAX_INPUT_QUALIFIERS; i++) {
		if ((flags & mask) && (mask & (qualifiers << 1))) {
			if (release) {
				if (!(mask & qualifiers_r)) {
					qualifiers_r |= mask;
					for (int ii = 0; ii < MAX_INPUT_SUB_EVENT; ii++) {
						int qevt = qualifiers_evt[i][ii];
						if (qevt > 0) {
							write_log (_T("Released %d '%s'\n"), qevt, events[qevt].name);
							inputdevice_do_keyboard (events[qevt].data, 0);
						}
					}
				}
			} else {
				if ((mask & qualifiers_r)) {
					qualifiers_r &= ~mask;
					for (int ii = 0; ii < MAX_INPUT_SUB_EVENT; ii++) {
						int qevt = qualifiers_evt[i][ii];
						if (qevt > 0) {
							write_log (_T("Pressed %d '%s'\n"), qevt, events[qevt].name);
							inputdevice_do_keyboard (events[qevt].data, 1);
						}
					}
				}
			}
		}
		mask <<= 2;
	}
}

static int inputdevice_translatekeycode_2 (int keyboard, int scancode, int keystate, bool qualifiercheckonly)
{
	struct uae_input_device *na = &keyboards[keyboard];
	int j, k;
	int handled = 0;

	if (!keyboards || scancode < 0)
		return handled;

	j = 0;
	while (j < MAX_INPUT_DEVICE_EVENTS && na->extra[j] >= 0) {
		if (na->extra[j] == scancode) {
			bool qualonly;
			uae_u64 qualmask[MAX_INPUT_SUB_EVENT];
			getqualmask (qualmask, na, j, &qualonly);

			if (qualonly)
				qualifiercheckonly = true;
			for (k = 0; k < MAX_INPUT_SUB_EVENT; k++) {/* send key release events in reverse order */
				uae_u64 *flagsp = &na->flags[j][sublevdir[keystate == 0 ? 1 : 0][k]];
				int evt = na->eventid[j][sublevdir[keystate == 0 ? 1 : 0][k]];
				uae_u64 flags = *flagsp;
				int autofire = (flags & ID_FLAG_AUTOFIRE) ? 1 : 0;
#ifndef INPUTDEVICE_SIMPLE
				int toggle = (flags & ID_FLAG_TOGGLE) ? 1 : 0;
				int inverttoggle = (flags & ID_FLAG_INVERTTOGGLE) ? 1 : 0;
#endif
				int invert = (flags & ID_FLAG_INVERT) ? 1 : 0;
				int setmode = (flags & ID_FLAG_SET_ONOFF) ? 1: 0;
				int setval = (flags & ID_FLAG_SET_ONOFF_VAL) ? SET_ONOFF_ON_VALUE : SET_ONOFF_OFF_VALUE;
				int toggled;
				int state;

				if (keystate < 0) {
					state = keystate;
				} else if (invert) {
					state = keystate ? 0 : 1;
				} else {
					state = keystate;
				}
				if (setmode) {
					if (state)
						state = setval;
				}

				setqualifiers (evt, state > 0);

				if (qualifiercheckonly) {
					if (!state && (flags & ID_FLAG_CANRELEASE)) {
						*flagsp &= ~ID_FLAG_CANRELEASE;
						handle_input_event (evt, state, 1, autofire);
					}
					continue;
        }

#ifndef INPUTDEVICE_SIMPLE
				if (inverttoggle) {
					na->flags[j][sublevdir[state == 0 ? 1 : 0][k]] &= ~ID_FLAG_TOGGLED;
					if (state) {
						queue_input_event (evt, -1, 0, 0);
						handled |= handle_input_event (evt, 2, 1, 0);
					} else {
						handled |= handle_input_event (evt, 2, 1, autofire);
					}
				} else if (toggle) {
					if (!state)
						continue;
					if (!checkqualifiers (evt, flags, qualmask, na->eventid[j]))
						continue;
					*flagsp ^= ID_FLAG_TOGGLED;
					toggled = (*flagsp & ID_FLAG_TOGGLED) ? 2 : 0;
					handled |= handle_input_event (evt, toggled, 1, autofire);
				} else {
#endif
					rqualifiers (flags, state ? true : false);
					if (!checkqualifiers (evt, flags, qualmask, na->eventid[j])) {
						if (!state && !(flags & ID_FLAG_CANRELEASE)) {
							if (!invert)
						  continue;
						} else if (state) {
						  continue;
  				  }
					}

				  if (state) {
						if (!invert)
					    *flagsp |= ID_FLAG_CANRELEASE;
				  } else {
						if (!(flags & ID_FLAG_CANRELEASE) && !invert)
							continue;
					  *flagsp &= ~ID_FLAG_CANRELEASE;
          }
				  handled |= handle_input_event (evt, state, 1, autofire);
#ifndef INPUTDEVICE_SIMPLE
				}
#endif
			}
			queue_input_event (-1, -1, 0, 0);
			return handled;
		}
		j++;
	}
	return handled;
}

// main keyboard press/release entry point
int inputdevice_translatekeycode (int keyboard, int scancode, int state)
{
	// if not default keyboard and all events are empty: use default keyboard
	if (!input_get_default_keyboard(keyboard) && isemptykey(keyboard, scancode)) {
		keyboard = input_get_default_keyboard(-1);
	}
	if (inputdevice_translatekeycode_2 (keyboard, scancode, state, false))
		return 1;
	return 0;
}
void inputdevice_checkqualifierkeycode (int keyboard, int scancode, int state)
{
	inputdevice_translatekeycode_2 (keyboard, scancode, state, true);
}

void inputdevice_init (void)
{
	idev[IDTYPE_JOYSTICK] = inputdevicefunc_joystick;
	idev[IDTYPE_JOYSTICK].init ();
	idev[IDTYPE_MOUSE] = inputdevicefunc_mouse;
	idev[IDTYPE_MOUSE].init ();
	idev[IDTYPE_KEYBOARD] = inputdevicefunc_keyboard;
	idev[IDTYPE_KEYBOARD].init ();
}

void inputdevice_close (void)
{
	idev[IDTYPE_JOYSTICK].close ();
	idev[IDTYPE_MOUSE].close ();
	idev[IDTYPE_KEYBOARD].close ();
}

static struct uae_input_device *get_uid (const struct inputdevice_functions *id, int devnum)
{
	struct uae_input_device *uid = 0;
	if (id == &idev[IDTYPE_JOYSTICK]) {
		uid = &joysticks[devnum];
	} else if (id == &idev[IDTYPE_MOUSE]) {
		uid = &mice[devnum];
	} else if (id == &idev[IDTYPE_KEYBOARD]) {
		uid = &keyboards[devnum];
	}
	return uid;
}

static int get_event_data (const struct inputdevice_functions *id, int devnum, int num, int *eventid, TCHAR **custom, uae_u64 *flags, int *port, int sub)
{
	const struct uae_input_device *uid = get_uid (id, devnum);
	int type = id->get_widget_type (devnum, num, 0, 0);
	int i;
	if (type == IDEV_WIDGET_BUTTON || type == IDEV_WIDGET_BUTTONAXIS) {
		i = num - id->get_widget_first (devnum, IDEV_WIDGET_BUTTON) + ID_BUTTON_OFFSET;
		*eventid = uid->eventid[i][sub];
		if (flags)
			*flags = uid->flags[i][sub];
		if (port)
			*port = uid->port[i][sub];
		if (custom)
			*custom = uid->custom[i][sub];
		return i;
	} else if (type == IDEV_WIDGET_AXIS) {
		i = num - id->get_widget_first (devnum, type) + ID_AXIS_OFFSET;
		*eventid = uid->eventid[i][sub];
		if (flags)
			*flags = uid->flags[i][sub];
		if (port)
			*port = uid->port[i][sub];
		if (custom)
			*custom = uid->custom[i][sub];
		return i;
	} else if (type == IDEV_WIDGET_KEY) {
		i = num - id->get_widget_first (devnum, type);
		*eventid = uid->eventid[i][sub];
		if (flags)
			*flags = uid->flags[i][sub];
		if (port)
			*port = uid->port[i][sub];
		if (custom)
			*custom = uid->custom[i][sub];
		return i;
	}
	return -1;
}

static TCHAR *stripstrdup (const TCHAR *s)
{
	TCHAR *out = my_strdup (s);
	if (!out)
		return NULL;
	for (int i = 0; out[i]; i++) {
		if (out[i] < ' ')
			out[i] = ' ';
	}
	return out;
}

static int put_event_data (const struct inputdevice_functions *id, int devnum, int num, int eventid, TCHAR *custom, uae_u64 flags, int port, int sub)
{
	struct uae_input_device *uid = get_uid (id, devnum);
	int type = id->get_widget_type (devnum, num, 0, 0);
	int i, ret;

	for (i = 0; i < MAX_INPUT_QUALIFIERS; i++) {
		uae_u64 mask1 = ID_FLAG_QUALIFIER1 << (i * 2);
		uae_u64 mask2 = mask1 << 1;
		if ((flags & (mask1 | mask2)) == (mask1 | mask2))
			flags &= ~mask2;
	}
	if (custom && custom[0] == 0)
		custom = NULL;
	if (custom)
		eventid = 0;
	if (eventid <= 0 && !custom)
		flags = 0;

	ret = -1;
	if (type == IDEV_WIDGET_BUTTON || type == IDEV_WIDGET_BUTTONAXIS) {
		i = num - id->get_widget_first (devnum, IDEV_WIDGET_BUTTON) + ID_BUTTON_OFFSET;
		uid->eventid[i][sub] = eventid;
		uid->flags[i][sub] = flags;
		uid->port[i][sub] = port;
		xfree (uid->custom[i][sub]);
		ret = i;
	} else if (type == IDEV_WIDGET_AXIS) {
		i = num - id->get_widget_first (devnum, type) + ID_AXIS_OFFSET;
		uid->eventid[i][sub] = eventid;
		uid->flags[i][sub] = flags;
		uid->port[i][sub] = port;
		xfree (uid->custom[i][sub]);
		ret = i;
	} else if (type == IDEV_WIDGET_KEY) {
		i = num - id->get_widget_first (devnum, type);
		uid->eventid[i][sub] = eventid;
		uid->flags[i][sub] = flags;
		uid->port[i][sub] = port;
		xfree (uid->custom[i][sub]);
		ret = i;
	}
	if (ret < 0)
		return -1;
	if (uid->custom[i][sub])
		uid->eventid[i][sub] = INPUTEVENT_SPC_CUSTOM_EVENT;
	return ret;
}

static int is_event_used (const struct inputdevice_functions *id, int devnum, int isnum, int isevent)
{
	struct uae_input_device *uid = get_uid (id, devnum);
	int num, evt, sub;

	for (num = 0; num < id->get_widget_num (devnum); num++) {
		for (sub = 0; sub < MAX_INPUT_SUB_EVENT; sub++) {
			if (get_event_data (id, devnum, num, &evt, NULL, NULL, NULL, sub) >= 0) {
				if (evt == isevent && isnum != num)
					return 1;
			}
		}
	}
	return 0;
}

// device based index from global device index
int inputdevice_get_device_index (int devnum)
{
	int jcnt = idev[IDTYPE_JOYSTICK].get_num ();
	int mcnt = idev[IDTYPE_MOUSE].get_num ();
	int kcnt = idev[IDTYPE_KEYBOARD].get_num ();

	if (devnum < jcnt)
		return devnum;
	else if (devnum < jcnt + mcnt)
		return devnum - jcnt;
	else if (devnum < jcnt + mcnt + kcnt)
		return devnum - (jcnt + mcnt);
	return -1;
}

/* returns number of devices of type "type" */
int inputdevice_get_device_total (int type)
{
	return idev[type].get_num ();
}
/* returns the name of device */
TCHAR *inputdevice_get_device_name (int type, int devnum)
{
	return idev[type].get_friendlyname (devnum);
}
/* returns the name of device */
TCHAR *inputdevice_get_device_name2 (int devnum)
{
	return getidf (devnum)->get_friendlyname (inputdevice_get_device_index (devnum));
}
/* returns machine readable name of device */
TCHAR *inputdevice_get_device_unique_name (int type, int devnum)
{
	return idev[type].get_uniquename (devnum);
}
/* returns state (enabled/disabled) */
int inputdevice_get_device_status (int devnum)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	if (idf == NULL)
		return -1;
	struct uae_input_device *uid = get_uid (idf, inputdevice_get_device_index (devnum));
	return uid->enabled != 0;
}

/* set state (enabled/disabled) */
void inputdevice_set_device_status (int devnum, int enabled)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	int num = inputdevice_get_device_index (devnum);
	struct uae_input_device *uid = get_uid (idf, num);
	if (enabled) { // disable incompatible devices ("super device" vs "raw device")
		for (int i = 0; i < idf->get_num (); i++) {
			if (idf->get_flags (i) != idf->get_flags (num)) {
				struct uae_input_device *uid2 = get_uid (idf, i);
				uid2->enabled = 0;
			}
		}
	}
	uid->enabled = enabled;
}

/* returns number of axis/buttons and keys from selected device */
int inputdevice_get_widget_num (int devnum)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	return idf->get_widget_num (inputdevice_get_device_index (devnum));
}

// return name of event, do not use ie->name directly
bool inputdevice_get_eventname (const struct inputevent *ie, TCHAR *out)
{
	if (!out)
		return false;
	_tcscpy (out, ie->name);
	return true;
}

int inputdevice_iterate (int devnum, int num, TCHAR *name, int *af)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	static int id_iterator;
	const struct inputevent *ie;
	int mask, data, type;
	uae_u64 flags;
	int devindex = inputdevice_get_device_index (devnum);

	*af = 0;
	*name = 0;
	for (;;) {
		ie = &events[++id_iterator];
		if (!ie->confname) {
			id_iterator = 0;
			return 0;
		}
		mask = 0;
		type = idf->get_widget_type (devindex, num, NULL, NULL);
		if (type == IDEV_WIDGET_BUTTON || type == IDEV_WIDGET_BUTTONAXIS) {
			if (idf == &idev[IDTYPE_JOYSTICK]) {
				mask |= AM_JOY_BUT;
			} else {
				mask |= AM_MOUSE_BUT;
			}
		} else if (type == IDEV_WIDGET_AXIS) {
			if (idf == &idev[IDTYPE_JOYSTICK]) {
				mask |= AM_JOY_AXIS;
			} else {
				mask |= AM_MOUSE_AXIS;
			}
		} else if (type == IDEV_WIDGET_KEY) {
			mask |= AM_K;
		}
		if (ie->allow_mask & AM_INFO) {
			const struct inputevent *ie2 = ie + 1;
			while (!(ie2->allow_mask & AM_INFO)) {
				if (is_event_used (idf, devindex, ie2 - ie, -1)) {
					ie2++;
					continue;
				}
				if (ie2->allow_mask & mask)
					break;
				ie2++;
			}
			if (!(ie2->allow_mask & AM_INFO))
				mask |= AM_INFO;
		}
		if (!(ie->allow_mask & mask))
			continue;
		get_event_data (idf, devindex, num, &data, NULL, &flags, NULL, 0);
		inputdevice_get_eventname (ie, name);
		*af = (flags & ID_FLAG_AUTOFIRE) ? 1 : 0;
		return 1;
	}
}

// return mapped event from devnum/num/sub
int inputdevice_get_mapping (int devnum, int num, uae_u64 *pflags, int *pport, TCHAR *name, TCHAR *custom, int sub)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	const struct uae_input_device *uid = get_uid (idf, inputdevice_get_device_index (devnum));
	int port, data;
	uae_u64 flags = 0, flag;
	int devindex = inputdevice_get_device_index (devnum);
	TCHAR *customp = NULL;

	if (name)
		_tcscpy (name, _T("<none>"));
	if (custom)
		custom[0] = 0;
	if (pflags)
		*pflags = 0;
	if (pport)
		*pport = 0;
	if (uid == 0 || num < 0)
		return 0;
	if (get_event_data (idf, devindex, num, &data, &customp, &flag, &port, sub) < 0)
		return 0;
	if (customp && custom)
		_tcscpy (custom, customp);
	if (flag & ID_FLAG_AUTOFIRE)
		flags |= IDEV_MAPPED_AUTOFIRE_SET;
#ifndef INPUTDEVICE_SIMPLE
	if (flag & ID_FLAG_TOGGLE)
		flags |= IDEV_MAPPED_TOGGLE;
	if (flag & ID_FLAG_INVERTTOGGLE)
		flags |= IDEV_MAPPED_INVERTTOGGLE;
#endif
	if (flag & ID_FLAG_INVERT)
		flags |= IDEV_MAPPED_INVERT;
	if (flag & ID_FLAG_GAMEPORTSCUSTOM1)
		flags |= IDEV_MAPPED_GAMEPORTSCUSTOM1;
#ifndef INPUTDEVICE_SIMPLE
	if (flag & ID_FLAG_GAMEPORTSCUSTOM2)
		flags |= IDEV_MAPPED_GAMEPORTSCUSTOM2;
#endif
	if (flag & ID_FLAG_QUALIFIER_MASK)
		flags |= flag & ID_FLAG_QUALIFIER_MASK;
	if (flag & ID_FLAG_SET_ONOFF)
		flags |= IDEV_MAPPED_SET_ONOFF;
	if (flag & ID_FLAG_SET_ONOFF_VAL)
		flags |= IDEV_MAPPED_SET_ONOFF_VAL;
	if (pflags)
		*pflags = flags;
	if (pport)
		*pport = port;
	if (!data)
		return 0;
	if (events[data].allow_mask & AM_AF)
		flags |= IDEV_MAPPED_AUTOFIRE_POSSIBLE;
	if (pflags)
		*pflags = flags;
	inputdevice_get_eventname (&events[data], name);
	return data;
}

// set event name/custom/flags to devnum/num/sub
int inputdevice_set_mapping (int devnum, int num, const TCHAR *name, TCHAR *custom, uae_u64 flags, int port, int sub)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	const struct uae_input_device *uid = get_uid (idf, inputdevice_get_device_index (devnum));
	int eid, data, portp, amask;
	uae_u64 flag;
	TCHAR ename[256];
	int devindex = inputdevice_get_device_index (devnum);
	TCHAR *customp = NULL;

	if (uid == 0 || num < 0)
		return 0;
	if (name) {
		eid = 1;
		while (events[eid].name) {
			inputdevice_get_eventname (&events[eid], ename);
			if (!_tcscmp(ename, name))
				break;
			eid++;
		}
		if (!events[eid].name)
			return 0;
		if (events[eid].allow_mask & AM_INFO)
			return 0;
	} else {
		eid = 0;
	}
	if (get_event_data (idf, devindex, num, &data, &customp, &flag, &portp, sub) < 0)
		return 0;
	if (data >= 0) {
		amask = events[eid].allow_mask;
		flag &= ~(ID_FLAG_AUTOFIRE_MASK | ID_FLAG_GAMEPORTSCUSTOM_MASK | IDEV_MAPPED_QUALIFIER_MASK | ID_FLAG_INVERT);
		if (amask & AM_AF) {
			flag |= (flags & IDEV_MAPPED_AUTOFIRE_SET) ? ID_FLAG_AUTOFIRE : 0;
#ifndef INPUTDEVICE_SIMPLE
			flag |= (flags & IDEV_MAPPED_TOGGLE) ? ID_FLAG_TOGGLE : 0;
			flag |= (flags & IDEV_MAPPED_INVERTTOGGLE) ? ID_FLAG_INVERTTOGGLE : 0;
#endif
		}
		flag |= (flags & IDEV_MAPPED_INVERT) ? ID_FLAG_INVERT : 0;
		flag |= (flags & IDEV_MAPPED_GAMEPORTSCUSTOM1) ? ID_FLAG_GAMEPORTSCUSTOM1 : 0;
#ifndef INPUTDEVICE_SIMPLE
		flag |= (flags & IDEV_MAPPED_GAMEPORTSCUSTOM2) ? ID_FLAG_GAMEPORTSCUSTOM2 : 0;
#endif
		flag |= flags & IDEV_MAPPED_QUALIFIER_MASK;
		flag &= ~(IDEV_MAPPED_SET_ONOFF | IDEV_MAPPED_SET_ONOFF_VAL);
		if (amask & AM_SETTOGGLE) {
			flag |= (flags & IDEV_MAPPED_SET_ONOFF) ? ID_FLAG_SET_ONOFF : 0;
			flag |= (flags & IDEV_MAPPED_SET_ONOFF_VAL) ? ID_FLAG_SET_ONOFF_VAL : 0;
		}
		if (port >= 0)
			portp = port;
		put_event_data (idf, devindex, num, eid, custom, flag, portp, sub);
		return 1;
	}
	return 0;
}

int inputdevice_get_widget_type (int devnum, int num, TCHAR *name, bool inccode)
{
	uae_u32 code = 0;
	const struct inputdevice_functions *idf = getidf (devnum);
	int r = idf->get_widget_type (inputdevice_get_device_index (devnum), num, name, &code);
	if (r && inccode && &idev[IDTYPE_KEYBOARD] == idf) {
		TCHAR *p = name + _tcslen(name);
		if (_tcsncmp(name, _T("KEY_"), 4))
			_stprintf(p, _T(" [0x%02X]"), code);
	}
	return r;
}

static int config_change;

void inputdevice_config_change (void)
{
	config_change = 1;
}

int inputdevice_config_change_test (void)
{
	int v = config_change;
	config_change = 0;
	return v;
}

// copy configuration #src to configuration #dst
void inputdevice_copyconfig (struct uae_prefs *src, struct uae_prefs *dst)
{
	dst->input_selected_setting = src->input_selected_setting;
  dst->input_joymouse_multiplier = src->input_joymouse_multiplier;
	dst->input_joymouse_deadzone = src->input_joymouse_deadzone;
	dst->input_joystick_deadzone = src->input_joystick_deadzone;
	dst->input_joymouse_speed = src->input_joymouse_speed;
	dst->input_mouse_speed = src->input_mouse_speed;
  dst->input_autofire_linecnt = src->input_autofire_linecnt;

  dst->key_for_menu = src->key_for_menu;
  dst->key_for_quit = src->key_for_quit;
  dst->button_for_menu = src->button_for_menu;
  dst->button_for_quit = src->button_for_quit;

	for (int i = 0; i < MAX_JPORTS; i++) {
		copyjport (src, dst, i);
	}
  
	for (int i = 0; i < MAX_INPUT_SETTINGS; i++) {
		for (int j = 0; j < MAX_INPUT_DEVICES; j++) {
			memcpy (&dst->joystick_settings[i][j], &src->joystick_settings[i][j], sizeof (struct uae_input_device));
			memcpy (&dst->mouse_settings[i][j], &src->mouse_settings[i][j], sizeof (struct uae_input_device));
			memcpy (&dst->keyboard_settings[i][j], &src->keyboard_settings[i][j], sizeof (struct uae_input_device));
		}
	}

	inputdevice_updateconfig (src, dst);
}

static void swapevent (struct uae_input_device *uid, int i, int j, int evt)
{
	uid->eventid[i][j] = evt;
	int port = uid->port[i][j];
	if (port == 1)
		port = 2;
	else if (port == 2)
		port = 1;
	else if (port == 3)
		port = 4;
	else if (port == 4)
		port = 3;
	uid->port[i][j] = port;
}

static void swapjoydevice (struct uae_input_device *uid, const int **swaps)
{
	for (int i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (int j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			bool found = false;
			for (int k = 0; k < 2 && !found; k++) {
				int evtnum;
				for (int kk = 0; (evtnum = swaps[k][kk]) >= 0 && !found; kk++) {
					if (uid->eventid[i][j] == evtnum) {
						swapevent (uid, i, j, swaps[1 - k][kk]);
						found = true;
					} else {
						for (int jj = 0; axistable[jj] >= 0; jj += 3) {
							if (evtnum == axistable[jj] || evtnum == axistable[jj + 1] || evtnum == axistable[jj + 2]) {
								for (int ii = 0; ii < 3; ii++) {
									if (uid->eventid[i][j] == axistable[jj + ii]) {
										int evtnum2 = swaps[1 - k][kk];
										for (int m = 0; axistable[m] >= 0; m += 3) {
											if (evtnum2 == axistable[m] || evtnum2 == axistable[m + 1] || evtnum2 == axistable[m + 2]) {
												swapevent (uid, i, j, axistable[m + ii]);												
												found = true;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

// swap gameports ports, remember to handle customized ports too
void inputdevice_swap_compa_ports (struct uae_prefs *prefs, int portswap)
{
	struct jport tmp;
	memcpy (&tmp, &prefs->jports[portswap], sizeof (struct jport));
	memcpy (&prefs->jports[portswap], &prefs->jports[portswap + 1], sizeof (struct jport));
	memcpy (&prefs->jports[portswap + 1], &tmp, sizeof (struct jport));
	inputdevice_updateconfig (NULL, prefs);
}

// swap device "devnum" ports 0<>1 and 2<>3
void inputdevice_swap_ports (struct uae_prefs *p, int devnum)
{
	const struct inputdevice_functions *idf = getidf (devnum);
	struct uae_input_device *uid = get_uid (idf, inputdevice_get_device_index (devnum));
	int i, j, k, event, unit;
	const struct inputevent *ie, *ie2;

	for (i = 0; i < MAX_INPUT_DEVICE_EVENTS; i++) {
		for (j = 0; j < MAX_INPUT_SUB_EVENT; j++) {
			event = uid->eventid[i][j];
			if (event <= 0)
				continue;
			ie = &events[event];
			if (ie->unit <= 0)
				continue;
			unit = ie->unit;
			k = 1;
			while (events[k].confname) {
				ie2 = &events[k];
				if (ie2->type == ie->type && ie2->data == ie->data && ie2->unit - 1 == ((ie->unit - 1) ^ 1) &&
					ie2->allow_mask == ie->allow_mask && uid->port[i][j] == 0) {
					uid->eventid[i][j] = k;
					break;
				}
				k++;
			}
		}
	}
}

//memcpy (p->joystick_settings[dst], p->joystick_settings[src], sizeof (struct uae_input_device) * MAX_INPUT_DEVICES);
static void copydev (struct uae_input_device *dst, struct uae_input_device *src, int selectedwidget)
{
	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		for (int j = 0; j < MAX_INPUT_DEVICE_EVENTS; j++) {
			if (j == selectedwidget || selectedwidget < 0) {
			  for (int k = 0; k < MAX_INPUT_SUB_EVENT_ALL; k++) {
				  xfree (dst[i].custom[j][k]);
			  }
		  }
		}
		if (selectedwidget < 0) {
		  xfree (dst[i].configname);
		  xfree (dst[i].name);
	  }
	}
	if (selectedwidget < 0) {
  	memcpy (dst, src, sizeof (struct uae_input_device) * MAX_INPUT_DEVICES);
	} else {
		int j = selectedwidget;
		for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
			for (int k = 0; k < MAX_INPUT_SUB_EVENT_ALL; k++) {
				dst[i].eventid[j][k] = src[i].eventid[j][k];
				dst[i].custom[j][k] = src[i].custom[j][k];
				dst[i].flags[j][k] = src[i].flags[j][k];
				dst[i].port[j][k] = src[i].port[j][k];
			}
			dst[i].extra[j] = src[i].extra[j];
		}
	}
	for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
		for (int j = 0; j < MAX_INPUT_DEVICE_EVENTS; j++) {
			if (j == selectedwidget || selectedwidget < 0) {
			  for (int k = 0; k < MAX_INPUT_SUB_EVENT_ALL; k++) {
				  if (dst[i].custom)
					  dst[i].custom[j][k] = my_strdup (dst[i].custom[j][k]);
			  }
		  }
		}
		if (selectedwidget < 0) {
		  dst[i].configname = my_strdup (dst[i].configname);
		  dst[i].name = my_strdup (dst[i].name);
		}
  }
}

// copy whole configuration #x-slot to another
// +1 = default
// +2 = default (pc keyboard)
void inputdevice_copy_single_config (struct uae_prefs *p, int src, int dst, int devnum, int selectedwidget)
{
	if (selectedwidget >= 0) {
		if (devnum < 0)
			return;
		if (gettype (devnum) != IDTYPE_KEYBOARD)
			return;
	}
	if (src >= MAX_INPUT_SETTINGS) {
		if (gettype (devnum) == IDTYPE_KEYBOARD) {
			p->input_keyboard_type = src > MAX_INPUT_SETTINGS ? 1 : 0;
			keyboard_default = keyboard_default_table[p->input_keyboard_type];
			inputdevice_default_kb (p, dst);
		}
	}
	if (src == dst)
		return;
	if (src < MAX_INPUT_SETTINGS) {
		if (devnum < 0 || gettype (devnum) == IDTYPE_JOYSTICK)
			copydev (p->joystick_settings[dst], p->joystick_settings[src], selectedwidget);
		if (devnum < 0 || gettype (devnum) == IDTYPE_MOUSE)
			copydev (p->mouse_settings[dst], p->mouse_settings[src], selectedwidget);
		if (devnum < 0 || gettype (devnum) == IDTYPE_KEYBOARD)
			copydev (p->keyboard_settings[dst], p->keyboard_settings[src], selectedwidget);
	}
}

void inputdevice_acquire (int allmode)
{
	int i;

	//write_log (_T("inputdevice_acquire\n"));

	for (i = 0; i < MAX_INPUT_DEVICES; i++)
		idev[IDTYPE_JOYSTICK].unacquire (i);
	for (i = 0; i < MAX_INPUT_DEVICES; i++)
		idev[IDTYPE_MOUSE].unacquire (i);
	for (i = 0; i < MAX_INPUT_DEVICES; i++)
		idev[IDTYPE_KEYBOARD].unacquire (i);

	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		if ((use_joysticks[i] && allmode >= 0) || (allmode && !idev[IDTYPE_JOYSTICK].get_flags (i)))
			idev[IDTYPE_JOYSTICK].acquire (i, 0);
	}
	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		if ((use_mice[i] && allmode >= 0) || (allmode && !idev[IDTYPE_MOUSE].get_flags (i)))
			idev[IDTYPE_MOUSE].acquire (i, allmode < 0);
	}
	// Always acquire first + enabled keyboards
	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		if (use_keyboards[i] || i == 0)
			idev[IDTYPE_KEYBOARD].acquire (i, allmode < 0);
	}

	if (input_acquired)
		return;

	idev[IDTYPE_JOYSTICK].acquire (-1, 0);
	idev[IDTYPE_MOUSE].acquire (-1, 0);
	idev[IDTYPE_KEYBOARD].acquire (-1, 0);
	//    if (!input_acquired)
	//	write_log (_T("input devices acquired (%s)\n"), allmode ? "all" : "selected only");
	input_acquired = 1;
}

void inputdevice_unacquire(bool emulationactive, int inputmask)
{
	int i;

	//write_log (_T("inputdevice_unacquire %d %d\n"), emulationactive, inputmask);

	if (!emulationactive)
		inputmask = 0;


	if (!(inputmask & 4)) {
	  for (i = 0; i < MAX_INPUT_DEVICES; i++)
		  idev[IDTYPE_JOYSTICK].unacquire (i);
	}
	if (!(inputmask & 2)) {
	  for (i = 0; i < MAX_INPUT_DEVICES; i++)
		  idev[IDTYPE_MOUSE].unacquire (i);
	}
	if (!(inputmask & 1)) {
	  for (i = 0; i < MAX_INPUT_DEVICES; i++)
		  idev[IDTYPE_KEYBOARD].unacquire (i);
	}

	if (!input_acquired)
		return;

	input_acquired = 0;
	if (!(inputmask & 4))
	  idev[IDTYPE_JOYSTICK].unacquire (-1);
	if (!(inputmask & 2))
	  idev[IDTYPE_MOUSE].unacquire (-1);
	if (!(inputmask & 1))
	  idev[IDTYPE_KEYBOARD].unacquire (-1);
}

void inputdevice_unacquire(void)
{
	inputdevice_unacquire(false, 0);
}

/* Call this function when host machine's joystick/joypad/etc button state changes
* This function translates button events to Amiga joybutton/joyaxis/keyboard events
*/

/* button states:
* state = -1 -> mouse wheel turned or similar (button without release)
* state = 1 -> button pressed
* state = 0 -> button released
*/

void setjoybuttonstate (int joy, int button, int state)
{
	setbuttonstateall (&joysticks[joy], &joysticks2[joy], button, state ? 1 : 0);
}

void setmousebuttonstate (int mouse, int button, int state)
{
	uae_u32 obuttonmask = mice2[mouse].buttonmask;
	setbuttonstateall (&mice[mouse], &mice2[mouse], button, state);
	if (obuttonmask != mice2[mouse].buttonmask)
		mousehack_helper (mice2[mouse].buttonmask);
}

/* same for joystick axis (analog or digital)
* (0 = center, -max = full left/top, max = full right/bottom)
*/
void setjoystickstate (int joy, int axis, int state, int max)
{
	struct uae_input_device *id = &joysticks[joy];
	struct uae_input_device2 *id2 = &joysticks2[joy];
	int deadzone = currprefs.input_joymouse_deadzone * max / 100;
	int i, v1, v2;

	v1 = state;
	v2 = id2->states[axis][MAX_INPUT_SUB_EVENT];

	if (v1 < deadzone && v1 > -deadzone)
		v1 = 0;
	if (v2 < deadzone && v2 > -deadzone)
		v2 = 0;

	//write_log (_T("%d:%d new=%d old=%d state=%d max=%d\n"), joy, axis, v1, v2, state, max);

	if (!joysticks[joy].enabled) {
		if (v1 > 0)
			v1 = 1;
		else if (v1 < 0)
			v1 = -1;
		if (v2 > 0)
			v2 = 1;
		else if (v2 < 0)
			v2 = -1;
		if (v1 && v1 != v2 && (axis == 0 || axis == 1)) {
			static int prevdir;
			static struct timeval tv1;
			struct timeval tv2;
			gettimeofday (&tv2, NULL);
			if ((uae_s64)tv2.tv_sec * 1000000 + tv2.tv_usec < (uae_s64)tv1.tv_sec * 1000000 + tv1.tv_usec + 500000 && prevdir == v1) {
				switchdevice (&joysticks[joy], v1 < 0 ? 0 : 1, false);
				tv1.tv_sec = 0;
				tv1.tv_usec = 0;
				prevdir = 0;
			} else {
				tv1.tv_sec = tv2.tv_sec;
				tv1.tv_usec = tv2.tv_usec;
				prevdir = v1;
			}
		}
		return;
	}
	for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		uae_u64 flags = id->flags[ID_AXIS_OFFSET + axis][i];
		int state2 = v1;
		if (flags & ID_FLAG_INVERT)
			state2 = -state2;
		if (state2 != id2->states[axis][i]) {
			//write_log(_T("-> %d %d\n"), i, state2);
			handle_input_event (id->eventid[ID_AXIS_OFFSET + axis][i], state2, max, flags & ID_FLAG_AUTOFIRE);
			id2->states[axis][i] = state2;
		}
	}
	id2->states[axis][MAX_INPUT_SUB_EVENT] = v1;
}
int getjoystickstate (int joy)
{
	return joysticks[joy].enabled;
}

void setmousestate (int mouse, int axis, int data, int isabs)
{
	int i, v, diff;
	int *mouse_p, *oldm_p;
	float d;
	struct uae_input_device *id = &mice[mouse];
	static float fract[MAX_INPUT_DEVICES][MAX_INPUT_DEVICE_EVENTS];

	if (!mice[mouse].enabled) {
		if (isabs && currprefs.input_tablet > 0) {
			if (axis == 0)
				lastmx = data;
			else
				lastmy = data;
			if (axis)
				mousehack_helper (mice2[mouse].buttonmask);
		}
		return;
	}
	d = 0;
	mouse_p = &mouse_axis[mouse][axis];
	oldm_p = &oldm_axis[mouse][axis];
	if (!isabs) {
		// eat relative movements while in mousehack mode
		if (currprefs.input_tablet == TABLET_MOUSEHACK && mousehack_alive () && axis < 2)
			return;
		*oldm_p = *mouse_p;
		*mouse_p += data;
		d = (*mouse_p - *oldm_p) * currprefs.input_mouse_speed / 100.0f;
	} else {
		d = data - *oldm_p;
		*oldm_p = data;
		*mouse_p += d;
		if (axis == 0)
			lastmx = data;
		else
			lastmy = data;
		if (axis)
			mousehack_helper (mice2[mouse].buttonmask);
		if (currprefs.input_tablet == TABLET_MOUSEHACK && mousehack_alive () && axis < 2)
			return;
	}
	v = (int)d;
	fract[mouse][axis] += d - v;
	diff = (int)fract[mouse][axis];
	v += diff;
	fract[mouse][axis] -= diff;
	for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
		uae_u64 flags = id->flags[ID_AXIS_OFFSET + axis][i];
		if (!isabs && (flags & ID_FLAG_INVERT))
			v = -v;
		handle_input_event (id->eventid[ID_AXIS_OFFSET + axis][i], v, 0, 0);
  }
}

int getmousestate (int joy)
{
	return mice[joy].enabled;
}

int jsem_isjoy (int port, const struct uae_prefs *p)
{
	int v = JSEM_DECODEVAL (port, p);
	if (v < JSEM_JOYS)
		return -1;
	v -= JSEM_JOYS;
	if (v >= inputdevice_get_device_total (IDTYPE_JOYSTICK))
		return -1;
	return v;
}

int jsem_ismouse (int port, const struct uae_prefs *p)
{
	int v = JSEM_DECODEVAL (port, p);
	if (v < JSEM_MICE)
		return -1;
	v -= JSEM_MICE;
	if (v >= inputdevice_get_device_total (IDTYPE_MOUSE))
		return -1;
	return v;
}

int jsem_iskbdjoy (int port, const struct uae_prefs *p)
{
	int v = JSEM_DECODEVAL (port, p);
	if (v < JSEM_KBDLAYOUT)
		return -1;
	v -= JSEM_KBDLAYOUT;
	if (v >= JSEM_LASTKBD)
		return -1;
	return v;
}

static bool fixjport (struct jport *port, int add, bool always)
{
	bool wasinvalid = false;
	int vv = port->id;
	if (vv == JPORT_NONE)
		return wasinvalid;
	if (vv >= JSEM_JOYS && vv < JSEM_MICE) {
		vv -= JSEM_JOYS;
		vv += add;
		if (vv >= inputdevice_get_device_total (IDTYPE_JOYSTICK))
			vv = 0;
		vv += JSEM_JOYS;
	} else if (vv >= JSEM_MICE && vv < JSEM_END) {
		vv -= JSEM_MICE;
		vv += add;
		if (vv >= inputdevice_get_device_total (IDTYPE_MOUSE))
			vv = 0;
		vv += JSEM_MICE;
	} else if (vv >= JSEM_KBDLAYOUT && vv < JSEM_LASTKBD) {
		vv -= JSEM_KBDLAYOUT;
		vv += add;
		if (vv >= JSEM_LASTKBD)
			vv = 0;
		vv += JSEM_KBDLAYOUT;
	}
	if (port->id != vv || always) {
		port->idc.shortid[0] = 0;
		port->idc.configname[0] = 0;
		port->idc.name[0] = 0;
		if (vv >= JSEM_JOYS && vv < JSEM_MICE) {
			_tcscpy(port->idc.name, inputdevice_get_device_name (IDTYPE_JOYSTICK, vv - JSEM_JOYS));
			_tcscpy(port->idc.configname, inputdevice_get_device_unique_name (IDTYPE_JOYSTICK, vv - JSEM_JOYS));
		} else if (vv >= JSEM_MICE && vv < JSEM_END) {
			_tcscpy(port->idc.name, inputdevice_get_device_name (IDTYPE_MOUSE, vv - JSEM_MICE));
			_tcscpy(port->idc.configname, inputdevice_get_device_unique_name (IDTYPE_MOUSE, vv - JSEM_MICE));
		} else if (vv >= JSEM_KBDLAYOUT && vv < JSEM_CUSTOM) {
			_stprintf(port->idc.shortid, _T("kbd%d"), vv - JSEM_KBDLAYOUT + 1);
		}
		wasinvalid = true;
	}
	port->id = vv;
	return wasinvalid;
}

static void inputdevice_get_previous_joy(struct uae_prefs *p, int portnum)
{
	bool found = false;
	int idx = 0;
	struct jport *jpx = &p->jports[portnum];
	for (;;) {
		struct jport *jp = inputdevice_get_used_device(portnum, idx);
		if (!jp)
			break;
		if (jp->idc.configname[0]) {
			found = inputdevice_joyport_config(p, jp->idc.name, jp->idc.configname, portnum, jp->mode, 1, true) != 0;
			if (!found && jp->id == JPORT_UNPLUGGED)
				found = inputdevice_joyport_config(p, jp->idc.name, NULL, portnum, jp->mode, 1, true) != 0;
		} else if (jp->id < JSEM_JOYS && jp->id >= 0) {
			jpx->id = jp->id;
			found = true;
		}
		if (found) {
			jpx->mode = jp->mode;
			jpx->autofire = jp->autofire;
			inputdevice_set_newest_used_device(portnum, jp);
			break;
		}
		idx++;
	}
	if (!found) {
		if (default_keyboard_layout[portnum] > 0) {
			p->jports[portnum].id = default_keyboard_layout[portnum] - 1;
		} else {
			p->jports[portnum].id = JPORT_NONE;
		}
	}
}

void inputdevice_validate_jports (struct uae_prefs *p, int changedport, bool *fixedports)
{
	for (int i = 0; i < MAX_JPORTS; i++) {
		fixjport (&p->jports[i], 0, changedport == i);
  }

	for (int i = 0; i < MAX_JPORTS; i++) {
		if (p->jports[i].id < 0)
			continue;
		for (int j = 0; j < MAX_JPORTS; j++) {
			if (p->jports[j].id < 0)
				continue;
			if (j == i)
				continue;
			if (p->jports[i].id == p->jports[j].id) {
				int cnt = 0;
				for (;;) {
					int k;
					if (i == changedport) {
						k = j;
						if (fixedports && fixedports[k]) {
							k = i;
						}
					} else {
						k = i;
					}
					// same in other slots too?
					bool other = false;
					for (int l = 0; l < MAX_JPORTS; l++) {
						if (l == k)
							continue;
						if (p->jports[l].id == p->jports[k].id) {
							other = true;
						}
					}

					if (!other && p->jports[i].id != p->jports[j].id)
						break;

					struct jport *jp = NULL;
					for (;;) {
						jp = inputdevice_get_used_device(k, cnt);
						cnt++;
						if (!jp)
							break;
						if (jp->id < 0)
							continue;
						memcpy(&p->jports[k].id, jp, sizeof(struct jport));
						if (fixjport(&p->jports[k], 0, false))
							continue;
						inputdevice_set_newest_used_device(k, &p->jports[k]);
						break;
					}
					if (jp)
						continue;
					freejport(p, k);
					break;
				}
			}
		}
	}
}

void inputdevice_joyport_config_store(struct uae_prefs *p, const TCHAR *value, int portnum, int mode, int type)
{
	struct jport *jp = &p->jports[portnum];
	if (type == 2) {
		_tcscpy(jp->idc.name, value);
	} else if (type == 1) {
		_tcscpy(jp->idc.configname, value);
	} else {
		_tcscpy(jp->idc.shortid, value);
	}
	if (mode >= 0)
		jp->mode = mode;
}

int inputdevice_joyport_config (struct uae_prefs *p, const TCHAR *value1, const TCHAR *value2, int portnum, int mode, int type, bool candefault)
{
	switch (type)
	{
	case 1: // check and set
	case 2: // check only
		{
			for (int j = 0; j < 2; j++) {
				int matched = -1;
				struct inputdevice_functions *idf;
				int dtype = IDTYPE_MOUSE;
				int idnum = JSEM_MICE;
				if (j > 0) {
					dtype = IDTYPE_JOYSTICK;
					idnum = JSEM_JOYS;
				}
				idf = &idev[dtype];
				if (value1 && value2) {
					for (int i = 0; i < idf->get_num(); i++) {
						TCHAR *name1 = idf->get_friendlyname(i);
						TCHAR *name2 = idf->get_uniquename(i);
						if (name2 && !_tcscmp(name2, value2) && name1 && !_tcscmp(name1, value1)) {
							// config+friendlyname matched: don't bother to check for duplicates
							matched = i;
							break;
						}
					}
				}
				if (matched < 0 && value2) {
					matched = -1;
					for (int i = 0; i < idf->get_num (); i++) {
						TCHAR *name2 = idf->get_uniquename (i);
						if (name2 && !_tcscmp (name2, value2)) {
							if (matched >= 0) {
								matched = -2;
								break;
							} else {
								matched = i;
							}
						}
					}
				}
				if (matched < 0 && value1) {
					matched = -1;
					for (int i = 0; i < idf->get_num (); i++) {
						TCHAR *name1 = idf->get_friendlyname (i);
						if (name1 && !_tcscmp (name1, value1)) {
							if (matched >= 0) {
								matched = -2;
								break;
							} else {
								matched = i;
							}
						}
					}
				}
				if (matched >= 0) {
					if (type == 1) {
						if (value1)
							_tcscpy(p->jports[portnum].idc.name, value1);
						if (value2)
							_tcscpy(p->jports[portnum].idc.configname, value2);
						p->jports[portnum].id = idnum + matched;
						if (mode >= 0)
							p->jports[portnum].mode = mode;
						set_config_changed ();
					}
					return 1;
				}
			}
			return 0;
		}
		break;
	case 0:
		{
			int start = JPORT_NONE, got = 0, max = -1;
			int type = -1;
			const TCHAR *pp = NULL;
			if (_tcsncmp (value1, _T("kbd"), 3) == 0) {
				start = JSEM_KBDLAYOUT;
				pp = value1 + 3;
				got = 1;
				max = JSEM_LASTKBD;
			} else if (_tcscmp(value1, _T("joydefault")) == 0) {
				type = IDTYPE_JOYSTICK;
				start = JSEM_JOYS;
				got = 1;
			} else if (_tcscmp(value1, _T("mousedefault")) == 0) {
				type = IDTYPE_MOUSE;
				start = JSEM_MICE;
				got = 1;
			} else if (_tcsncmp (value1, _T("joy"), 3) == 0) {
				type = IDTYPE_JOYSTICK;
				start = JSEM_JOYS;
				pp = value1 + 3;
				got = 1;
				max = idev[IDTYPE_JOYSTICK].get_num ();
			} else if (_tcsncmp (value1, _T("mouse"), 5) == 0) {
				type = IDTYPE_MOUSE;
				start = JSEM_MICE;
				pp = value1 + 5;
				got = 1;
				max = idev[IDTYPE_MOUSE].get_num ();
			} else if (_tcscmp(value1, _T("none")) == 0) {
				got = 2;
			}
			if (got) {
				if (pp && max != 0) {
					int v = _tstol (pp);
					if (start >= 0) {
						if (start == JSEM_KBDLAYOUT && v > 0)
							v--;
						if (v >= 0) {
							if (v >= max)
								v = 0;
							start += v;
							got = 2;
						}
					}
				}
				if (got >= 2) {
					p->jports[portnum].id = start;
					if (mode >= 0)
						p->jports[portnum].mode = mode;
					if (start < JSEM_JOYS)
						default_keyboard_layout[portnum] = start + 1;
					if (got == 2 && candefault) {
						inputdevice_store_used_device(&p->jports[portnum], portnum, false);
					}
					set_config_changed ();
					return 1;
				}
				// joystick not found, select previously used or default
				if (start == JSEM_JOYS && p->jports[portnum].id < JSEM_JOYS) {
					inputdevice_get_previous_joy(p, portnum);
					set_config_changed ();
					return 1;
				}
			}
		}
		break;
	}
	return 0;
}

int inputdevice_getjoyportdevice (int port, int val)
{
	int idx;
	if (val < 0) {
		idx = -1;
	} else if (val >= JSEM_MICE) {
		idx = val - JSEM_MICE;
		if (idx >= inputdevice_get_device_total (IDTYPE_MOUSE))
			idx = 0;
		else
			idx += inputdevice_get_device_total (IDTYPE_JOYSTICK);
		idx += JSEM_LASTKBD;
	} else if (val >= JSEM_JOYS) {
		idx = val - JSEM_JOYS;
		if (idx >= inputdevice_get_device_total (IDTYPE_JOYSTICK))
			idx = 0;
		idx += JSEM_LASTKBD;
	} else {
		idx = val - JSEM_KBDLAYOUT;
	}
	return idx;
}

void inputdevice_fix_prefs(struct uae_prefs *p, bool userconfig)
{
	struct jport jport_config_store[MAX_JPORTS];

	for (int i = 0; i < MAX_JPORTS; i++) {
		memcpy(&jport_config_store[i], &p->jports[i], sizeof(struct jport));
	}

	bool defaultports = userconfig == false;
	bool matched[MAX_JPORTS];
	// configname+friendlyname first
	for (int i = 0; i < MAX_JPORTS; i++) {
		struct jport *jp = &jport_config_store[i];
		matched[i] = false;
		if (jp->idc.configname[0] && jp->idc.name[0]) {
			if (inputdevice_joyport_config(p, jp->idc.name, jp->idc.configname, i, jp->mode, 1, userconfig)) {
				inputdevice_validate_jports(p, i, matched);
				inputdevice_store_used_device(&p->jports[i], i, defaultports);
				matched[i] = true;
				write_log(_T("Port%d: COMBO '%s' + '%s' matched\n"), i, jp->idc.name, jp->idc.configname);
			}
		}
	}
	// configname next
	for (int i = 0; i < MAX_JPORTS; i++) {
		if (!matched[i]) {
			struct jport *jp = &jport_config_store[i];
			if (jp->idc.configname[0]) {
				if (inputdevice_joyport_config(p, NULL, jp->idc.configname, i, jp->mode, 1, userconfig)) {
					inputdevice_validate_jports(p, i, matched);
					inputdevice_store_used_device(&p->jports[i], i, defaultports);
					matched[i] = true;
					write_log(_T("Port%d: CONFIG '%s' matched\n"), i, jp->idc.configname);
				}
			}
		}
	}
	// friendly name next
	for (int i = 0; i < MAX_JPORTS; i++) {
		if (!matched[i]) {
			struct jport *jp = &jport_config_store[i];
			if (jp->idc.name[0]) {
				if (inputdevice_joyport_config(p, jp->idc.name, NULL, i, jp->mode, 1, userconfig)) {
					inputdevice_validate_jports(p, i, matched);
					inputdevice_store_used_device(&p->jports[i], i, defaultports);
					matched[i] = true;
					write_log(_T("Port%d: NAME '%s' matched\n"), i, jp->idc.name);
				}
			}
		}
	}
	// joyportX last and only if no name/configname
	for (int i = 0; i < MAX_JPORTS; i++) {
		if (!matched[i]) {
			struct jport *jp = &jport_config_store[i];
			if (jp->idc.shortid[0] && !jp->idc.name[0] && !jp->idc.configname[0]) {
				if (inputdevice_joyport_config(p, jp->idc.shortid, NULL, i, jp->mode, 0, userconfig)) {
					inputdevice_validate_jports(p, i, matched);
					inputdevice_store_used_device(&p->jports[i], i, defaultports);
					matched[i] = true;
					write_log(_T("Port%d: ID '%s' matched\n"), i, jp->idc.shortid);
				}
			}
			if (!matched[i]) {
				if (jp->idc.configname[0] && jp->idc.name[0]) {
					struct jport jpt = { 0 };
					memcpy(&jpt.idc, &jp->idc, sizeof(struct inputdevconfig));
					jpt.id = JPORT_UNPLUGGED;
					write_log(_T("Unplugged stored, port %d '%s' (%s)\n"), i, jp->idc.name, jp->idc.configname);
					inputdevice_store_used_device(&jpt, i, defaultports);
					freejport(p, i);
					inputdevice_get_previous_joy(p, i);
					matched[i] = true;
				}
			}
		}
	}
	for (int i = 0; i < MAX_JPORTS; i++) {
		if (!matched[i]) {
			struct jport *jp = &jport_config_store[i];
			freejport(p, i);
			if (jp->id != JPORT_NONE) {
				inputdevice_get_previous_joy(p, i);
				write_log(_T("Port%d: ID=%d getting previous: %d\n"), i, jp->id, p->jports[i].id);
			} else {
				write_log(_T("Port%d: NONE\n"), i);
			}
		}
	}
}
