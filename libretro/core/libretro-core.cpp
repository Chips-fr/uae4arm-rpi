#include "libretro.h"
#include "libretro-core.h"

#include "libretro/retrodep/WHDLoad_hdf.gz.c"

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "custom.h"

#include "uae.h"
#include "inputdevice.h"

#include "zlib.h"
#include "fsdb.h"
#include "filesys.h"
#include "autoconf.h"

cothread_t mainThread;
cothread_t emuThread;

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH ;
int retrow=320; 
int retroh=240;

static unsigned msg_interface_version = 0;

#define RETRO_DEVICE_AMIGA_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)
#define RETRO_DEVICE_AMIGA_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)

unsigned amiga_devices[ 2 ];

extern int SHIFTON,pauseg,SND;
extern char RPATH[512];
extern char RETRO_DIR[512];
extern int SHOWKEY;

#include "cmdline.cpp"

extern void update_input(void);
extern void texture_init(void);
extern void texture_uninit(void);
extern void Emu_init();
extern void Emu_uninit();
extern void input_gui(void);
extern void retro_virtualkb(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

// Amiga default kickstarts

#define A500_ROM        "kick34005.A500"
#define A600_ROM        "kick40063.A600"
#define A1200_ROM       "kick40068.A1200"
#define CD32_ROM        "kick40060.CD32"
#define CD32_ROM_EXT    "kick40060.CD32.ext"

bool isCD32 = false;

#ifdef _WIN32
#define RETRO_PATH_SEPARATOR            "\\"
// Windows also support the unix path separator
#define RETRO_PATH_SEPARATOR_ALT        "/"
#else
#define RETRO_PATH_SEPARATOR            "/"
#endif

void path_join(char* out, const char* basedir, const char* filename)
{
   snprintf(out, MAX_PATH, "%s%s%s", basedir, RETRO_PATH_SEPARATOR, filename);
}


void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

  static const struct retro_controller_description p1_controllers[] = {
    { "AMIGA Joystick", RETRO_DEVICE_AMIGA_JOYSTICK },
    { "AMIGA Keyboard", RETRO_DEVICE_AMIGA_KEYBOARD },
  };
  static const struct retro_controller_description p2_controllers[] = {
    { "AMIGA Joystick", RETRO_DEVICE_AMIGA_JOYSTICK },
    { "AMIGA Keyboard", RETRO_DEVICE_AMIGA_KEYBOARD },
  };

  static const struct retro_controller_info ports[] = {
    { p1_controllers, 2  }, // port 1
    { p2_controllers, 2  }, // port 2
    { NULL, 0 }
  };

  cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );

  struct retro_variable variables[] = {
      { "uae4arm_model",          "Model; Auto|A500|A600|A1200|CD32", },
      { "uae4arm_fastmem",        "Fast Mem; None|1 MB|2 MB|4 MB|8 MB", },
      { "uae4arm_resolution",     "Internal resolution; 640x270|320x240|320x256|320x262|640x240|640x256|640x262|640x270|768x270", },
      { "uae4arm_leds_on_screen", "Leds on screen; on|off", },
      { "uae4arm_floppy_speed",   "Floppy speed; 100|200|400|800", },
      { "uae4arm_linedoubling",   "Line doubling (de-interlace); off|on", },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}


void Retro_Msg(const char * msg_str)
{
   if (msg_interface_version >= 1)
   {
      struct retro_message_ext msg = {
         msg_str,
         3000,
         3,
         RETRO_LOG_WARN,
         RETRO_MESSAGE_TARGET_ALL,
         RETRO_MESSAGE_TYPE_NOTIFICATION,
         -1
      };
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
   }
   else
   {
      struct retro_message msg = {
         msg_str,
         180
      };
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
   }
}

void gz_uncompress(gzFile in, FILE *out)
{
   char gzbuf[16384];
   int len;
   int err;

   for (;;)
   {
      len = gzread(in, gzbuf, sizeof(gzbuf));
      if (len < 0)
         fprintf(stdout, "%s", gzerror(in, &err));
      if (len == 0)
         break;
      if ((int)fwrite(gzbuf, 1, (unsigned)len, out) != len)
         fprintf(stdout, "Write error!\n");
   }
}



