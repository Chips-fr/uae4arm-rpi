#include <algorithm>
#include <iostream>
#include <vector>
#include <sstream>
#ifndef __LIBRETRO__
#include <guichan.hpp>
#include <guichan/sdl.hpp>
#endif
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "keybuf.h"
#include "zfile.h"
#include "gui.h"
#ifndef __LIBRETRO__
#include "od-pandora/gui/SelectorEntry.hpp"
#include "gui/gui_handling.h"
#endif
#include "memory.h"
#include "rommgr.h"
#include "newcpu.h"
#include "custom.h"
#include "inputdevice.h"
#include "xwin.h"
#include "drawing.h"
#include "sd-pandora/sound.h"
#include "audio.h"
#include "keybuf.h"
#include "keyboard.h"
#include "disk.h"
#include "savestate.h"
#include "filesys.h"
#include "autoconf.h"
#include "blkdev.h"
#include "SDL.h"
#include "td-sdl/thread.h"

#ifdef RASPBERRY
 #include <linux/kd.h>
 #include <sys/ioctl.h>
#endif

#ifdef __LIBRETRO__
// Todo: create specific file for libretro ?
#define MAX_HD_DEVICES 5
int currentStateNum = 0;

typedef struct {
  char Name[MAX_DPATH];
  char FullPath[MAX_DPATH];
  char Description[MAX_DPATH];
} ConfigFileInfo;
extern std::vector<ConfigFileInfo*> ConfigFilesList;
extern void ReadDirectory(const char *path, std::vector<std::string> *dirs, std::vector<std::string> *files);
extern void FilterFiles(std::vector<std::string> *files, const char *filter[]);

#define MAX_STARTUP_TITLE 64
#define MAX_STARTUP_MESSAGE 256
static TCHAR startup_title[MAX_STARTUP_TITLE] = _T("");
static TCHAR startup_message[MAX_STARTUP_MESSAGE] = _T("");

static char last_active_config[MAX_PATH] = { '\0' };

void target_startup_msg(TCHAR *title, TCHAR *msg)
{
  _tcsncpy(startup_title, title, MAX_STARTUP_TITLE);
  _tcsncpy(startup_message, msg, MAX_STARTUP_MESSAGE);
}

bool gui_running = false;

void gui_restart(void)
{
  gui_running = false;
}

void run_gui(void)
{
  gui_running = true;
}

void SetLastActiveConfig(const char *filename)
{
  extractFileName(filename, last_active_config);
  removeFileExtension(last_active_config);
}
#endif

int emulating = 0;

struct gui_msg {
  int num;
  const char *msg;
};
struct gui_msg gui_msglist[] = {
  { NUMSG_NEEDEXT2,       "The software uses a non-standard floppy disk format. You may need to use a custom floppy disk image file instead of a standard one. This message will not appear again." },
  { NUMSG_NOROM,          "Could not load system ROM, trying system ROM replacement." },
  { NUMSG_NOROMKEY,       "Could not find system ROM key file." },
  { NUMSG_KSROMCRCERROR,  "System ROM checksum incorrect. The system ROM image file may be corrupt." },
  { NUMSG_KSROMREADERROR, "Error while reading system ROM." },
  { NUMSG_NOEXTROM,       "No extended ROM found." },
  { NUMSG_KS68EC020,      "The selected system ROM requires a 68EC020 or later CPU." },
  { NUMSG_KS68020,        "The selected system ROM requires a 68020 or later CPU." },
  { NUMSG_KS68030,        "The selected system ROM requires a 68030 CPU." },
  { NUMSG_STATEHD,        "WARNING: Current configuration is not fully compatible with state saves." },
  { NUMSG_KICKREP,        "You need to have a floppy disk (image file) in DF0: to use the system ROM replacement." },
  { NUMSG_KICKREPNO,      "The floppy disk (image file) in DF0: is not compatible with the system ROM replacement functionality." },
  { NUMSG_ROMNEED,        "One of the following system ROMs is required:\n\n%s\n\nCheck the System ROM path in the Paths panel and click Rescan ROMs." },
  { NUMSG_EXPROMNEED,     "One of the following expansion boot ROMs is required:\n\n%s\n\nCheck the System ROM path in the Paths panel and click Rescan ROMs." },
  { NUMSG_NOMEMORY,       "Out of memory or too much Z3 autoconfig space configured." },

  { -1, "" }
};

