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
//#include <execinfo.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "autoconf.h"
#include "uae.h"
#include "options.h"
#include "td-sdl/thread.h"
#include "gui.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "keybuf.h"
#include "keyboard.h"
#include "joystick.h"
#include "disk.h"
#include "savestate.h"
#include "newcpu.h"
#include "traps.h"
#include "native2amiga.h"
#include "rtgmodes.h"
#include "uaeresource.h"
#include "gp2x.h"
#include "zfile.h"

extern void signal_segv(int signum, siginfo_t* info, void*ptr);

extern int doStylusRightClick;
extern int mouseMoving;
extern int justClicked;
extern int fcounter;

extern int keycode2amiga(int prKeySym);
extern int loadconfig_old(struct uae_prefs *p, const char *orgpath);
extern void SetLastActiveConfig(const char *filename);

/* Keyboard and mouse */
/* Keyboard */
int uae4all_keystate[256];
#ifdef PANDORA
static int shiftWasPressed = 0;
#endif

char start_path_data[MAX_DPATH];
char currentDir[MAX_DPATH];
int show_inputmode = 0;

int sleep_resolution = 1000 / 1;

static char config_path[MAX_DPATH];
static char rom_path[MAX_DPATH];
char last_loaded_config[MAX_DPATH] = { '\0' };

static bool slow_mouse = false;

static bool cpuSpeedChanged = false;
static int lastCpuSpeed = 600;


extern "C" int skel_umain( int argc, char *argv[] );


void reinit_amiga(void)
{
  write_log("reinit_amiga() called\n");
  DISK_free ();
  memory_cleanup ();
#ifdef FILESYS
  filesys_cleanup ();
#endif
#ifdef AUTOCONFIG
  expansion_cleanup ();
#endif
  
  currprefs = changed_prefs;
  /* force sound settings change */
  currprefs.produce_sound = 0;

  framecnt = 1;
#ifdef AUTOCONFIG
  rtarea_setup ();
#endif
#ifdef FILESYS
  rtarea_init ();
  uaeres_install ();
  hardfile_install();
#endif

#ifdef AUTOCONFIG
  expansion_init ();
#endif
#ifdef FILESYS
  filesys_install (); 
#endif
  memory_init ();
  memory_reset ();

#ifdef AUTOCONFIG
  emulib_install ();
  native2amiga_install ();
#endif

  custom_init (); /* Must come after memory_init */
  DISK_init ();
  
  init_m68k();
}


void sleep_millis (int ms)
{
  usleep(ms * 1000);
}