void update_prefs_retrocfg(struct uae_prefs * prefs)
{

   struct retro_variable var;

   var.key = "uae4arm_resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), var.value);

      pch = strtok(str, "x");
      if (pch)
         retrow = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         retroh = strtoul(pch, NULL, 0);

      prefs->gfx_size.width  = retrow;
      prefs->gfx_size.height = retroh;
      prefs->gfx_resolution  = prefs->gfx_size.width > 600 ? 1 : 0;

      LOGI("[libretro-uae4arm]: Got size: %u x %u.\n", retrow, retroh);

      CROP_WIDTH =retrow;
      CROP_HEIGHT= (retroh-80);
      VIRTUAL_WIDTH = retrow;
      texture_init();

   }

   var.key = "uae4arm_leds_on_screen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "on")  == 0) prefs->leds_on_screen = 1;
      if (strcmp(var.value, "off") == 0) prefs->leds_on_screen = 0;
   }

   var.key = "uae4arm_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      LOGI("[libretro-uae4arm]: Got model: %s.\n", var.value);

      if (strcmp(var.value, "Auto") == 0)
      {
         if ((strcasestr(RPATH,".cue") != NULL) || (strcasestr(RPATH,".ccd") != NULL) || (strcasestr(RPATH,".iso") != NULL))
         {
            LOGI("[libretro-uae4arm]: Auto-model -> CD32 selected\n");
            var.value = "CD32";
         }
         else if (strcasestr(RPATH,"aga") != NULL)
         {
            LOGI("[libretro-uae4arm]: Auto-model -> A1200 selected\n");
            var.value = "A1200";
         }
         else
         {
            if ((strcasestr(RPATH,".hdf") != NULL) || (strcasestr(RPATH,".lha") != NULL))
            {
               if (strcasestr(RPATH,"cd32") != NULL)
               {
                  // Some whdload has cd32 in their name...
                  LOGI("[libretro-uae4arm]: Auto-model -> A1200 selected\n");
                  var.value = "A1200";
               }
               else
               {
                  LOGI("[libretro-uae4arm]: Auto-model -> A600 selected\n");
                  var.value = "A600";
               }
            }
            else
            {
               LOGI("[libretro-uae4arm]: Auto-model -> A500 selected\n");
               var.value = "A500";
            }
         }
      }

      // Treat .lha files as whdload slave. A better implementation would use zfile_isdiskimage...
      if (strcasestr(RPATH,".lha") != NULL)
      {

          char whdload_hdf[512] = {0};
          path_join((char*)&whdload_hdf, retro_save_directory, "WHDLoad.hdf");

          /* Verify WHDLoad.hdf */
          if (!my_existsfile(whdload_hdf))
          {
             LOGI("[libretro-uae4arm]: WHDLoad image file '%s' not found, attempting to create one\n", (const char*)&whdload_hdf);

             char whdload_hdf_gz[512];
             path_join((char*)&whdload_hdf_gz, retro_save_directory, "WHDLoad.hdf.gz");

             FILE *whdload_hdf_gz_fp;
             if ((whdload_hdf_gz_fp = fopen(whdload_hdf_gz, "wb")))
             {
                /* Write GZ */
                fwrite(___whdload_WHDLoad_hdf_gz, ___whdload_WHDLoad_hdf_gz_len, 1, whdload_hdf_gz_fp);
                fclose(whdload_hdf_gz_fp);

                /* Extract GZ */
                struct gzFile_s *whdload_hdf_gz_fp;
                if ((whdload_hdf_gz_fp = gzopen(whdload_hdf_gz, "r")))
                {
                   FILE *whdload_hdf_fp;
                   if ((whdload_hdf_fp = fopen(whdload_hdf, "wb")))
                   {
                      gz_uncompress(whdload_hdf_gz_fp, whdload_hdf_fp);
                      fclose(whdload_hdf_fp);
                   }
                   gzclose(whdload_hdf_gz_fp);
                }
                remove(whdload_hdf_gz);
             }
             else
                LOGI("[libretro-uae4arm]: Unable to create WHDLoad image file: '%s'\n", (const char*)&whdload_hdf);
          }

          /* Attach HDF */
          if (my_existsfile(whdload_hdf))
          {
              uaedev_config_info ci;
              struct uaedev_config_data *uci;

              LOGI("[libretro-uae4arm]: Attach HDF\n");

              uci_set_defaults(&ci, false);
              strncpy(ci.devname, "WHDLoad", MAX_DPATH);
              strncpy(ci.rootdir, whdload_hdf, MAX_DPATH);
              ci.type = UAEDEV_HDF;
              ci.controller_type = 0;        // UAE vs IDE
              ci.controller_type_unit = 0;
              ci.controller_unit = 0;
              ci.controller_media_type = 0;
              ci.unit_feature_level = 1;
              ci.unit_special_flags = 0;
              ci.readonly = 0; // ro should be ok
              ci.sectors = 32;
              ci.surfaces = 1;
              ci.reserved = 2;
              ci.blocksize = 512;
              ci.bootpri = 0;
              ci.physical_geometry = hardfile_testrdb(ci.rootdir);

              //tmp_str = string_replace_substring(whdload_hdf, "\\", "\\\\");
              uci = add_filesys_config (prefs, -1,&ci);
              //fprintf(configfile, "hardfile2=rw,WHDLoad:\"%s\",32,1,2,512,0,,uae0\n", (const char*)tmp_str);
              if (uci) {
  		  struct hardfiledata *hfd = get_hardfile_data (uci->configoffset);
                  if(hfd)
                      hardfile_media_change (hfd, &ci, true, false);
                  else
                      hardfile_added(&ci);
              }
          }
          /* Attach LHA */
          struct uaedev_config_info ci;
          struct uaedev_config_data *uci;
    
          LOGI("[libretro-uae4arm]: Attach LHA\n");

          uci_set_defaults(&ci, true);
          strncpy(ci.devname, "DH0", MAX_DPATH);
          strncpy(ci.volname, "LHA", MAX_DPATH);
          strncpy(ci.rootdir, RPATH, MAX_DPATH);
          ci.type = UAEDEV_DIR;
          ci.readonly = 0;
          ci.bootpri = -128;
    
          uci = add_filesys_config(prefs, -1, &ci);
          if (uci) {
            filesys_media_change (ci.rootdir, 1, uci);
          }
      }

      if (strcmp(var.value, "A600") == 0)
      {
         prefs->cpu_model = 68000;
         prefs->chipmem_size = 2 * 0x80000;
         prefs->bogomem_size = 0;
         prefs->m68k_speed = M68K_SPEED_7MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, A600_ROM);
         path_join(prefs->romfile, retro_system_directory, A600_ROM);
      }
      else if (strcmp(var.value, "A1200") == 0)
      {
         //prefs->cpu_type="68ec020";
         prefs->cpu_model = 68020;
         prefs->chipmem_size = 4 * 0x80000;
         prefs->bogomem_size = 0;
         prefs->m68k_speed = M68K_SPEED_14MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, A1200_ROM);
         path_join(prefs->romfile, retro_system_directory, A1200_ROM);
      }
      else if (strcmp(var.value, "CD32") == 0)
      {
         #define DRV_NONE -1
         #define SCSI_UNIT_IMAGE 1

         isCD32 = true;
         //prefs->cpu_type="68ec020";
         prefs->cpu_model = 68020;
         prefs->chipmem_size = 4 * 0x80000;
         prefs->bogomem_size = 0;
         prefs->m68k_speed = M68K_SPEED_14MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, CD32_ROM);
         path_join(prefs->romfile,    retro_system_directory, CD32_ROM);
         path_join(prefs->romextfile, retro_system_directory, CD32_ROM_EXT);
         prefs->floppyslots[0].dfxtype = DRV_NONE;
         prefs->floppyslots[1].dfxtype = DRV_NONE;
         prefs->cdslots[0].inuse = true;
         prefs->cdslots[0].type = SCSI_UNIT_IMAGE;
         prefs->cs_cd32c2p = prefs->cs_cd32cd = prefs->cs_cd32nvram = true;
         prefs->cs_compatible = CP_CD32;
         prefs->nr_floppies=0;
         prefs->bogomem_size = 0;
         prefs->jports[1].mode = JSEM_MODE_JOYSTICK_CD32;
         //built_in_prefs (&currprefs, 5, 1, 0, 0);
      }
      else // if (strcmp(var.value, "A500") == 0)
      {
         //prefs->cpu_type="68000";
         prefs->cpu_model = 68000;
         prefs->m68k_speed = M68K_SPEED_7MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->chipmem_size = 2 * 0x80000;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, A500_ROM);
         path_join(prefs->romfile, retro_system_directory, A500_ROM);
      }
   }


   var.key = "uae4arm_fastmem";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "None") == 0)
      {
         prefs->fastmem[0].size = 0;
      }
      if (strcmp(var.value, "1 MB") == 0)
      {
         prefs->fastmem[0].size = 0x100000;
      }
      if (strcmp(var.value, "2 MB") == 0)
      {
         prefs->fastmem[0].size = 0x100000 * 2;
      }
      if (strcmp(var.value, "4 MB") == 0)
      {
         prefs->fastmem[0].size = 0x100000 * 4;
      }
      if (strcmp(var.value, "8 MB") == 0)
      {
         prefs->fastmem[0].size = 0x100000 * 8;
      }
   }


   var.key = "uae4arm_floppy_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      prefs->floppy_speed=atoi(var.value);
   }

   var.key = "uae4arm_linedoubling";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "on")  == 0) prefs->gfx_vresolution = 1;
      if (strcmp(var.value, "off") == 0) prefs->gfx_vresolution = 0;
   }

}

