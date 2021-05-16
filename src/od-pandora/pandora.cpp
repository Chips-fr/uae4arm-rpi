/*
 * UAE - The Un*x Amiga Emulator
 *
 * Pandora interface
 *
 */

#include <algorithm>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdarg.h>
#include <asm/sigcontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <execinfo.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "td-sdl/thread.h"
#include "gui.h"
#include "memory.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "disk.h"
#include "savestate.h"
#include "rtgmodes.h"
#include "rommgr.h"
#include "zfile.h"
#include "gfxboard.h"
#include <SDL.h>
#include "pandora_rp9.h"

// Temporary reintroduce reinit_amiga
#include "newcpu.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "autoconf.h"
#include "custom.h"
#include "keybuf.h"
#include "bsdsocket.h"
#include "blkdev.h"
#include "native2amiga.h"
#include "uaeresource.h"
#include "akiko.h"
// Temporary reintroduce reinit_amiga

#ifdef WITH_LOGGING
extern FILE *debugfile;
#endif

int quickstart_start = 1;
int quickstart_model = 0;
int quickstart_conf = 0;

extern void signal_segv(int signum, siginfo_t* info, void*ptr);
extern void signal_buserror(int signum, siginfo_t* info, void*ptr);
extern void signal_term(int signum, siginfo_t* info, void*ptr);
extern void gui_force_rtarea_hdchange(void);

static int delayed_mousebutton = 0;
static int doStylusRightClick;

extern void SetLastActiveConfig(const char *filename);

/* Keyboard */
int customControlMap[SDLK_LAST];

char start_path_data[MAX_DPATH];
char currentDir[MAX_DPATH];
#ifdef CAPSLOCK_DEBIAN_WORKAROUND
  #include <linux/kd.h>
  #include <sys/ioctl.h>
  unsigned char kbd_led_status;
  char kbd_flags;
#endif

static char config_path[MAX_DPATH];
static char rom_path[MAX_DPATH];
static char rp9_path[MAX_DPATH];
char last_loaded_config[MAX_DPATH] = { '\0' };

static bool cpuSpeedChanged = false;
static int lastCpuSpeed = 600;
int defaultCpuSpeed = 600;

int max_uae_width;
int max_uae_height;

#ifdef __LIBRETRO__
#include "core-log.h"
#include "libco/libco.h"

extern cothread_t mainThread;
extern cothread_t emuThread;
extern char RETRO_DIR[512];
#endif

extern "C" int main( int argc, char *argv[] );


void reinit_amiga(void)
{
  write_log("reinit_amiga() called\n");
  DISK_free ();
#ifdef CD32
	akiko_free ();
#endif
#ifdef FILESYS
  filesys_cleanup ();
  hardfile_reset();
#endif
#ifdef AUTOCONFIG
#if defined (BSDSOCKET)
  bsdlib_reset();
#endif
  expansion_cleanup ();
#endif
	device_func_reset ();
  memory_cleanup ();
  
  /* At this point, there might run some threads from bsdsocket.*/
//  write_log("Threads in reinit_amiga():\n");
//  dbg_list_threads();

  init_mem_banks ();

  currprefs = changed_prefs;
  /* force sound settings change */
  currprefs.produce_sound = 0;

  HandleA3000Mem(currprefs.mbresmem_low_size, currprefs.mbresmem_high_size);
  
  framecnt = 1;
#ifdef AUTOCONFIG
  rtarea_setup ();
#endif
#ifdef FILESYS
  rtarea_init ();
  uaeres_install ();
  hardfile_install();
#endif
  keybuf_init();

#ifdef AUTOCONFIG
  expansion_init ();
#endif
#ifdef FILESYS
  filesys_install (); 
#endif
  memory_init ();
  memory_reset ();

#ifdef AUTOCONFIG
#if defined (BSDSOCKET)
	bsdlib_install ();
#endif
  emulib_install ();
  native2amiga_install ();
#endif

  custom_init (); /* Must come after memory_init */
  DISK_init ();
  
  reset_frame_rate_hack ();
  init_m68k();
}


void sleep_millis (int ms)
{
  usleep(ms * 1000);
}

int sleep_millis_main (int ms)
{
	uae_u32 start = read_processor_time ();
  usleep(ms * 1000);
  idletime += read_processor_time () - start;
  return 0;
}


void logging_init( void )
{
#ifdef WITH_LOGGING
  static int started;
  static int first;
  char debugfilename[MAX_DPATH];

  if (first > 1) {
  	write_log ("***** RESTART *****\n");
	  return;
  }
  if (first == 1) {
  	if (debugfile)
	    fclose (debugfile);
    debugfile = 0;
  }

    sprintf(debugfilename, "%s/uae4arm_log.txt", start_path_data);
    if(!debugfile)
    debugfile = fopen(debugfilename, "wt");

  first++;
  write_log ( "UAE4ARM Logfile\n\n");
#endif
}

void logging_cleanup( void )
{
#ifdef WITH_LOGGING
  if(debugfile)
    fclose(debugfile);
  debugfile = 0;
#endif
}