std::vector<ConfigFileInfo*> ConfigFilesList;
std::vector<AvailableROM*> lstAvailableROMs;
std::vector<std::string> lstMRUDiskList;
std::vector<std::string> lstMRUCDList;


void AddFileToDiskList(const char *file, int moveToTop)
{
  int i;
  
  for(i=0; i<lstMRUDiskList.size(); ++i)
  {
    if(!strcasecmp(lstMRUDiskList[i].c_str(), file))
    {
      if(moveToTop)
      {
        lstMRUDiskList.erase(lstMRUDiskList.begin() + i);
        lstMRUDiskList.insert(lstMRUDiskList.begin(), file);
      }
      break;
    }
  }
  if(i >= lstMRUDiskList.size())
    lstMRUDiskList.insert(lstMRUDiskList.begin(), file);

  while(lstMRUDiskList.size() > MAX_MRU_DISKLIST)
    lstMRUDiskList.pop_back();
}


void AddFileToCDList(const char *file, int moveToTop)
{
  int i;
  
  for(i=0; i<lstMRUCDList.size(); ++i)
  {
    if(!strcasecmp(lstMRUCDList[i].c_str(), file))
    {
      if(moveToTop)
      {
        lstMRUCDList.erase(lstMRUCDList.begin() + i);
        lstMRUCDList.insert(lstMRUCDList.begin(), file);
      }
      break;
    }
  }
  if(i >= lstMRUCDList.size())
    lstMRUCDList.insert(lstMRUCDList.begin(), file);

  while(lstMRUCDList.size() > MAX_MRU_CDLIST)
    lstMRUCDList.pop_back();
}


void ClearAvailableROMList(void)
{
  while(lstAvailableROMs.size() > 0)
  {
    AvailableROM *tmp = lstAvailableROMs[0];
    lstAvailableROMs.erase(lstAvailableROMs.begin());
    delete tmp;
  }
}

static void addrom(struct romdata *rd, char *path)
{
  AvailableROM *tmp;
  char tmpName[MAX_DPATH];
  tmp = new AvailableROM();
  getromname(rd, tmpName);
  strncpy(tmp->Name, tmpName, MAX_PATH);
  if(path != NULL)
    strncpy(tmp->Path, path, MAX_PATH);
  tmp->ROMType = rd->type;
  lstAvailableROMs.push_back(tmp);
  romlist_add(path, rd);
}

struct romscandata {
    uae_u8 *keybuf;
    int keysize;
};

static struct romdata *scan_single_rom_2 (struct zfile *f)
{
  uae_u8 buffer[20] = { 0 };
  uae_u8 *rombuf;
  int cl = 0, size;
  struct romdata *rd = 0;

  zfile_fseek (f, 0, SEEK_END);
  size = zfile_ftell (f);
  zfile_fseek (f, 0, SEEK_SET);
  if (size > 524288 * 2) /* don't skip KICK disks or 1M ROMs */
  	return 0;
  zfile_fread (buffer, 1, 11, f);
  if (!memcmp (buffer, "KICK", 4)) {
	  zfile_fseek (f, 512, SEEK_SET);
	  if (size > 262144)
	    size = 262144;
  } else if (!memcmp (buffer, "AMIROMTYPE1", 11)) {
  	cl = 1;
	  size -= 11;
  } else {
	  zfile_fseek (f, 0, SEEK_SET);
  }
  rombuf = xcalloc (uae_u8, size);
  if (!rombuf)
  	return 0;
  zfile_fread (rombuf, 1, size, f);
  if (cl > 0) {
  	decode_cloanto_rom_do (rombuf, size, size);
	  cl = 0;
  }
  if (!cl) {
  	rd = getromdatabydata (rombuf, size);
  	if (!rd && (size & 65535) == 0) {
	    /* check byteswap */
	    int i;
	    for (i = 0; i < size; i+=2) {
    		uae_u8 b = rombuf[i];
    		rombuf[i] = rombuf[i + 1];
    		rombuf[i + 1] = b;
 	    }
 	    rd = getromdatabydata (rombuf, size);
  	}
  }
  free (rombuf);
  return rd;
}

static struct romdata *scan_single_rom (char *path)
{
    struct zfile *z;
    char tmp[MAX_DPATH];
    struct romdata *rd;

    strncpy (tmp, path, MAX_PATH);
    rd = getromdatabypath(path);
    if (rd && rd->crc32 == 0xffffffff)
	return rd;
    z = zfile_fopen (path, "rb", ZFD_NORMAL);
    if (!z)
	return 0;
    return scan_single_rom_2 (z);
}