static void retro_wrap_emulator()
{    

   pre_main(RPATH);

   LOGI("EXIT EMU THD\n");
   pauseg=-1;

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   while(true)
   {
      LOGI("Running a dead emulator.");
      co_switch(mainThread);
   }

}

void Emu_init(){

   memset(Key_Sate,0,512);
   memset(Key_Sate2,0,512);

   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(8*65536*sizeof(void*), retro_wrap_emulator);
   }

}

void Emu_uninit(){
   texture_uninit();
   uae_quit ();
}

void retro_shutdown_core(void)
{
   LOGI("SHUTDOWN\n");

   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void)
{
   uae_reset(1,1);
}

void retro_init(void)
{
   const char *system_dir = NULL;

   msg_interface_version = 0;
   environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &msg_interface_version);

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory
      retro_system_directory=system_dir;
   }

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory=content_dir;
   }		

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)
      sprintf(RETRO_DIR, "%s\0", ".");
   else
      sprintf(RETRO_DIR, "%s\0", retro_system_directory);

   sprintf(retro_system_data_directory, "%s/data\0",RETRO_DIR);

   LOGI("Retro SYSTEM_DIRECTORY %s\n", retro_system_directory );
   LOGI("Retro SAVE_DIRECTORY %s\n",   retro_save_directory   );
   LOGI("Retro CONTENT_DIRECTORY %s\n",retro_content_directory);