void stripslashes (TCHAR *p)
{
	while (_tcslen (p) > 0 && (p[_tcslen (p) - 1] == '\\' || p[_tcslen (p) - 1] == '/'))
		p[_tcslen (p) - 1] = 0;
}
void fixtrailing (TCHAR *p)
{
	if (_tcslen(p) == 0)
		return;
	if (p[_tcslen(p) - 1] == '/' || p[_tcslen(p) - 1] == '\\')
		return;
	_tcscat(p, "/");
}

void getpathpart (TCHAR *outpath, int size, const TCHAR *inpath)
{
	_tcscpy (outpath, inpath);
	TCHAR *p = _tcsrchr (outpath, '/');
	if (p)
		p[0] = 0;
	fixtrailing (outpath);
}
void getfilepart (TCHAR *out, int size, const TCHAR *path)
{
	out[0] = 0;
	const TCHAR *p = _tcsrchr (path, '/');
	if (p)
		_tcscpy (out, p + 1);
	else
		_tcscpy (out, path);
}


uae_u8 *target_load_keyfile (struct uae_prefs *p, const char *path, int *sizep, char *name)
{
  return 0;
}


void target_run (void)
{
  // Reset counter for access violations
  init_max_signals();
}

void target_quit (void)
{
}


static void fix_apmodes(struct uae_prefs *p)
{
  if(p->ntscmode)
  {
    p->gfx_apmode[0].gfx_refreshrate = 60;
    p->gfx_apmode[1].gfx_refreshrate = 60;
  }  
  else
  {
    p->gfx_apmode[0].gfx_refreshrate = 50;
    p->gfx_apmode[1].gfx_refreshrate = 50;
  }  

  p->gfx_apmode[0].gfx_vsync = 2;
  p->gfx_apmode[1].gfx_vsync = 2;
  p->gfx_apmode[0].gfx_vflip = -1;
  p->gfx_apmode[1].gfx_vflip = -1;
  
	fixup_prefs_dimensions (p);
}


void target_fixup_options (struct uae_prefs *p)
{
	p->rtgboards[0].rtgmem_type = GFXBOARD_UAE_Z3;

	if(z3base_adr == Z3BASE_REAL) {
  	// map Z3 memory at real address (0x40000000)
  	p->z3_mapping_mode = Z3MAPPING_REAL;
    p->z3autoconfig_start = z3base_adr;
	} else {
	  // map Z3 memory at UAE address (0x10000000)
  	p->z3_mapping_mode = Z3MAPPING_UAE;
    p->z3autoconfig_start = z3base_adr;
	}

  if(p->cs_cd32cd && p->cs_cd32nvram && (p->cs_compatible == CP_GENERIC || p->cs_compatible == 0)) {
    // Old config without cs_compatible, but other cd32-flags
    p->cs_compatible = CP_CD32;
    built_in_chipset_prefs(p);
  }

  if(p->cs_cd32cd && p->cartfile[0]) {
    p->cs_cd32fmv = 1;
  }
  
	p->picasso96_modeflags = RGBFF_CLUT | RGBFF_R5G6B5 | RGBFF_R8G8B8A8;
  p->gfx_resolution = p->gfx_size.width > 600 ? 1 : 0;
  
  if(p->cachesize > 0)
    p->fpu_no_unimplemented = 0;
  else
    p->fpu_no_unimplemented = 1;
  
  fix_apmodes(p);
}


void target_default_options (struct uae_prefs *p, int type)
{
  p->pandora_vertical_offset = OFFSET_Y_ADJUST;
  p->pandora_cpu_speed = defaultCpuSpeed;
  p->pandora_hide_idle_led = 0;
  
  p->pandora_tapDelay = 10;
	p->pandora_customControls = 0;

	p->picasso96_modeflags = RGBFF_CLUT | RGBFF_R5G6B5 | RGBFF_R8G8B8A8;
	
	memset(customControlMap, 0, sizeof(customControlMap));
	
	p->cr[CHIPSET_REFRESH_PAL].locked = true;
	p->cr[CHIPSET_REFRESH_PAL].vsync = 1;

	p->cr[CHIPSET_REFRESH_NTSC].locked = true;
	p->cr[CHIPSET_REFRESH_NTSC].vsync = 1;
	
	p->cr[0].index = 0;
	p->cr[0].horiz = -1;
	p->cr[0].vert = -1;
	p->cr[0].lace = -1;
	p->cr[0].resolution = 0;
	p->cr[0].vsync = -1;
	p->cr[0].rate = 60.0;
	p->cr[0].ntsc = 1;
	p->cr[0].locked = true;
	p->cr[0].rtg = true;
	_tcscpy (p->cr[0].label, _T("RTG"));
}