static int isromext(char *path)
{
  char *ext;
  int i;

  if (!path)
	  return 0;
  ext = strrchr (path, '.');
  if (!ext)
  	return 0;
  ext++;

  if (!stricmp (ext, "rom") ||  !stricmp (ext, "adf") || !stricmp (ext, "key")
	|| !stricmp (ext, "a500") || !stricmp (ext, "a1200") || !stricmp (ext, "a4000"))
    return 1;
  for (i = 0; uae_archive_extensions[i]; i++) {
	  if (!stricmp (ext, uae_archive_extensions[i]))
	    return 1;
  }
  return 0;
}

static int scan_rom_2 (struct zfile *f, void *dummy)
{
  char *path = zfile_getname(f);
  struct romdata *rd;

  if (!isromext(path))
	  return 0;
  rd = scan_single_rom_2(f);
  if (rd)
    addrom (rd, path);
  return 0;
}

static void scan_rom(char *path)
{
  if (!isromext(path)) {
	  //write_log("ROMSCAN: skipping file '%s', unknown extension\n", path);
	  return;
  }
  zfile_zopen (path, scan_rom_2, 0);
}


void RescanROMs(void)
{
  std::vector<std::string> files;
  char path[MAX_DPATH];
  
  romlist_clear();
  
  ClearAvailableROMList();
  fetch_rompath(path, MAX_DPATH);
  
  load_keyring(&changed_prefs, path);
  ReadDirectory(path, NULL, &files);
  for(int i=0; i<files.size(); ++i)
  {
    char tmppath[MAX_PATH];
    strncpy(tmppath, path, MAX_PATH - 1);
    strncat(tmppath, files[i].c_str(), MAX_PATH - 1);
    scan_rom (tmppath);
  }
  
	int id = 1;
	for (;;) {
		struct romdata *rd = getromdatabyid (id);
		if (!rd)
			break;
		if (rd->crc32 == 0xffffffff && strncmp(rd->model, "AROS", 4) == 0)
			addrom (rd, ":AROS");
    if (rd->crc32 == 0xffffffff && rd->id == 63) {
      addrom (rd, ":HRTMon");
    }
		id++;
	}
}

static void ClearConfigFileList(void)
{
  while(ConfigFilesList.size() > 0)
  {
    ConfigFileInfo *tmp = ConfigFilesList[0];
    ConfigFilesList.erase(ConfigFilesList.begin());
    delete tmp;
  }
}


void ReadConfigFileList(void)
{
  char path[MAX_PATH];
  std::vector<std::string> files;
  const char *filter_rp9[] = { ".rp9", "\0" };
  const char *filter_uae[] = { ".uae", "\0" };
    
  ClearConfigFileList();
  
  // Read rp9 files
  fetch_rp9path(path, MAX_PATH);
  ReadDirectory(path, NULL, &files);
  FilterFiles(&files, filter_rp9);
  for (int i=0; i<files.size(); ++i)
  {
    ConfigFileInfo *tmp = new ConfigFileInfo();
    strncpy(tmp->FullPath, path, MAX_DPATH);
    strncat(tmp->FullPath, files[i].c_str(), MAX_DPATH);
    strncpy(tmp->Name, files[i].c_str(), MAX_DPATH);
    removeFileExtension(tmp->Name);
    strncpy(tmp->Description, _T("rp9"), MAX_PATH);
    ConfigFilesList.push_back(tmp);
  }
  
  // Read standard config files
  fetch_configurationpath(path, MAX_PATH);
  ReadDirectory(path, NULL, &files);
  FilterFiles(&files, filter_uae);
  for (int i=0; i<files.size(); ++i)
  {
    ConfigFileInfo *tmp = new ConfigFileInfo();
    strncpy(tmp->FullPath, path, MAX_DPATH);
    strncat(tmp->FullPath, files[i].c_str(), MAX_DPATH);
    strncpy(tmp->Name, files[i].c_str(), MAX_DPATH);
    removeFileExtension(tmp->Name);
    cfgfile_get_description(tmp->FullPath, tmp->Description);
    ConfigFilesList.push_back(tmp);
  }
}

ConfigFileInfo* SearchConfigInList(const char *name)
{
  for(int i=0; i<ConfigFilesList.size(); ++i)
  {
    if(!strncasecmp(ConfigFilesList[i]->Name, name, MAX_DPATH))
      return ConfigFilesList[i];
  }
  return NULL;
}