void sleep_millis_busy (int ms)
{
  usleep(ms * 1000);
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

	sprintf(debugfilename, "%s/uae4all_log.txt", start_path_data);
	if(!debugfile)
    debugfile = fopen(debugfilename, "wt");

  first++;
  write_log ( "UAE4ALL Logfile\n\n");
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


uae_u8 *target_load_keyfile (struct uae_prefs *p, char *path, int *sizep, char *name)
{
  return 0;
}


void target_quit (void)
{
}


void target_fixup_options (struct uae_prefs *p)
{
}


void target_default_options (struct uae_prefs *p, int type)
{
  p->pandora_horizontal_offset = 0;
  p->pandora_vertical_offset = 0;
  p->pandora_cpu_speed = 600;

  p->pandora_joyConf = 0;
  p->pandora_joyPort = 0;
  p->pandora_tapDelay = 10;
  p->pandora_stylusOffset = 0;
  
	p->pandora_customControls = 0;
#ifdef RASPBERRY
	p->pandora_custom_dpad = 1;
#else
	p->pandora_custom_dpad = 0;
#endif
	p->pandora_custom_up = 0;
	p->pandora_custom_down = 0;
	p->pandora_custom_left = 0;
	p->pandora_custom_right = 0;
	p->pandora_custom_A = 0;
	p->pandora_custom_B = 0;
	p->pandora_custom_X = 0;
	p->pandora_custom_Y = 0;
	p->pandora_custom_L = 0;
	p->pandora_custom_R = 0;

	p->pandora_button1 = GP2X_BUTTON_X;
	p->pandora_button2 = GP2X_BUTTON_A;
	p->pandora_autofireButton1 = GP2X_BUTTON_B;
	p->pandora_jump = -1;
	
	p->picasso96_modeflags = RGBFF_R5G6B5 | RGBFF_R8G8B8A8;
}


void target_save_options (struct zfile *f, struct uae_prefs *p)
{
  cfgfile_write (f, "pandora.cpu_speed=%d\n", p->pandora_cpu_speed);
  cfgfile_write (f, "pandora.joy_conf=%d\n", p->pandora_joyConf);
  cfgfile_write (f, "pandora.joy_port=%d\n", p->pandora_joyPort);
  cfgfile_write (f, "pandora.stylus_offset=%d\n", p->pandora_stylusOffset);
  cfgfile_write (f, "pandora.tap_delay=%d\n", p->pandora_tapDelay);
  cfgfile_write (f, "pandora.custom_controls=%d\n", p->pandora_customControls);
  cfgfile_write (f, "pandora.custom_dpad=%d\n", p->pandora_custom_dpad);
  cfgfile_write (f, "pandora.custom_up=%d\n", p->pandora_custom_up);
  cfgfile_write (f, "pandora.custom_down=%d\n", p->pandora_custom_down);
  cfgfile_write (f, "pandora.custom_left=%d\n", p->pandora_custom_left);
  cfgfile_write (f, "pandora.custom_right=%d\n", p->pandora_custom_right);
  cfgfile_write (f, "pandora.custom_a=%d\n", p->pandora_custom_A);
  cfgfile_write (f, "pandora.custom_b=%d\n", p->pandora_custom_B);
  cfgfile_write (f, "pandora.custom_x=%d\n", p->pandora_custom_X);
  cfgfile_write (f, "pandora.custom_y=%d\n", p->pandora_custom_Y);
  cfgfile_write (f, "pandora.custom_l=%d\n", p->pandora_custom_L);
  cfgfile_write (f, "pandora.custom_r=%d\n", p->pandora_custom_R);
  cfgfile_write (f, "pandora.move_x=%d\n", p->pandora_horizontal_offset);
  cfgfile_write (f, "pandora.move_y=%d\n", p->pandora_vertical_offset);
  cfgfile_write (f, "pandora.button1=%d\n", p->pandora_button1);
  cfgfile_write (f, "pandora.button2=%d\n", p->pandora_button2);
  cfgfile_write (f, "pandora.autofire_button=%d\n", p->pandora_autofireButton1);
  cfgfile_write (f, "pandora.jump=%d\n", p->pandora_jump);
}


int target_parse_option (struct uae_prefs *p, char *option, char *value)
{
  int result = (cfgfile_intval (option, value, "cpu_speed", &p->pandora_cpu_speed, 1)
    || cfgfile_intval (option, value, "joy_conf", &p->pandora_joyConf, 1)
    || cfgfile_intval (option, value, "joy_port", &p->pandora_joyPort, 1)
    || cfgfile_intval (option, value, "stylus_offset", &p->pandora_stylusOffset, 1)
    || cfgfile_intval (option, value, "tap_delay", &p->pandora_tapDelay, 1)
    || cfgfile_intval (option, value, "custom_controls", &p->pandora_customControls, 1)
    || cfgfile_intval (option, value, "custom_dpad", &p->pandora_custom_dpad, 1)
    || cfgfile_intval (option, value, "custom_up", &p->pandora_custom_up, 1)
    || cfgfile_intval (option, value, "custom_down", &p->pandora_custom_down, 1)
    || cfgfile_intval (option, value, "custom_left", &p->pandora_custom_left, 1)
    || cfgfile_intval (option, value, "custom_right", &p->pandora_custom_right, 1)
    || cfgfile_intval (option, value, "custom_a", &p->pandora_custom_A, 1)
    || cfgfile_intval (option, value, "custom_b", &p->pandora_custom_B, 1)
    || cfgfile_intval (option, value, "custom_x", &p->pandora_custom_X, 1)
    || cfgfile_intval (option, value, "custom_y", &p->pandora_custom_Y, 1)
    || cfgfile_intval (option, value, "custom_l", &p->pandora_custom_L, 1)
    || cfgfile_intval (option, value, "custom_r", &p->pandora_custom_R, 1)
    || cfgfile_intval (option, value, "move_x", &p->pandora_horizontal_offset, 1)
    || cfgfile_intval (option, value, "move_y", &p->pandora_vertical_offset, 1)
    || cfgfile_intval (option, value, "button1", &p->pandora_button1, 1)
    || cfgfile_intval (option, value, "button2", &p->pandora_button2, 1)
    || cfgfile_intval (option, value, "autofire_button", &p->pandora_autofireButton1, 1)
    || cfgfile_intval (option, value, "jump", &p->pandora_jump, 1)
    );
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


int target_cfgfile_load (struct uae_prefs *p, char *filename, int type, int isdefault)
{
  int i;
  int result = 0;

  filesys_prepare_reset();
  while(p->mountitems > 0)
    kill_filesys_unitconfig(p, 0);
  discard_prefs(p, type);
  
	char *ptr = strstr(filename, ".uae");
  if(ptr > 0)
  {
    int type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
    result = cfgfile_load(p, filename, &type, 0);
  }
  if(result)
  {
  	set_joyConf(p);
    extractFileName(filename, last_loaded_config);
  }
  else 
    result = loadconfig_old(p, filename);

  if(result)
  {
    for(i=0; i < p->nr_floppies; ++i)
    {
      if(!DISK_validate_filename(p->df[i], 0, NULL, NULL))
        p->df[i][0] = 0;
      disk_insert(i, p->df[i]);
      if(strlen(p->df[i]) > 0)
        AddFileToDiskList(p->df[i], 1);
    }

    inputdevice_updateconfig (p);

    SetLastActiveConfig(filename);
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
  
  strcpy(tmp, file);
	char *ptr = strstr(tmp, ".uae");
	if(ptr > 0)
  {
    *(ptr + 1) = '\0';
    strcat(tmp, "conf");
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
	strcpy(buffer,p);
}


void extractPath(char *str, char *buffer)
{
	strcpy(buffer, str);
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


void loadAdfDir(void)
{
	char path[MAX_DPATH];
  int i;

	strcpy(currentDir, start_path_data);
	snprintf(config_path, MAX_DPATH, "%s/conf/", start_path_data);
	snprintf(rom_path, MAX_DPATH, "%s/kickstarts/", start_path_data);

	snprintf(path, MAX_DPATH, "%s/conf/adfdir.conf", start_path_data);
	FILE *f1=fopen(path,"rt");
	if(f1)
	{
		fscanf(f1, "path=");
		get_string(f1, currentDir, sizeof(currentDir));
		if(!feof(f1))
		{
		  fscanf(f1, "config_path=");
		  get_string(f1, config_path, sizeof(config_path));
		  fscanf(f1, "rom_path=");
      get_string(f1, rom_path, sizeof(rom_path));
      
      int numROMs;
      fscanf(f1, "ROMs=%d\n", &numROMs);
      for(i=0; i<numROMs; ++i)
      {
        AvailableROM *tmp;
        tmp = new AvailableROM();
        fscanf(f1, "ROMName=");
        get_string(f1, tmp->Name, sizeof(tmp->Name));
        fscanf(f1, "ROMPath=");
        get_string(f1, tmp->Path, sizeof(tmp->Path));
        fscanf(f1, "ROMType=%d\n", &(tmp->ROMType));
        lstAvailableROMs.push_back(tmp);
      }
      
      lstMRUDiskList.clear();
      int numDisks = 0;
      char disk[MAX_PATH];
      fscanf(f1, "MRUDiskList=%d\n", &numDisks);
      for(i=0; i<numDisks; ++i)
      {
        fscanf(f1, "Diskfile=");
        get_string(f1, disk, sizeof(disk));
        lstMRUDiskList.push_back(disk);
      }
	  }
		fclose(f1);
	}
}


void setCpuSpeed()
{
return;
#if 0
	char speedCmd[128];

#ifdef RASPBERRY
	return;
#endif

  currprefs.pandora_cpu_speed = changed_prefs.pandora_cpu_speed;

	if(currprefs.pandora_cpu_speed != lastCpuSpeed)
	{
/*
		snprintf((char*)speedCmd, 128, "unset DISPLAY; echo y | sudo -n /usr/pandora/scripts/op_cpuspeed.sh %d", currprefs.pandora_cpu_speed);
		system(speedCmd);
*/
		lastCpuSpeed = currprefs.pandora_cpu_speed;
		cpuSpeedChanged = true;
	}
	if(changed_prefs.ntscmode != currprefs.ntscmode)
	{
/*
		if(changed_prefs.ntscmode)
			system("sudo /usr/pandora/scripts/op_lcdrate.sh 60");
		else
			system("sudo /usr/pandora/scripts/op_lcdrate.sh 50");
*/
	}
#endif
}


void resetCpuSpeed(void)
{
return;
#if 0
#ifdef RASPBERRY
	return;
#endif
  if(cpuSpeedChanged)
  {
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
            lastCpuSpeed = value - 10;
            currprefs.pandora_cpu_speed = changed_prefs.pandora_cpu_speed = value;
            setCpuSpeed();
            printf("CPU speed reset to %d\n", value);
          }
        }
      }
      fclose(f);
    }
  }
#endif
}

#include "core-log.h"
#include "libco/libco.h"

extern cothread_t mainThread;
extern cothread_t emuThread;
extern char RETRO_DIR[512];

int skel_main (int argc, char *argv[])
{
  struct sigaction action;

  // Get startup path
	getcwd(start_path_data, MAX_DPATH);

//FIXME use sysdir path
#if defined(ANDROID) || defined(__ANDROID__)
sprintf(start_path_data,"/mnt/sdcard/uae4arm\0");
#else
sprintf(start_path_data,"%s/uae4arm\0",RETRO_DIR);
#endif
LOGI("spd(%s)\n",start_path_data);

	loadAdfDir();

  snprintf(savestate_fname, MAX_PATH, "%s/saves/default.ads", start_path_data);
	logging_init ();
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = signal_segv;
  action.sa_flags = SA_SIGINFO;
  if(sigaction(SIGSEGV, &action, NULL) < 0)
  {
    LOGI("Failed to set signal handler (SIGSEGV).\n");
    abort();
  }
  if(sigaction(SIGILL, &action, NULL) < 0)
  {
    LOGI("Failed to set signal handler (SIGILL).\n");
    abort();
  }

  alloc_AmigaMem();
  RescanROMs();
co_switch(mainThread);
  real_main (argc, argv);
  free_AmigaMem();
  
  logging_cleanup();
  
  return 0;
}


int needmousehack (void)
{
    return 1;
}


int mousehack_allowed (void)
{
    return 1;
}


void keyboard_init(void)
{
  int i;
  
  for (i = 256; i--;)
		uae4all_keystate[i] = 0;
  shiftWasPressed = 0;
}

extern int Retro_PollEvent();

void handle_events (void)
{

Retro_PollEvent();
//LOGI("handleevent\n");
#if 0
  SDL_Event rEvent;
  int iAmigaKeyCode;
  int i, j;
  int iIsHotKey = 0;

  /* Handle GUI events */
  gui_handle_events ();
  handle_joymouse();
    
  while (SDL_PollEvent(&rEvent))
  {
		switch (rEvent.type)
		{
  		case SDL_QUIT:
  			uae_quit();
  			break;

  		case SDL_KEYDOWN:
  			if(rEvent.key.keysym.sym==SDLK_PAGEUP)
  				slow_mouse=true;
  			if(currprefs.pandora_custom_dpad == 1) // dPad as mouse, why these emulation of key presses?
  			{
  				if(rEvent.key.keysym.sym==SDLK_RSHIFT)
  				{
  					uae4all_keystate[AK_LALT] = 1;
  					record_key(AK_LALT << 1);
  				}
  				if(rEvent.key.keysym.sym==SDLK_RCTRL || rEvent.key.keysym.sym==SDLK_END || rEvent.key.keysym.sym==SDLK_HOME)
  				{
  					uae4all_keystate[AK_RALT] = 1;
  					record_key(AK_RALT << 1);
  				}
  				if(rEvent.key.keysym.sym==SDLK_PAGEDOWN)
  				{
  					uae4all_keystate[AK_DN] = 1;
  					record_key(AK_DN << 1);
  				}
  			}
			
  			if (rEvent.key.keysym.sym==SDLK_LALT)
  			{
					// state moves thus:
					// joystick mode (with virt keyboard on L and R)
					// mouse mode (with mouse buttons on L and R)
					// remapping mode (with whatever's been supplied)
					// back to start of state

					currprefs.pandora_custom_dpad++;
#ifdef RASPBERRY
					if(currprefs.pandora_custom_dpad > 1)
#else
					if(currprefs.pandora_custom_dpad > 2)
#endif
					  currprefs.pandora_custom_dpad = 0;
          changed_prefs.pandora_custom_dpad = currprefs.pandora_custom_dpad;
          if(currprefs.pandora_custom_dpad == 2)
          	mousehack_set(mousehack_follow);
          else
      	    mousehack_set (mousehack_dontcare);
            
  				show_inputmode = 1;
  			}

				if (rEvent.key.keysym.sym==SDLK_RSHIFT || rEvent.key.keysym.sym==SDLK_RCTRL)
					doStylusRightClick = 1;

  			if (rEvent.key.keysym.sym!=SDLK_UP && rEvent.key.keysym.sym!=SDLK_DOWN && rEvent.key.keysym.sym!=SDLK_LEFT &&
  				rEvent.key.keysym.sym!=SDLK_RIGHT && rEvent.key.keysym.sym!=SDLK_PAGEUP && rEvent.key.keysym.sym!=SDLK_PAGEDOWN &&
  				rEvent.key.keysym.sym!=SDLK_HOME && rEvent.key.keysym.sym!=SDLK_END && rEvent.key.keysym.sym!=SDLK_LALT &&
  				rEvent.key.keysym.sym!=SDLK_LCTRL && rEvent.key.keysym.sym!=SDLK_RSHIFT && rEvent.key.keysym.sym!=SDLK_RCTRL)
  			{
  				iAmigaKeyCode = keycode2amiga(&(rEvent.key.keysym));
  				if (iAmigaKeyCode >= 0)
  				{
#ifdef PANDORA
  				  if(iAmigaKeyCode & SIMULATE_SHIFT)
  			    {
              // We need to simulate shift
              iAmigaKeyCode = iAmigaKeyCode & 0x1ff;
              shiftWasPressed = uae4all_keystate[AK_LSH];
              if(!shiftWasPressed)
              {
                uae4all_keystate[AK_LSH] = 1;
                record_key(AK_LSH << 1);
              }
  			    }
  				  if(iAmigaKeyCode & SIMULATE_RELEASED_SHIFT)
  			    {
              // We need to simulate released shift
              iAmigaKeyCode = iAmigaKeyCode & 0x1ff;
              shiftWasPressed = uae4all_keystate[AK_LSH];
              if(shiftWasPressed)
              {
                uae4all_keystate[AK_LSH] = 0;
                record_key((AK_LSH << 1) | 1);
              }
  			    }
#endif
  					if (!uae4all_keystate[iAmigaKeyCode])
  					{
  						uae4all_keystate[iAmigaKeyCode] = 1;
  						record_key(iAmigaKeyCode << 1);
  					}
  				}
  			}
  			break;

		case SDL_KEYUP:
  			if(rEvent.key.keysym.sym==SDLK_PAGEUP)
  				slow_mouse = false;
  			if(currprefs.pandora_custom_dpad == 1) // dPad as mouse, why these emulation of key presses?
  			{
  				if(rEvent.key.keysym.sym==SDLK_RSHIFT)
  				{
  					uae4all_keystate[AK_LALT] = 0;
  					record_key((AK_LALT << 1) | 1);
  				}
  				if(rEvent.key.keysym.sym==SDLK_RCTRL || rEvent.key.keysym.sym==SDLK_END || rEvent.key.keysym.sym==SDLK_HOME)
  				{
  					uae4all_keystate[AK_RALT] = 0;
  					record_key((AK_RALT << 1) | 1);
  				}
  				if(rEvent.key.keysym.sym==SDLK_PAGEDOWN)
  				{
  					uae4all_keystate[AK_DN] = 0;
  					record_key((AK_DN << 1) | 1);
  				}
  			}

				if (rEvent.key.keysym.sym==SDLK_RSHIFT || rEvent.key.keysym.sym==SDLK_RCTRL)
			  {
					doStylusRightClick = 0;
  				mouseMoving = 0;
  				justClicked = 0;
  				fcounter = 0;
  				buttonstate[2] = 0;
				}

  			if (rEvent.key.keysym.sym==SDLK_LALT)
  			{
  				show_inputmode = 0;
  			}
  			if (rEvent.key.keysym.sym!=SDLK_UP && rEvent.key.keysym.sym!=SDLK_DOWN && rEvent.key.keysym.sym!=SDLK_LEFT &&
  				rEvent.key.keysym.sym!=SDLK_RIGHT && rEvent.key.keysym.sym!=SDLK_PAGEUP && rEvent.key.keysym.sym!=SDLK_PAGEDOWN &&
  				rEvent.key.keysym.sym!=SDLK_HOME && rEvent.key.keysym.sym!=SDLK_END && rEvent.key.keysym.sym!=SDLK_LALT &&
  				rEvent.key.keysym.sym!=SDLK_LCTRL && rEvent.key.keysym.sym!=SDLK_RSHIFT && rEvent.key.keysym.sym!=SDLK_RCTRL)
  			{
  				iAmigaKeyCode = keycode2amiga(&(rEvent.key.keysym));
  				if (iAmigaKeyCode >= 0)
  				{
#ifdef PANDORA
  				  if(iAmigaKeyCode & SIMULATE_SHIFT)
  			    {
              // We needed to simulate shift
              iAmigaKeyCode = iAmigaKeyCode & 0x1ff;
              if(!shiftWasPressed)
              {
                uae4all_keystate[AK_LSH] = 0;
                record_key((AK_LSH << 1) | 1);
                shiftWasPressed = 0;
              }
  			    }
  				  if(iAmigaKeyCode & SIMULATE_RELEASED_SHIFT)
  			    {
              // We needed to simulate released shift
              iAmigaKeyCode = iAmigaKeyCode & 0x1ff;
              if(shiftWasPressed)
              {
                uae4all_keystate[AK_LSH] = 1;
                record_key(AK_LSH << 1);
                shiftWasPressed = 0;
              }
  			    }
#endif
  					uae4all_keystate[iAmigaKeyCode] = 0;
  					record_key((iAmigaKeyCode << 1) | 1);
  				}
  			}
  			break;

  		case SDL_MOUSEBUTTONDOWN:
    		if (currprefs.pandora_custom_dpad < 2)
    			buttonstate[(rEvent.button.button-1)%3] = 1;
  			break;

  		case SDL_MOUSEBUTTONUP:
    		if (currprefs.pandora_custom_dpad < 2)
    			buttonstate[(rEvent.button.button-1)%3] = 0;
  			break;

  		case SDL_MOUSEMOTION:
  			if(currprefs.pandora_custom_dpad == 2)
  			{
  				lastmx = rEvent.motion.x*2 - currprefs.pandora_stylusOffset + stylusAdjustX >> 1;
  				lastmy = rEvent.motion.y*2 - currprefs.pandora_stylusOffset + stylusAdjustY >> 1;
  				//mouseMoving = 1;
  			}
  			else if(slow_mouse)
  			{
  				lastmx += rEvent.motion.xrel;
  				lastmy += rEvent.motion.yrel;
  				if(rEvent.motion.x == 0)
  					lastmx -= 2;
  				if(rEvent.motion.y == 0)
  					lastmy -= 2;
  				if(rEvent.motion.x == currprefs.gfx_size.width - 1)
  					lastmx += 2;
  				if(rEvent.motion.y == currprefs.gfx_size.height - 1)
  					lastmy += 2;
  			}
  			else
  			{
				  int mouseScale = currprefs.input_joymouse_multiplier / 2;
				  if(rEvent.motion.xrel > 20 || rEvent.motion.xrel < -20 || rEvent.motion.yrel > 20 || rEvent.motion.yrel < -20)
				    break;
  				lastmx += rEvent.motion.xrel * mouseScale;
  				lastmy += rEvent.motion.yrel * mouseScale;
  				if(rEvent.motion.x == 0)
  					lastmx -= mouseScale * 4;
  				if(rEvent.motion.y == 0)
  					lastmy -= mouseScale * 4;
  				if(rEvent.motion.x == currprefs.gfx_size.width - 1)
  					lastmx += mouseScale * 4;
  				if(rEvent.motion.y == currprefs.gfx_size.height - 1)
  					lastmy += mouseScale * 4;
  			}
  			newmousecounters = 1;
  			break;
		}
  }
#endif
}

//void uae_set_thread_priority(int pri1){}


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
  { -1, "" }
};