void target_save_options (struct zfile *f, struct uae_prefs *p)
{
  cfgfile_write (f, "pandora.cpu_speed", "%d", p->pandora_cpu_speed);
  cfgfile_write (f, "pandora.hide_idle_led", "%d", p->pandora_hide_idle_led);
  cfgfile_write (f, "pandora.tap_delay", "%d", p->pandora_tapDelay);
  cfgfile_write (f, "pandora.custom_controls", "%d", p->pandora_customControls);
  cfgfile_write (f, "pandora.custom_up", "%d", customControlMap[VK_UP]);
  cfgfile_write (f, "pandora.custom_down", "%d", customControlMap[VK_DOWN]);
  cfgfile_write (f, "pandora.custom_left", "%d", customControlMap[VK_LEFT]);
  cfgfile_write (f, "pandora.custom_right", "%d", customControlMap[VK_RIGHT]);
  cfgfile_write (f, "pandora.custom_a", "%d", customControlMap[VK_A]);
  cfgfile_write (f, "pandora.custom_b", "%d", customControlMap[VK_B]);
  cfgfile_write (f, "pandora.custom_x", "%d", customControlMap[VK_X]);
  cfgfile_write (f, "pandora.custom_y", "%d", customControlMap[VK_Y]);
  cfgfile_write (f, "pandora.custom_l", "%d", customControlMap[VK_L]);
  cfgfile_write (f, "pandora.custom_r", "%d", customControlMap[VK_R]);
  cfgfile_write (f, "pandora.move_y", "%d", p->pandora_vertical_offset - OFFSET_Y_ADJUST);
}


void target_restart (void)
{
  emulating = 0;
  gui_restart();
}


TCHAR *target_expand_environment (const TCHAR *path, TCHAR *out, int maxlen)
{
  if(out == NULL) {
    return strdup(path);
  } else {
    _tcscpy(out, path);
    return out;
  }
}

int target_parse_option (struct uae_prefs *p, const char *option, const char *value)
{
  int result = (cfgfile_intval (option, value, "cpu_speed", &p->pandora_cpu_speed, 1)
    || cfgfile_intval (option, value, "hide_idle_led", &p->pandora_hide_idle_led, 1)
    || cfgfile_intval (option, value, "tap_delay", &p->pandora_tapDelay, 1)
    || cfgfile_intval (option, value, "custom_controls", &p->pandora_customControls, 1)
    || cfgfile_intval (option, value, "custom_up", &customControlMap[VK_UP], 1)
    || cfgfile_intval (option, value, "custom_down", &customControlMap[VK_DOWN], 1)
    || cfgfile_intval (option, value, "custom_left", &customControlMap[VK_LEFT], 1)
    || cfgfile_intval (option, value, "custom_right", &customControlMap[VK_RIGHT], 1)
    || cfgfile_intval (option, value, "custom_a", &customControlMap[VK_A], 1)
    || cfgfile_intval (option, value, "custom_b", &customControlMap[VK_B], 1)
    || cfgfile_intval (option, value, "custom_x", &customControlMap[VK_X], 1)
    || cfgfile_intval (option, value, "custom_y", &customControlMap[VK_Y], 1)
    || cfgfile_intval (option, value, "custom_l", &customControlMap[VK_L], 1)
    || cfgfile_intval (option, value, "custom_r", &customControlMap[VK_R], 1)
    );
  if(!result) {
    result = cfgfile_intval (option, value, "move_y", &p->pandora_vertical_offset, 1);
    if(result)
      p->pandora_vertical_offset += OFFSET_Y_ADJUST;
  }

  return result;
}


void fetch_datapath (char *out, int size)
{
  strncpy(out, start_path_data, size);
  strncat(out, "/", size);
}


void fetch_saveimagepath (char *out, int size, int dir)
{
  strncpy(out, start_path_data, size);
  strncat(out, "/savestates/", size);
}


void fetch_configurationpath (char *out, int size)
{
  strncpy(out, config_path, size);
}


void set_configurationpath(char *newpath)
{
  strncpy(config_path, newpath, MAX_DPATH);
}


void fetch_rompath (char *out, int size)
{
  strncpy(out, rom_path, size);
}


void set_rompath(char *newpath)
{
  strncpy(rom_path, newpath, MAX_DPATH);
}


void fetch_rp9path (char *out, int size)
{
  strncpy(out, rp9_path, size);
}


void fetch_savestatepath(char *out, int size)
{
  strncpy(out, start_path_data, size);
  strncat(out, "/savestates/", size);
}


void fetch_screenshotpath(char *out, int size)
{
  strncpy(out, start_path_data, size);
  strncat(out, "/screenshots/", size);
}