static void prefs_to_gui()
{
  /* filesys hack */
  changed_prefs.mountitems = currprefs.mountitems;
  memcpy(&changed_prefs.mountconfig, &currprefs.mountconfig, MOUNT_CONFIG_SIZE * sizeof (struct uaedev_config_info));
}


static void gui_to_prefs (void)
{
  /* filesys hack */
  currprefs.mountitems = changed_prefs.mountitems;
  memcpy(&currprefs.mountconfig, &changed_prefs.mountconfig, MOUNT_CONFIG_SIZE * sizeof (struct uaedev_config_info));
	fixup_prefs (&changed_prefs, true);
}


static void after_leave_gui(void)
{
  // Check if we have to set or clear autofire
  int new_af = (changed_prefs.input_autofire_linecnt == 0) ? 0 : 1;
  int update = 0;
  int num;
  
  for(num = 0; num < 2; ++num) {
    if(changed_prefs.jports[num].id == JSEM_JOYS && changed_prefs.jports[num].autofire != new_af) {
      changed_prefs.jports[num].autofire = new_af;
      update = 1;
    }
  }
  if(update)
    inputdevice_updateconfig(NULL, &changed_prefs);

  inputdevice_copyconfig (&changed_prefs, &currprefs);
  inputdevice_config_change_test();
}


int gui_init (void)
{
  int ret = 0;
  
	emulating=0;
	
  if(lstAvailableROMs.size() == 0)
    RescanROMs();

  graphics_subshutdown();
  prefs_to_gui();
  run_gui();
  gui_to_prefs();
  if(quit_program < 0)
    quit_program = -quit_program;
  if(quit_program == UAE_QUIT)
    ret = -2; // Quit without start of emulator

  update_display(&changed_prefs);

  after_leave_gui();
    
	emulating=1;
  return ret;
}

void gui_exit(void)
{
	sync();
	pandora_stop_sound();
	saveAdfDir();
	ClearConfigFileList();
	ClearAvailableROMList();
}


void gui_purge_events(void)
{
	int counter = 0;

#ifndef __LIBRETRO__
	SDL_Event event;
	SDL_Delay(150);
	// Strangely PS3 controller always send events, so we need a maximum number of event to purge.
	while(SDL_PollEvent(&event) && counter < 50)
	{
		counter++;
		SDL_Delay(10);
	}
#endif
	keybuf_init();
}


int gui_update (void)
{
#ifndef __LIBRETRO__
  char tmp[MAX_PATH];

  fetch_savestatepath(savestate_fname, MAX_DPATH);
  fetch_screenshotpath(screenshot_filename, MAX_DPATH);
  
  if(strlen(currprefs.floppyslots[0].df) > 0)
    extractFileName(currprefs.floppyslots[0].df, tmp);
  else
    strncpy(tmp, last_loaded_config, MAX_PATH);

  strncat(savestate_fname, tmp, MAX_DPATH - 1);
  strncat(screenshot_filename, tmp, MAX_DPATH - 1);
  removeFileExtension(savestate_fname);
  removeFileExtension(screenshot_filename);

  switch(currentStateNum)
  {
    case 1:
  		strncat(savestate_fname,"-1.uss", MAX_PATH - 1);
	    strncat(screenshot_filename,"-1.png", MAX_PATH - 1);
	    break;
    case 2:
  		strncat(savestate_fname,"-2.uss", MAX_PATH - 1);
  		strncat(screenshot_filename,"-2.png", MAX_PATH - 1);
  		break;
    case 3:
  		strncat(savestate_fname,"-3.uss", MAX_PATH - 1);
  		strncat(screenshot_filename,"-3.png", MAX_PATH - 1);
  		break;
    default: 
	   	strncat(savestate_fname,".uss", MAX_PATH - 1);
  		strncat(screenshot_filename,".png", MAX_PATH - 1);
  }
#endif
  return 0;
}