#ifndef RENDER16B
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif
   
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "PIXEL FORMAT is not supported.\n");
      LOGI("PIXEL FORMAT is not supported.\n");
      exit(0);
   }

	struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,  "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,  "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" }
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

   Emu_init();

   texture_init();

}

extern void main_exit();
void retro_deinit(void)
{
   Emu_uninit(); 

   co_switch(emuThread);
   LOGI("exit emu\n");

   co_switch(mainThread);
   LOGI("exit main\n");
   if(emuThread)
   {
      co_delete(emuThread);
      emuThread = 0;
   }

   LOGI("Retro DeInit\n");
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}


void retro_set_controller_port_device( unsigned port, unsigned device )
{
  if ( port < 2 )
  {
    amiga_devices[ port ] = device;

    LOGI(" (%d)=%d \n",port,device);
  }
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "uae4arm";
   info->library_version  = "0.5";
   info->valid_extensions = "adf|dms|zip|ipf|hdf|lha|uae|cue|iso";
   info->need_fullpath    = true;
   info->block_extract    = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   //FIXME handle vice PAL/NTSC
   struct retro_game_geometry geom = { retrow, retroh, 1280, 1024,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}


void retro_audiocb(signed short int *sound_buffer,int sndbufsize){

    if(pauseg==0)
        audio_batch_cb(sound_buffer, sndbufsize);
}


extern int Retro_PollEvent();

void retro_run(void)
{
   int x;
   bool updated = false;
   static bool AvoidFirstPoll = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_prefs_retrocfg(&changed_prefs);

   // First poll segfault since emu not initialized...
   if (AvoidFirstPoll == 0)
      AvoidFirstPoll = 1;
   else
      Retro_PollEvent();

   co_switch(emuThread);

   if(pauseg==0)
      if(SHOWKEY)
         retro_virtualkb();

   video_cb(Retro_Screen,retrow,retroh * 1 << (currprefs.gfx_vresolution),retrow<<PIXEL_BYTES);

}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path;

   (void)info;

   full_path = info->path;

   strcpy(RPATH,full_path);

#ifdef RENDER16B
   memset(Retro_Screen,0,1280*1024*2);
#else
   memset(Retro_Screen,0,1280*1024*2*2);
#endif

   co_switch(emuThread);

   return true;
}

void retro_unload_game(void){

   pauseg=-1;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_PAL;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