int target_cfgfile_load (struct uae_prefs *p, const char *filename, int type, int isdefault)
{
  int i;
  int result = 0;

  write_log(_T("target_cfgfile_load(): load file %s\n"), filename);
  
  discard_prefs(p, type);
#ifndef __LIBRETRO__
  default_prefs(p, true, 0);
#endif
  
	char *ptr;
#ifdef RP9_SUPPORT
	ptr = strstr((char *)filename, ".rp9");
  if(ptr > 0)
  {
    // Load rp9 config
    result = rp9_parse_file(p, filename);
    if(result)
      extractFileName(filename, last_loaded_config);
  }
  else 
#endif
	{
  	ptr = strstr((char *)filename, ".uae");
    if(ptr > 0)
    {
      int type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
      result = cfgfile_load(p, filename, &type, 0, 1);
    }
    if(result)
      extractFileName(filename, last_loaded_config);
  }

  if(result)
  {
    for(i=0; i < p->nr_floppies; ++i)
    {
      if(!DISK_validate_filename(p, p->floppyslots[i].df, 0, NULL, NULL, NULL))
        p->floppyslots[i].df[0] = 0;
      disk_insert(i, p->floppyslots[i].df);
      if(strlen(p->floppyslots[i].df) > 0)
        AddFileToDiskList(p->floppyslots[i].df, 1);
    }

    if(!isdefault)
      inputdevice_updateconfig (NULL, p);
#ifdef WITH_LOGGING
    p->leds_on_screen = true;
#endif
    SetLastActiveConfig(filename);
    quickstart_model = -1;
  }

  return result;
}


int check_configfile(char *file)
{
  char tmp[MAX_PATH];
  
  FILE *f = fopen(file, "rt");
  if(f)
  {
    fclose(f);
    return 1;
  }
  
  strncpy(tmp, file, MAX_PATH);
	char *ptr = strstr(tmp, ".uae");
	if(ptr > 0)
  {
    *(ptr + 1) = '\0';
    strncat(tmp, "conf", MAX_PATH);
    f = fopen(tmp, "rt");
    if(f)
    {
      fclose(f);
      return 2;
    }
  }

  return 0;
}


void extractFileName(const char * str,char *buffer)
{
	const char *p=str+strlen(str)-1;
	while(*p != '/' && p > str)
		p--;
	p++;
	strncpy(buffer,p, MAX_PATH);
}


void extractPath(char *str, char *buffer)
{
	strncpy(buffer, str, MAX_PATH);
	char *p = buffer + strlen(buffer) - 1;
	while(*p != '/' && p > buffer)
		p--;
	p[1] = '\0';
}


void removeFileExtension(char *filename)
{
  char *p = filename + strlen(filename) - 1;
  while(p > filename && *p != '.')
  {
    *p = '\0';
    --p;
  }
  *p = '\0';
}


void ReadDirectory(const char *path, std::vector<std::string> *dirs, std::vector<std::string> *files)
{
  DIR *dir;
  struct dirent *dent;

  if(dirs != NULL)
    dirs->clear();
  if(files != NULL)
    files->clear();
  
  dir = opendir(path);
  if(dir != NULL)
  {
    while((dent = readdir(dir)) != NULL)
    {
      if(dent->d_type == DT_DIR)
      {
        if(dirs != NULL)
          dirs->push_back(dent->d_name);
      }
      else if (files != NULL)
        files->push_back(dent->d_name);
    }
    if(dirs != NULL && dirs->size() > 0 && (*dirs)[0] == ".")
      dirs->erase(dirs->begin());
    closedir(dir);
  }
  
  if(dirs != NULL)
    std::sort(dirs->begin(), dirs->end());
  if(files != NULL)
    std::sort(files->begin(), files->end());
}


void saveAdfDir(void)
{
	char path[MAX_DPATH];
	int i;
	
	snprintf(path, MAX_DPATH, "%s/conf/adfdir.conf", start_path_data);
	FILE *f=fopen(path,"w");
	if (!f)
	  return;
	  
	char buffer[MAX_DPATH];
	snprintf(buffer, MAX_DPATH, "path=%s\n", currentDir);
	fputs(buffer,f);

	snprintf(buffer, MAX_DPATH, "config_path=%s\n", config_path);
	fputs(buffer, f);

	snprintf(buffer, MAX_DPATH, "rom_path=%s\n", rom_path);
	fputs(buffer, f);

  snprintf(buffer, MAX_DPATH, "ROMs=%d\n", lstAvailableROMs.size());
  fputs(buffer, f);
  for(i=0; i<lstAvailableROMs.size(); ++i)
  {
    snprintf(buffer, MAX_DPATH, "ROMName=%s\n", lstAvailableROMs[i]->Name);
    fputs(buffer, f);
    snprintf(buffer, MAX_DPATH, "ROMPath=%s\n", lstAvailableROMs[i]->Path);
    fputs(buffer, f);
    snprintf(buffer, MAX_DPATH, "ROMType=%d\n", lstAvailableROMs[i]->ROMType);
    fputs(buffer, f);
  }
  
  snprintf(buffer, MAX_DPATH, "MRUDiskList=%d\n", lstMRUDiskList.size());
  fputs(buffer, f);
  for(i=0; i<lstMRUDiskList.size(); ++i)
  {
    snprintf(buffer, MAX_DPATH, "Diskfile=%s\n", lstMRUDiskList[i].c_str());
    fputs(buffer, f);
  }

  snprintf(buffer, MAX_DPATH, "MRUCDList=%d\n", lstMRUCDList.size());
  fputs(buffer, f);
  for(i=0; i<lstMRUCDList.size(); ++i)
  {
    snprintf(buffer, MAX_DPATH, "CDfile=%s\n", lstMRUCDList[i].c_str());
    fputs(buffer, f);
  }

  snprintf(buffer, MAX_DPATH, "Quickstart=%d\n", quickstart_start);
  fputs(buffer, f);

	fclose(f);
	return;
}