void gui_display (int shortcut)
{
	if (quit_program != 0)
		return;
	emulating=1;
	pause_sound();
  blkdev_entergui();

  if(lstAvailableROMs.size() == 0)
    RescanROMs();
  graphics_subshutdown();
  prefs_to_gui();
  run_gui();
  gui_to_prefs();
//	if(quit_program)
//		screen_is_picasso = 0;

  update_display(&changed_prefs);

	/* Clear menu garbage at the bottom of the screen */
	black_screen_now();
	/* Empty audio buffer */
	reset_sound();
	resume_sound();
  blkdev_exitgui();

  after_leave_gui();

  gui_update ();

  gui_purge_events();
  fpscounter_reset();
}

  
void moveVertical(int value)
{
	changed_prefs.pandora_vertical_offset += value;
	if(changed_prefs.pandora_vertical_offset < -16 + OFFSET_Y_ADJUST)
		changed_prefs.pandora_vertical_offset = -16 + OFFSET_Y_ADJUST;
	else if(changed_prefs.pandora_vertical_offset > 16 + OFFSET_Y_ADJUST)
		changed_prefs.pandora_vertical_offset = 16 + OFFSET_Y_ADJUST;
}


extern char keyboard_type;

void gui_handle_events (void)
{
#ifndef __LIBRETRO__
	Uint8 *keystate = SDL_GetKeyState(NULL);

	// Strangely in FBCON left window is seen as left alt ??
	if (keyboard_type == 2) // KEYCODE_FBCON
	{
		if(keystate[SDLK_LCTRL] && (keystate[SDLK_LSUPER] || keystate[SDLK_LALT]) && (keystate[SDLK_RSUPER] ||keystate[SDLK_MENU]))
			uae_reset(0,1);
	} else
	{
		if(keystate[SDLK_LCTRL] && keystate[SDLK_LSUPER] && (keystate[SDLK_RSUPER] ||keystate[SDLK_MENU]))
			uae_reset(0,1);
	}
#endif
}

void gui_disk_image_change (int unitnum, const char *name, bool writeprotected)
{
}

void gui_led (int led, int on, int brightness)
{
#ifdef RASPBERRY
   #define LED_ALL   -1         // Define for all LEDs
   
   unsigned char kbd_led_status;
   
   // Check current prefs/ update if changed
   if (currprefs.kbd_led_num != changed_prefs.kbd_led_num) currprefs.kbd_led_num = changed_prefs.kbd_led_num;
   if (currprefs.kbd_led_scr != changed_prefs.kbd_led_scr) currprefs.kbd_led_scr = changed_prefs.kbd_led_scr;
   if (currprefs.kbd_led_cap != changed_prefs.kbd_led_cap) currprefs.kbd_led_cap = changed_prefs.kbd_led_cap;
   
   ioctl(0, KDGETLED, &kbd_led_status);
   
	// Handle floppy led status
	if (led == LED_DF0 || led == LED_DF1 || led == LED_DF2 || led == LED_DF3)
	{ 
		if (currprefs.kbd_led_num == led)
		{  
			if (on) 
			  kbd_led_status |= LED_NUM;
			else 
			  kbd_led_status &= ~LED_NUM;
		}
		if (currprefs.kbd_led_scr == led)
		{  
			if (on) 
			  kbd_led_status |= LED_SCR;
			else 
			  kbd_led_status &= ~LED_SCR;
		}
	}
   
	// Handle power, hd/cd led status
	if (led == LED_POWER || led == LED_HD || led == LED_CD)
	{ 
		if (currprefs.kbd_led_num == led)
		{   
			if (on) 
			  kbd_led_status |= LED_NUM;
			else 
			  kbd_led_status &= ~LED_NUM;
		}
		if (currprefs.kbd_led_scr == led)
		{   
			if (on) 
			  kbd_led_status |= LED_SCR;
			else 
			  kbd_led_status &= ~LED_SCR;
		}
	}
  
  // Handle all LEDs off
  if (led == LED_ALL) {
     kbd_led_status &= ~LED_NUM;
     kbd_led_status &= ~LED_SCR;
  }
  ioctl(0, KDSETLED, kbd_led_status);
#endif
}

void gui_flicker_led (int led, int unitnum, int status)
{
  static int hd_resetcounter , cd_resetcounter;

  switch(led)
  {
    case -1: // Reset HD and CD
      gui_data.hd = 0;
      break;
      
    case LED_POWER:
      break;

    case LED_HD:
      if (status == 0) {
  	    hd_resetcounter--;
  	    if (hd_resetcounter > 0)
  	      return;
      }
      gui_data.hd = status;
      hd_resetcounter = 2;
      break;

    case LED_CD:
      if (status == 0) {
  	    cd_resetcounter--;
  	    if (cd_resetcounter > 0)
  	      return;
      }
      gui_data.cd = status;
      cd_resetcounter = 2;
      break;
  }
#ifdef RASPBERRY
   gui_led(led, status , -1);
#endif
}