std::vector<AvailableROM*> lstAvailableROMs;
std::vector<std::string> lstMRUDiskList;

void gui_disk_image_change (int unitnum, const char *name)
{
}

void gui_hd_led (int led)
{
    static int resetcounter;

    if (led == 0) {
	    resetcounter--;
	    if (resetcounter > 0)
	      return;
    }
    gui_data.hd = led;
    resetcounter = 2;
}

void gui_cd_led (int led)
{
}

void gui_led (int led, int on)
{
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

  //InGameMessage(msg);
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


void gui_lock (void)
{
}

void gui_unlock (void)
{
}

void gui_exit(void)
{/*
	resetCpuSpeed();
	sync();
	pandora_stop_sound();
	saveAdfDir();
	ClearConfigFileList();
	ClearAvailableROMList();
*/
}

int gui_init (void)
{
  int ret = 0;
 /* 
#ifndef ANDROIDSDL
  SDL_JoystickEventState(SDL_ENABLE);
  SDL_JoystickOpen(0);
#endif

	emulating=0;

  if(lstAvailableROMs.size() == 0)
    RescanROMs();

  graphics_subshutdown();
  prefs_to_gui();
  run_gui();
  gui_to_prefs();
  if(quit_program < 0)
    quit_program = -quit_program;
  if(quit_program == 1)
    ret = -2; // Quit without start of emulator

	setCpuSpeed();
  update_display(&changed_prefs);

	inputmode_init();
  inputdevice_copyconfig (&changed_prefs, &currprefs);
  inputdevice_config_change_test();
*/
	emulating=1;
  return ret;
}

int gui_update (void)
{
/*
  char tmp[MAX_PATH];

  fetch_savestatepath(savestate_fname, MAX_DPATH);
  fetch_screenshotpath(screenshot_filename, MAX_DPATH);
  
  if(strlen(currprefs.df[0]) > 0)
    extractFileName(currprefs.df[0], tmp);
  else
    strncpy(tmp, last_loaded_config, MAX_PATH);

  strncat(savestate_fname, tmp, MAX_DPATH);
  strncat(screenshot_filename, tmp, MAX_DPATH);
  removeFileExtension(savestate_fname);
  removeFileExtension(screenshot_filename);

  switch(currentStateNum)
  {
    case 1:
  		strcat(savestate_fname,"-1.uss");
	    strcat(screenshot_filename,"-1.png");
	    break;
    case 2:
  		strcat(savestate_fname,"-2.uss");
  		strcat(screenshot_filename,"-2.png");
  		break;
    case 3:
  		strcat(savestate_fname,"-3.uss");
  		strcat(screenshot_filename,"-3.png");
  		break;
    default: 
  	   	strcat(savestate_fname,".uss");
    		strcat(screenshot_filename,".png");
  }*/
    return 0;
}

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

static void ClearAvailableROMList(void)
{
  while(lstAvailableROMs.size() > 0)
  {
    AvailableROM *tmp = lstAvailableROMs[0];
    lstAvailableROMs.erase(lstAvailableROMs.begin());
    delete tmp;
  }
}
int emulating = 0;
int stylusClickOverride=0;
static char last_active_config[MAX_PATH] = { '\0' };

void SetLastActiveConfig(const char *filename)
{
  extractFileName(filename, last_active_config);
  removeFileExtension(last_active_config);
}

static void addrom(struct romdata *rd, char *path)
{
  AvailableROM *tmp;
  char tmpName[MAX_DPATH];
  tmp = new AvailableROM();
  getromname(rd, tmpName);
  strncpy(tmp->Name, tmpName, MAX_PATH);
  strncpy(tmp->Path, path, MAX_PATH);
  tmp->ROMType = rd->type;
  lstAvailableROMs.push_back(tmp);
}

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
  rombuf = (uae_u8 *) xcalloc (size, 1);
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
  struct romdata *rd;

    if (!isromext(path)) {
	//write_log("ROMSCAN: skipping file '%s', unknown extension\n", path);
	return;
    }
  rd = getarcadiarombyname(path);
  if (rd) 
    addrom(rd, path);
  else
    zfile_zopen (path, scan_rom_2, 0);
}


void RescanROMs(void)
{
  std::vector<std::string> files;
  char path[MAX_DPATH];
  
  ClearAvailableROMList();
  fetch_rompath(path, MAX_DPATH);
  
  load_keyring(&changed_prefs, path);
  ReadDirectory(path, NULL, &files);
  for(int i=0; i<files.size(); ++i)
  {
    char tmppath[MAX_PATH];
    strncpy(tmppath, path, MAX_PATH);
    strncat(tmppath, files[i].c_str(), MAX_PATH);
    scan_rom (tmppath);
  }
}