void get_string(FILE *f, char *dst, int size)
{
  char buffer[MAX_PATH];
  fgets(buffer, MAX_PATH, f);
  int i = strlen (buffer);
  while (i > 0 && (buffer[i - 1] == '\t' || buffer[i - 1] == ' ' 
  || buffer[i - 1] == '\r' || buffer[i - 1] == '\n'))
	  buffer[--i] = '\0';
  strncpy(dst, buffer, size);
}


static void trimwsa (char *s)
{
  /* Delete trailing whitespace.  */
  int len = strlen (s);
  while (len > 0 && strcspn (s + len - 1, "\t \r\n") == 0)
    s[--len] = '\0';
}


void loadAdfDir(void)
{
	char path[MAX_DPATH];
  int i;

	strncpy(currentDir, start_path_data, MAX_DPATH);
	snprintf(config_path, MAX_DPATH, "%s/conf/", start_path_data);
	snprintf(rom_path, MAX_DPATH, "%s/kickstarts/", start_path_data);
	snprintf(rp9_path, MAX_DPATH, "%s/rp9/", start_path_data);

	snprintf(path, MAX_DPATH, "%s/conf/adfdir.conf", start_path_data);
  struct zfile *fh;
  fh = zfile_fopen (path, _T("r"), ZFD_NORMAL);
  if (fh) {
    char linea[CONFIG_BLEN];
    TCHAR option[CONFIG_BLEN], value[CONFIG_BLEN];
    int numROMs, numDisks, numCDs;
    int romType = -1;
    char romName[MAX_PATH] = { '\0' };
    char romPath[MAX_PATH] = { '\0' };
    char tmpFile[MAX_PATH];
    
    while (zfile_fgetsa (linea, sizeof (linea), fh) != 0) {
    	trimwsa (linea);
    	if (strlen (linea) > 0) {
  	    if (!cfgfile_separate_linea (path, linea, option, value))
      		continue;
        
        if(cfgfile_string(option, value, "ROMName", romName, sizeof(romName))
        || cfgfile_string(option, value, "ROMPath", romPath, sizeof(romPath))
        || cfgfile_intval(option, value, "ROMType", &romType, 1)) {
          if(strlen(romName) > 0 && strlen(romPath) > 0 && romType != -1) {
            AvailableROM *tmp = new AvailableROM();
            strncpy(tmp->Name, romName, sizeof(tmp->Name));
            strncpy(tmp->Path, romPath, sizeof(tmp->Path));
            tmp->ROMType = romType;
            lstAvailableROMs.push_back(tmp);
            strncpy(romName, "", sizeof(romName));
            strncpy(romPath, "", sizeof(romPath));
            romType = -1;
          }
        } else if (cfgfile_string(option, value, "Diskfile", tmpFile, sizeof(tmpFile))) {
          FILE *f = fopen(tmpFile, "rb");
          if(f != NULL) {
            fclose(f);
            lstMRUDiskList.push_back(tmpFile);
          }
        } else if (cfgfile_string(option, value, "CDfile", tmpFile, sizeof(tmpFile))) {
          FILE *f = fopen(tmpFile, "rb");
          if(f != NULL) {
            fclose(f);
            lstMRUCDList.push_back(tmpFile);
          }
        } else {
          cfgfile_string(option, value, "path", currentDir, sizeof(currentDir));
          cfgfile_string(option, value, "config_path", config_path, sizeof(config_path));
          cfgfile_string(option, value, "rom_path", rom_path, sizeof(rom_path));
          cfgfile_intval(option, value, "ROMs", &numROMs, 1);
          cfgfile_intval(option, value, "MRUDiskList", &numDisks, 1);
          cfgfile_intval(option, value, "MRUCDList", &numCDs, 1);
          cfgfile_intval(option, value, "Quickstart", &quickstart_start, 1);
        }
    	}
    }
    zfile_fclose (fh);
  }
}


int currVSyncRate = 0;
bool SetVSyncRate(int hz)
{
#ifdef PANDORA_SPECIFIC
	char cmd[64];

  if(currVSyncRate != hz && (hz == 50 || hz == 60))
  {
    snprintf((char*)cmd, 64, "sudo /usr/pandora/scripts/op_lcdrate.sh %d", hz);
    system(cmd);
    currVSyncRate = hz;
    return true;
  }
#endif
  return false;
}