void gui_filename (int num, const char *name)
{
}

void gui_message (const char *format,...)
{
  char msg[2048];
  va_list parms;

  va_start (parms, format);
  vsprintf( msg, format, parms );
  va_end (parms);

  target_startup_msg("Error", msg);
  uae_restart(1, NULL);
}

void notify_user (int msg)
{
  int i=0;
  while(gui_msglist[i].num >= 0)
  {
    if(gui_msglist[i].num == msg)
    {
      gui_message(gui_msglist[i].msg);
      break;
    }
    ++i;
  }
}

void notify_user_parms (int msg, const TCHAR *parms, ...)
{
	TCHAR msgtxt[MAX_DPATH];
	TCHAR tmp[MAX_DPATH];
	int c = 0;
	va_list parms2;

  int i=0;
  while(gui_msglist[i].num >= 0)
  {
    if(gui_msglist[i].num == msg)
    {
      strncpy(tmp, gui_msglist[i].msg, MAX_DPATH);
    	va_start (parms2, parms);
    	_vsntprintf (msgtxt, sizeof msgtxt / sizeof (TCHAR), tmp, parms2);
      gui_message(msgtxt);
    	va_end (parms2);
      break;
    }
    ++i;
  }
}


int translate_message (int msg,	TCHAR *out)
{
  int i=0;
  while(gui_msglist[i].num >= 0)
  {
    if(gui_msglist[i].num == msg)
    {
      strncpy(out, gui_msglist[i].msg, MAX_DPATH);
      return 1;
    }
    ++i;
  }
  return 0;
}


void FilterFiles(std::vector<std::string> *files, const char *filter[])
{
  for (int q=0; q<files->size(); q++)
  {
    std::string tmp = (*files)[q];
    
    bool bRemove = true;
    for(int f=0; filter[f] != NULL && strlen(filter[f]) > 0; ++f)
    {
      if(tmp.size() >= strlen(filter[f]))
      {
        if(!strcasecmp(tmp.substr(tmp.size() - strlen(filter[f])).c_str(), filter[f]))
        {
          bRemove = false;
          break;
        }
      }
    }
    
    if(bRemove)
    {
      files->erase(files->begin() + q);
      --q;
    }
  }  
}


bool DevicenameExists(const char *name)
{
  int i;
  struct uaedev_config_data *uci;
  struct uaedev_config_info *ci;
  
  for(i=0; i<MAX_HD_DEVICES; ++i)
  {
    uci = &changed_prefs.mountconfig[i];
    ci = &uci->ci;
    
    if(ci->devname && ci->devname[0])
    {
      if(!strcmp(ci->devname, name))
        return true;
      if(ci->volname != 0 && !strcmp(ci->volname, name))
        return true;
    }
  }
  return false;
}


void CreateDefaultDevicename(char *name)
{
  int freeNum = 0;
  bool foundFree = false;
  
  while(!foundFree && freeNum < 10)
  {
    sprintf(name, "DH%d", freeNum);
    foundFree = !DevicenameExists(name);
    ++freeNum;
  }
}


int tweakbootpri (int bp, int ab, int dnm)
{
  if (dnm)
  	return BOOTPRI_NOAUTOMOUNT;
  if (!ab)
  	return BOOTPRI_NOAUTOBOOT;
  if (bp < -127)
  	bp = -127;
  return bp;
}


bool hardfile_testrdb (const TCHAR *filename)
{
	bool isrdb = false;
	struct zfile *f = zfile_fopen (filename, _T("rb"), ZFD_NORMAL);
	uae_u8 tmp[8];
	int i;

	if (!f)
		return false;
	for (i = 0; i < 16; i++) {
		zfile_fseek (f, i * 512, SEEK_SET);
		memset (tmp, 0, sizeof tmp);
		zfile_fread (tmp, 1, sizeof tmp, f);
		if (!memcmp (tmp, "RDSK\0\0\0", 7) || !memcmp (tmp, "DRKS\0\0", 6) || (tmp[0] == 0x53 && tmp[1] == 0x10 && tmp[2] == 0x9b && tmp[3] == 0x13 && tmp[4] == 0 && tmp[5] == 0)) {
			// RDSK or ADIDE "encoded" RDSK
			isrdb = true;
			break;
		}
	}
	zfile_fclose (f);
  return isrdb;
}