void setCpuSpeed()
{
#ifdef PANDORA_SPECIFIC
  char speedCmd[128];

  currprefs.pandora_cpu_speed = changed_prefs.pandora_cpu_speed;

	if(currprefs.pandora_cpu_speed != lastCpuSpeed)
	{
		snprintf((char*)speedCmd, 128, "unset DISPLAY; echo y | sudo -n /usr/pandora/scripts/op_cpuspeed.sh %d", currprefs.pandora_cpu_speed);
		system(speedCmd);
		lastCpuSpeed = currprefs.pandora_cpu_speed;
		cpuSpeedChanged = true;
	}
	if(changed_prefs.ntscmode != currprefs.ntscmode)
	{
		if(changed_prefs.ntscmode)
			SetVSyncRate(60);
		else
			SetVSyncRate(50);
		fix_apmodes(&changed_prefs);
	}
#else
  return;
#endif
}


int getDefaultCpuSpeed(void)
{
#ifdef PANDORA_SPECIFIC
  int speed = 600;
  FILE* f = fopen ("/etc/pandora/conf/cpu.conf", "rt");
  if(f)
  {
    char line[128];
    for(int i=0; i<6; ++i)
    {
      fscanf(f, "%s\n", &line);
      if(strncmp(line, "default:", 8) == 0)
      {
        int value = 0;
        sscanf(line, "default:%d", &value);
        if(value > 500 && value < 1200)
        {
          speed = value;
        }
      }
    }
    fclose(f);
  }
  return speed;
#else
  return 0;
#endif
}


void resetCpuSpeed(void)
{
#ifdef PANDORA_SPECIFIC
  if(cpuSpeedChanged)
  {
    lastCpuSpeed = defaultCpuSpeed - 10;
    currprefs.pandora_cpu_speed = changed_prefs.pandora_cpu_speed = defaultCpuSpeed;
    setCpuSpeed();
  }
#else
  return;
#endif
}


void target_addtorecent (const TCHAR *name, int t)
{
}


void target_reset (void)
{
}


bool target_can_autoswitchdevice(void)
{
	return true;
}


uae_u32 emulib_target_getcpurate (uae_u32 v, uae_u32 *low)
{
  *low = 0;
  if (v == 1) {
    *low = 1e+9; /* We have nano seconds */
    return 0;
  } else if (v == 2) {
    int64_t time;
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    time = (((int64_t) ts.tv_sec) * 1000000000) + ts.tv_nsec;
    *low = (uae_u32) (time & 0xffffffff);
    return (uae_u32)(time >> 32);
  }
  return 0;
}

#ifdef __LIBRETRO__
int skel_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
  struct sigaction action;
  
	max_uae_width = 768;
	max_uae_height = 625;

  defaultCpuSpeed = getDefaultCpuSpeed();
  
  // Get startup path
	getcwd(start_path_data, MAX_DPATH);

#ifdef __LIBRETRO__
//FIXME use sysdir path
#if defined(ANDROID) || defined(__ANDROID__)
sprintf(start_path_data,"/mnt/sdcard/uae4arm\0");
#else
sprintf(start_path_data,"%s/uae4arm\0",RETRO_DIR);
#endif
LOGI("spd(%s)\n",start_path_data);
#endif
	loadAdfDir();
#ifdef RP9_SUPPORT
  rp9_init();
#endif

  snprintf(savestate_fname, MAX_PATH, "%s/saves/default.ads", start_path_data);
	logging_init ();
  
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = signal_segv;
  action.sa_flags = SA_SIGINFO;
  if(sigaction(SIGSEGV, &action, NULL) < 0)
  {
    printf("Failed to set signal handler (SIGSEGV).\n");
    abort();
  }
  if(sigaction(SIGILL, &action, NULL) < 0)
  {
    printf("Failed to set signal handler (SIGILL).\n");
    abort();
  }

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = signal_buserror;
  action.sa_flags = SA_SIGINFO;
  if(sigaction(SIGBUS, &action, NULL) < 0)
  {
    printf("Failed to set signal handler (SIGBUS).\n");
    abort();
  }

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = signal_term;
  action.sa_flags = SA_SIGINFO;
  if(sigaction(SIGTERM, &action, NULL) < 0)
  {
    printf("Failed to set signal handler (SIGTERM).\n");
    abort();
  }

  alloc_AmigaMem();
  RescanROMs();
#ifdef CAPSLOCK_DEBIAN_WORKAROUND
  // set capslock state based upon current "real" state
  ioctl(0, KDGKBLED, &kbd_flags);
  ioctl(0, KDGETLED, &kbd_led_status);
  if ((kbd_flags & 07) & LED_CAP)
  {
   // record capslock pressed
   kbd_led_status |= LED_CAP;
   inputdevice_do_keyboard(AK_CAPSLOCK, 1);
  }
  else
  {
   // record capslock as not pressed
   kbd_led_status &= ~LED_CAP;
   inputdevice_do_keyboard(AK_CAPSLOCK, 0);
  }
  ioctl(0, KDSETLED, kbd_led_status);
#endif

#ifdef __LIBRETRO__
  co_switch(mainThread);
#endif

  real_main (argc, argv);

#ifdef CAPSLOCK_DEBIAN_WORKAROUND
  // restore keyboard LEDs to normal state
  ioctl(0, KDSETLED, 0xFF);
#endif

  ClearAvailableROMList();
  romlist_clear();
  free_keyring();
  free_AmigaMem();
  lstMRUDiskList.clear();
  lstMRUCDList.clear();
#ifdef RP9_SUPPORT
  rp9_cleanup();
#endif
  
  logging_cleanup();

  return 0;
}


int handle_msgpump (void)
{
	int got = 0;


#ifndef __LIBRETRO__
  SDL_Event rEvent;
  int keycode;
  int modifier;
  int handled = 0;
  int i;
  
  if(delayed_mousebutton) {
    --delayed_mousebutton;
    if(delayed_mousebutton == 0)
      setmousebuttonstate (0, 0, 1);
  }
  
	while (SDL_PollEvent(&rEvent)) {
		got = 1;
		
		switch (rEvent.type)
		{
  		case SDL_QUIT:
  			uae_quit();
  			break;

		case SDL_JOYBUTTONDOWN:
			if (currprefs.button_for_menu != -1 && rEvent.jbutton.button == currprefs.button_for_menu)
				inputdevice_add_inputcode(AKS_ENTERGUI, 1);
			if (currprefs.button_for_quit != -1 && rEvent.jbutton.button == currprefs.button_for_quit)
				inputdevice_add_inputcode(AKS_QUIT, 1);
			break;
	
  		case SDL_KEYDOWN:

  		  if (currprefs.key_for_menu != 0 && rEvent.key.keysym.sym == currprefs.key_for_menu)
  		  	inputdevice_add_inputcode(AKS_ENTERGUI, 1);
  		  if (currprefs.key_for_quit != 0 && rEvent.key.keysym.sym == currprefs.key_for_quit)
  		  	inputdevice_add_inputcode(AKS_QUIT, 1);
#ifdef ACTION_REPLAY
  		  if(rEvent.key.keysym.sym == currprefs.key_for_cartridge)
        		      if(currprefs.cartfile[0] != '\0') {
          		      inputdevice_add_inputcode (AKS_FREEZEBUTTON, 1);
    				        handled = 1;
    				      }
#endif

  		  switch(rEvent.key.keysym.sym)
  		  {
 		#ifdef CAPSLOCK_DEBIAN_WORKAROUND
		case SDLK_CAPSLOCK: // capslock
		     // Treat CAPSLOCK as a toggle. If on, set off and vice/versa
                     ioctl(0, KDGKBLED, &kbd_flags);
                     ioctl(0, KDGETLED, &kbd_led_status);
                     if ((kbd_flags & 07) & LED_CAP)
                     {
                        // On, so turn off
                        kbd_led_status &= ~LED_CAP;
                        kbd_flags &= ~LED_CAP;
                        inputdevice_do_keyboard(AK_CAPSLOCK, 0);
                     } else {
                               // Off, so turn on
                               kbd_led_status |= LED_CAP;
                               kbd_flags |= LED_CAP;
                               inputdevice_do_keyboard(AK_CAPSLOCK, 1);
                            }
                     ioctl(0, KDSETLED, kbd_led_status);
                     ioctl(0, KDSKBLED, kbd_flags);
                     break;
                 #endif

				  case SDLK_LSHIFT: // Shift key
				  inputdevice_do_keyboard(AK_LSH, 1);
				  break;
            
				  case VK_L: // Left shoulder button
				  case VK_R:  // Right shoulder button
  					if(currprefs.input_tablet > TABLET_OFF) {
  					  // Holding left or right shoulder button -> stylus does right mousebutton
  					  doStylusRightClick = 1;
            }
            // Fall through...
            
  				default:
  				  if(currprefs.pandora_customControls) {
  				    keycode = customControlMap[rEvent.key.keysym.sym];
  				    if(keycode < 0) {
  				      // Simulate mouse or joystick
  				      SimulateMouseOrJoy(keycode, 1);
  				      break;
  				    }
  				    else if(keycode > 0) {
  				      // Send mapped key press
  				      inputdevice_do_keyboard(keycode, 1);
  				      break;
  				    }
  				  }
  				  else
  				  {  
  				  modifier = rEvent.key.keysym.mod;
  				  keycode = translate_pandora_keys(rEvent.key.keysym.sym, &modifier);
  				  if(keycode)
  				  {
				      if(modifier == KMOD_SHIFT)
  				      inputdevice_do_keyboard(AK_LSH, 1);
  				    else
  				      inputdevice_do_keyboard(AK_LSH, 0);
				      inputdevice_do_keyboard(keycode, 1);
  				  } else {
				      if (keyboard_type == KEYCODE_UNK)
				        inputdevice_translatekeycode(0, rEvent.key.keysym.sym, 1);
				      else
				        inputdevice_translatekeycode(0, rEvent.key.keysym.scancode, 1);

				    }
                                  break;
                                  }
				}
        break;
        
  	  case SDL_KEYUP:
  	    switch(rEvent.key.keysym.sym)
  	    {
                #ifdef CAPSLOCK_DEBIAN_WORKAROUND
                case SDLK_CAPSLOCK: // capslock
                     // Treat CAPSLOCK as a toggle. If on, set off and vice/versa
                     ioctl(0, KDGKBLED, &kbd_flags);
                     ioctl(0, KDGETLED, &kbd_led_status);
                     if ((kbd_flags & 07) & LED_CAP)
                     {
                        // On, so turn off
                        kbd_led_status &= ~LED_CAP;
                        kbd_flags &= ~LED_CAP;
                        inputdevice_do_keyboard(AK_CAPSLOCK, 0);
                     } else {
                               // Off, so turn on
                               kbd_led_status |= LED_CAP;
                               kbd_flags |= LED_CAP;
                               inputdevice_do_keyboard(AK_CAPSLOCK, 1);
                            }
                     ioctl(0, KDSETLED, kbd_led_status);
                     ioctl(0, KDSKBLED, kbd_flags);
                     break;
                 #endif

				  case SDLK_LSHIFT: // Shift key
            inputdevice_do_keyboard(AK_LSH, 0);
            break;
            
				  case VK_L: // Left shoulder button
				  case VK_R:  // Right shoulder button
  					if(currprefs.input_tablet > TABLET_OFF) {
  					  // Release left or right shoulder button -> stylus does left mousebutton
    					doStylusRightClick = 0;
            }
            // Fall through...
  				
  				default:
  				  if(currprefs.pandora_customControls) {
  				    keycode = customControlMap[rEvent.key.keysym.sym];
  				    if(keycode < 0) {
  				      // Simulate mouse or joystick
  				      SimulateMouseOrJoy(keycode, 0);
  				      break;
  				    }
  				    else if(keycode > 0) {
  				      // Send mapped key release
				      inputdevice_do_keyboard(keycode, 0);
  				      break;
  				    }
  				  }

  				  modifier = rEvent.key.keysym.mod;
  				  keycode = translate_pandora_keys(rEvent.key.keysym.sym, &modifier);
  				  if(keycode)
  				  {
				      inputdevice_do_keyboard(keycode, 0);
				      if(modifier == KMOD_SHIFT)
  				      inputdevice_do_keyboard(AK_LSH, 0);
            } else {
				      if (keyboard_type == KEYCODE_UNK)
				        inputdevice_translatekeycode(0, rEvent.key.keysym.sym, 0);
				      else
				        inputdevice_translatekeycode(0, rEvent.key.keysym.scancode, 0);
				    }
  				  break;
  	    }
  	    break;
  	    
  	  case SDL_MOUSEBUTTONDOWN:
        if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
    	    if(rEvent.button.button == SDL_BUTTON_LEFT) {
    	      if(currprefs.input_tablet > TABLET_OFF && !doStylusRightClick) {
    	        // Delay mousebutton, we need new position first...
    	        delayed_mousebutton = currprefs.pandora_tapDelay << 1;
    	      } else {
      	      setmousebuttonstate (0, doStylusRightClick, 1);
      	    }
    	    }
    	    else if(rEvent.button.button == SDL_BUTTON_RIGHT)
    	      setmousebuttonstate (0, 1, 1);
        }
  	    break;

  	  case SDL_MOUSEBUTTONUP:
        if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
    	    if(rEvent.button.button == SDL_BUTTON_LEFT) {
  	        setmousebuttonstate (0, doStylusRightClick, 0);
          }
    	    else if(rEvent.button.button == SDL_BUTTON_RIGHT)
    	      setmousebuttonstate (0, 1, 0);
        }
  	    break;
  	    
  		case SDL_MOUSEMOTION:
  		  if(currprefs.input_tablet == TABLET_OFF) {
          if(currprefs.jports[0].id == JSEM_MICE || currprefs.jports[1].id == JSEM_MICE) {
  			    int x, y;
    		    int mouseScale = currprefs.input_joymouse_multiplier / 2;
            x = rEvent.motion.xrel;
    				y = rEvent.motion.yrel;
#ifdef PANDORA_SPECIFIC
    				if(rEvent.motion.x == 0 && x > -4)
    					x = -4;
    				if(rEvent.motion.y == 0 && y > -4)
    					y = -4;
    				if(rEvent.motion.x == currprefs.gfx_size.width - 1 && x < 4)
    					x = 4;
    				if(rEvent.motion.y == currprefs.gfx_size.height - 1 && y < 4)
    					y = 4;
#endif
  				  setmousestate(0, 0, x * mouseScale, 0);
        	  setmousestate(0, 1, y * mouseScale, 0);
          }
        }
        break;
		}
	}
#endif
	return got;
}


static uaecptr clipboard_data;

void amiga_clipboard_die (void)
{
}

void amiga_clipboard_init (void)
{
}

void amiga_clipboard_task_start (uaecptr data)
{
  clipboard_data = data;
}

uae_u32 amiga_clipboard_proc_start (void)
{
  return clipboard_data;
}

void amiga_clipboard_got_data (uaecptr data, uae_u32 size, uae_u32 actual)
{
}

int amiga_clipboard_want_data (void)
{
  return 0;
}

void clipboard_vsync (void)
{
}
