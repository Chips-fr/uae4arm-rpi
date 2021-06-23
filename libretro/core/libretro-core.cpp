#include "libretro.h"

#ifdef VITA
#define __GNU_VISIBLE 1
#endif
#include "libretro-core.h"

#include "libretro/retrodep/WHDLoad_files.zip.c"
#include "libretro/retrodep/WHDLoad_hdf.gz.c"
//#include "libretro/retrodep/WHDSaves_hdf.gz.c"
//#include "libretro/retrodep/WHDLoad_prefs.gz.c"

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "custom.h"

#include "uae.h"
#include "savestate.h"

#include "zlib.h"
#include "zfile.h"
#include "fsdb.h"
#include "filesys.h"
#include "autoconf.h"

cothread_t mainThread;
cothread_t emuThread;

unsigned int CROP_WIDTH;
unsigned int CROP_HEIGHT;
unsigned int VIRTUAL_WIDTH ;
unsigned int retrow=320; 
unsigned int retroh=240;
static unsigned msg_interface_version = 0;
unsigned int opt_use_whdload = 1;

#define RETRO_DEVICE_AMIGA_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)
#define RETRO_DEVICE_AMIGA_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)

unsigned amiga_devices[ 2 ];

extern int SHIFTON,pauseg,SND ,snd_sampler;
extern short signed int SNDBUF[1024*2];
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

struct zfile *retro_deserialize_file = NULL;
static size_t save_state_file_size = 0;

// Amiga default kickstarts

#define A500_ROM        "kick34005.A500"
#define A600_ROM        "kick40063.A600"
#define A1200_ROM       "kick40068.A1200"

static char uae_machine[256];
static char uae_kickstart[16];
static char uae_config[1024];


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
      { "uae4arm_model",          "Model; Auto|A500|A600|A1200", },
      { "uae4arm_fastmem",        "Fast Mem; None|1 MB|2 MB|4 MB|8 MB", },
      { "uae4arm_resolution",     "Internal resolution; 640x270|320x240|320x256|320x262|640x240|640x256|640x262|640x270|768x270", },
      { "uae4arm_leds_on_screen", "Leds on screen; on|off", },
      { "uae4arm_floppy_speed",   "Floppy speed; 100|200|400|800", },
      { "uae4arm_fastcopper",     "fast copper; on|off", },
      { "uae4arm_whdloadmode",    "whdload mode; files|hdfs"},
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

void Retro_Kickstart_Replacement_Msg(void)
{

   static char Already_done = 0;

   if (Already_done)
      return;

   const char *msg_str = "No Kickstart file found - add for better compatibility";

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

   Already_done = 1;

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

#include "archivers/zip/unzip.h"
//#include "file/file_path.h"
#include "zfile.h"
#ifdef WIN32
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP_STR "/"
#endif
void zip_uncompress(char *in, char *out, char *lastfile)
{
    unzFile uf = NULL;

    struct zfile *zf;
    // In this code base zfile_fopen return the uncompress zfile whereas new code base returned the compress one...
    // We can use fopen_2 instead...
    //zf = zfile_fopen(in, "rb");
    zf = zfile_fopen_2(in, "rb");
    uf = unzOpen(zf);

    uLong i;
    unz_global_info gi;
    int err;
    err = unzGetGlobalInfo (uf, &gi);

    const char* password = NULL;
    int size_buf = 8192;

    for (i = 0; i < gi.number_entry; i++)
    {
        char filename_inzip[256];
        char filename_withpath[512];
        filename_inzip[0] = '\0';
        filename_withpath[0] = '\0';
        char* filename_withoutpath;
        char* p;
        unz_file_info file_info;
        FILE *fout = NULL;
        void* buf;

        buf = (void*)malloc(size_buf);
        if (buf == NULL)
        {
            fprintf(stderr, "Unzip: Error allocating memory\n");
            return;
        }

        err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        snprintf(filename_withpath, sizeof(filename_withpath), "%s%s%s", out, DIR_SEP_STR, filename_inzip);
        //if (dc_get_image_type(filename_inzip) == DC_IMAGE_TYPE_FLOPPY && lastfile != NULL)
        //    snprintf(lastfile, RETRO_PATH_MAX, "%s", filename_inzip);

        p = filename_withoutpath = filename_inzip;
        while ((*p) != '\0')
        {
            if (((*p) == '/') || ((*p) == '\\'))
                filename_withoutpath = p+1;
            p++;
        }

        if ((*filename_withoutpath) == '\0')
        {
            fprintf(stdout, "Unzip mkdir:   %s\n", filename_withpath);
            my_mkdir(filename_withpath);
        }
        else
        {
            const char* write_filename;
            int skip = 0;

            write_filename = filename_withpath;

            err = unzOpenCurrentFile(uf);
            if (err != UNZ_OK)
            {
                fprintf(stderr, "Unzip: Error %d with zipfile in unzOpenCurrentFilePassword: %s\n", err, write_filename);
            }

            if ((skip == 0) && (err == UNZ_OK))
            {
                fout = fopen(write_filename, "wb");
                if (fout == NULL)
                {
                    fprintf(stderr, "Unzip: Error opening %s\n", write_filename);
                }
            }

            if (fout != NULL)
            {
                fprintf(stdout, "Unzip extract: %s\n", write_filename);

                do
                {
                    err = unzReadCurrentFile(uf, buf, size_buf);
                    if (err < 0)
                    {
                        fprintf(stderr, "Unzip: Error %d with zipfile in unzReadCurrentFile\n", err);
                        break;
                    }
                    if (err > 0)
                    {
                        if (!fwrite(buf, err, 1, fout))
                        {
                            fprintf(stderr, "Unzip: Error in writing extracted file\n");
                            err = UNZ_ERRNO;
                            break;
                        }
                    }
                }
                while (err > 0);
                if (fout)
                    fclose(fout);
            }

            if (err == UNZ_OK)
            {
                err = unzCloseCurrentFile(uf);
                if (err != UNZ_OK)
                {
                    fprintf(stderr, "Unzip: Error %d with zipfile in unzCloseCurrentFile\n", err);
                }
            }
            else
                unzCloseCurrentFile(uf);
        }

        free(buf);

        if ((i+1) < gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err != UNZ_OK)
            {
                fprintf(stderr, "Unzip: Error %d with zipfile in unzGoToNextFile\n", err);
                break;
            }
        }
    }

    if (uf)
    {
        unzCloseCurrentFile(uf);
        unzClose(uf);
        uf = NULL;
    }
}


void update_prefs_retrocfg(struct uae_prefs * prefs)
{
   uae_machine[0] = '\0';
   uae_config[0]  = '\0';

   struct retro_variable var = {0};

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
      if (strcmp(var.value, "on") == 0)  prefs->leds_on_screen = 1;
      if (strcmp(var.value, "off") == 0) prefs->leds_on_screen = 0;
   }

   var.key = "uae4arm_fastmem";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "None") == 0)
      {
         prefs->fastmem_size = 0;
      }
      if (strcmp(var.value, "1 MB") == 0)
      {
         prefs->fastmem_size = 0x100000;
      }
      if (strcmp(var.value, "2 MB") == 0)
      {
         prefs->fastmem_size = 0x100000 * 2;
      }
      if (strcmp(var.value, "4 MB") == 0)
      {
         prefs->fastmem_size = 0x100000 * 4;
      }
      if (strcmp(var.value, "8 MB") == 0)
      {
         prefs->fastmem_size = 0x100000 * 8;
      }
   }

   var.key = "uae4arm_whdloadmode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "hdfs") == 0)
      {
         opt_use_whdload = 2;
      }
      else
      {
         opt_use_whdload = 1;
      }
   }

   var.key = "uae4arm_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      LOGI("[libretro-uae4arm]: Got model: %s.\n", var.value);
      if (strcmp(var.value, "Auto") == 0)
      {
         if ((strcasestr(RPATH,"aga") != NULL) || (strcasestr(RPATH,"amiga1200") != NULL))
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
                  // Some whdload have cd32 in their name...
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

          char whdload_hdf[PATH_MAX] = {0};
          char tmp_str [PATH_MAX];
          struct uaedev_config_info *uci;



          // WHDLoad HDF mode
          if (opt_use_whdload == 2)
          {

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
                  //uaedev_config_info ci;
                  struct uaedev_config_info *uci;

                  LOGI("[libretro-uae4arm]: Attach HDF\n");

                  uci = add_filesys_config(prefs, -1, "WHDLoad", 0, whdload_hdf, 0, 
                        32, 1, 2, 512, 0, 0, 0, 0);

                  if (uci)
                      hardfile_do_disk_change (uci, 1);
              }



          }
          // WHDLoad File mode
          else
          {

             char whdload_path[PATH_MAX];
             path_join((char*)&whdload_path, retro_save_directory, "WHDLoad");

             char whdload_c_path[PATH_MAX];
             path_join((char*)&whdload_c_path, retro_save_directory, "WHDLoad/C");

             if (!my_existsdir(whdload_path) || (my_existsdir(whdload_path) && !my_existsdir(whdload_c_path)))
             {
                LOGI("[libretro-uae4arm]: WHDLoad image directory '%s' not found, attempting to create one\n", (const char*)&whdload_path);
                my_mkdir(whdload_path);

                char whdload_files_zip[PATH_MAX];
                path_join((char*)&whdload_files_zip, retro_save_directory, "WHDLoad_files.zip");

                FILE *whdload_files_zip_fp;
                if (whdload_files_zip_fp = fopen(whdload_files_zip, "wb"))
                {
                   // Write ZIP
                   fwrite(___whdload_WHDLoad_files_zip, ___whdload_WHDLoad_files_zip_len, 1, whdload_files_zip_fp);
                   fclose(whdload_files_zip_fp);

                   // Extract ZIP
                   zip_uncompress(whdload_files_zip, whdload_path, NULL);
                   remove(whdload_files_zip);
                }
                else
                   LOGI("[libretro-uae4arm]: Error extracting WHDLoad directory: '%s'!\n", (const char*)&whdload_path);
             }
             if (my_existsdir(whdload_path) && my_existsdir(whdload_c_path))
             {
                LOGI("[libretro-uae4arm]: Attach WHDLoad files\n");
                //tmp_str = string_replace_substring(whdload_path, "\\", "\\\\");
                //fprintf(configfile, "filesystem2=rw,WHDLoad:WHDLoad:\"%s\",0\n", (const char*)tmp_str);
                //free(tmp_str);
                //tmp_str = NULL;

                //uci_set_defaults(&ci, true);
                //strncpy(ci.devname, "WHDLoad",     MAX_DPATH);
                //strncpy(ci.volname, "WHDLoad",     MAX_DPATH);
                //strncpy(ci.rootdir, whdload_path,  MAX_DPATH);
                //ci.type = UAEDEV_DIR;
                //ci.readonly = 0;
                //ci.bootpri  = 0;

                //uci = add_filesys_config(prefs, -1, &ci);
                //if (uci) {
                //  filesys_media_change (ci.rootdir, 1, uci);
                //}

                uci = add_filesys_config(prefs, -1, "WHDLoad", "WHDLoad", whdload_path, 
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
                if (uci)
                    filesys_media_change (uci->rootdir, 1, uci);
             }
             else
                LOGI("[libretro-uae4arm]: Error creating WHDLoad directory: '%s'!\n", (const char*)&whdload_path);

          }


          // Attach retro_system_directory as a read only hard drive for WHDLoad kickstarts/prefs/key
          LOGI("[libretro-uae4arm]: Attach whdcommon\n");

          // Force the ending slash to make sure the path is not treated as a file
          if (retro_system_directory[strlen(retro_system_directory) - 1] != '/')
             sprintf(tmp_str,"%s%s",retro_system_directory, "/");
          else
             sprintf(tmp_str,"%s",retro_system_directory);

          uci = add_filesys_config(prefs, -1, "whdcommon", "", tmp_str, 
            0, 0, 0, 0, 0, -128, 0, 0, 0);

          if (uci)
              filesys_media_change (uci->rootdir, 1, uci);



          // WHDSaves file mode
          {
             LOGI("[libretro-uae4arm]: Attach WHDSaves files\n");

             char whdsaves_path[MAX_DPATH];
             path_join((char*)&whdsaves_path, retro_save_directory, "WHDSaves");
             if (!my_existsdir(whdsaves_path))
                my_mkdir(whdsaves_path);
             if (my_existsdir(whdsaves_path))
             {
                  uci = add_filesys_config(prefs, -1, "WHDSaves", "WHDSaves", whdsaves_path, 
                    0, 0, 0, 0, 0, -128, 0, 0, 0);

                  if (uci)
                      filesys_media_change (uci->rootdir, 1, uci);
              }
          }


          /* Attach LHA */

          LOGI("[libretro-uae4arm]: Attach LHA\n");

          uci = add_filesys_config(prefs, -1, "DH0", "LHA", RPATH, 
            0, 0, 0, 0, 0, -128, 0, 0, 0);
          if (uci)
              filesys_media_change (uci->rootdir, 1, uci);

          // Temp: Add automatically 8 MBytes of Fast...
          prefs->fastmem_size = 0x100000 * 8;

      }

      if (strcmp(var.value, "A600") == 0)
      {
         //strcat(uae_machine, A600);
         //strcpy(uae_kickstart, A600_ROM);
         prefs->cpu_model = 68000;
         prefs->chipmem_size = 2 * 0x80000;
         prefs->m68k_speed = M68K_SPEED_7MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, A600_ROM);
         path_join(prefs->romfile, retro_system_directory, A600_ROM);
      }
      else if (strcmp(var.value, "A1200") == 0)
      {
         //strcat(uae_machine, A1200);
         //strcpy(uae_kickstart, A1200_ROM);
         //prefs->cpu_type="68ec020";
         prefs->cpu_model = 68020;
         prefs->chipmem_size = 4 * 0x80000;
         prefs->m68k_speed = M68K_SPEED_14MHZ_CYCLES;
         prefs->cpu_compatible = 0;
         prefs->address_space_24 = 1;
         prefs->chipset_mask = CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(prefs->romfile, A1200_ROM);
         path_join(prefs->romfile, retro_system_directory, A1200_ROM);
      }
      else // if (strcmp(var.value, "A500") == 0)
      {
         //strcat(uae_machine, A500);
         //strcpy(uae_kickstart, A500_ROM);
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

   var.key = "uae4arm_floppy_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      prefs->floppy_speed=atoi(var.value);
   }

   var.key = "uae4arm_fastcopper";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "on")  == 0) prefs->fast_copper = 1;
      if (strcmp(var.value, "off") == 0) prefs->fast_copper = 0;
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
}

void retro_shutdown_core(void)
{
   LOGI("SHUTDOWN\n");

   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void)
{
   uae_reset(1);
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

   if(retro_system_directory==NULL)sprintf(RETRO_DIR, "%s\0",".");
   else sprintf(RETRO_DIR, "%s\0", retro_system_directory);

   sprintf(retro_system_data_directory, "%s/data\0",RETRO_DIR);

   LOGI("Retro SYSTEM_DIRECTORY %s\n",retro_system_directory);
   LOGI("Retro SAVE_DIRECTORY %s\n",retro_save_directory);
   LOGI("Retro CONTENT_DIRECTORY %s\n",retro_content_directory);

#ifndef RENDER16B
   enum retro_pixel_format fmt =RETRO_PIXEL_FORMAT_XRGB8888;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif
   
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "PIXEL FORMAT is not supported.\n");
      LOGI("PIXEL FORMAT is not supported.\n");
      exit(0);
   }

   // Savestates
   // > Considered incomplete because runahead cannot
   //   be enabled until content is full loaded
   static uint64_t quirks = RETRO_SERIALIZATION_QUIRK_INCOMPLETE;
   environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);

   // > Ensure save state de-serialization file
   //   is closed/NULL
   //   (redundant safety check, possibly required
   //   for static builds...)
   if (retro_deserialize_file)
   {
      zfile_fclose(retro_deserialize_file);
      retro_deserialize_file = NULL;
   }

   struct retro_input_descriptor inputDescriptors[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"   },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"     },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"   },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "R2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "L2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "R3" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "L3" }
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
   info->library_name     = "uae4arm_chips-rtype_version";
   info->library_version  = "0.3";
   info->valid_extensions = "adf|dms|zip|ipf|hdf|lha|uae";
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


void retro_run(void)
{
   bool updated = false;
   static int Deffered = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_prefs_retrocfg(&changed_prefs);

   // Too early event poll or savesate segfault since emu not initialized...
   if (Deffered == 0)
   {
      Deffered = 1;
   }
   else
   {
      if (Deffered == 1)
      {
         // Save states
         // > Ensure that save state file path is empty,
         //   since we use memory based save states
         savestate_fname[0] = '\0';
         // > Get save state size
         //   Here we use initial size + 5%
         //   Should be sufficient in all cases
         // NOTE: It would be better to calculate the
         // state size based on current config parameters,
         // but while
         //   - currprefs.chipmem_size
         //   - currprefs.bogomem_size
         //   - currprefs.fastmem_size
         // account for *most* of the size, there are
         // simply too many other factors to rely on this
         // alone (i.e. mem size + 5% is fine in most cases,
         // but if the user supplies a custom uae config file
         // then this is not adequate at all). Untangling the
         // full set of values that are recorded is beyond
         // my patience...
         struct zfile *state_file = save_state("libretro", 0);

         if (state_file)
         {
            save_state_file_size  = (size_t)zfile_size(state_file);
            save_state_file_size += (size_t)(((float)save_state_file_size * 0.05f) + 0.5f);
            zfile_fclose(state_file);
         }
         Deffered = 2;
      }

      //Retro_PollEvent();
   }

   co_switch(emuThread);

   if(pauseg==0)
      if(SHOWKEY )
         retro_virtualkb();

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);

}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path;

   full_path = info->path;

   strcpy(RPATH,full_path);

#ifdef RENDER16B
   memset(Retro_Screen,0,1280*1024*2);
#else
   memset(Retro_Screen,0,1280*1024*2*2);
#endif
   memset(SNDBUF,0,1024*2*2);

   co_switch(emuThread);

   return true;
}

void retro_unload_game(void)
{
   // Ensure save state de-serialization file
   // is closed/NULL
   // Note: Have to do this here (not in retro_deinit())
   // since leave_program() calls zfile_exit()
   if (retro_deserialize_file)
   {
      zfile_fclose(retro_deserialize_file);
      retro_deserialize_file = NULL;
   }

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
   return save_state_file_size;
}

bool retro_serialize(void *data_, size_t size)
{
   struct zfile *state_file = save_state("libretro", (uae_u64)save_state_file_size);
   bool success = false;

   if (state_file)
   {
      uae_s64 state_file_size = zfile_size(state_file);

      if (size >= state_file_size)
      {
         size_t len = zfile_fread(data_, 1, state_file_size, state_file);

         if (len == state_file_size)
            success = true;
      }

      zfile_fclose(state_file);
   }

   return success;
}

bool retro_unserialize(const void *data_, size_t size)
{
   // TODO: When attempting to use runahead, CD32
   // and WHDLoad content will hang on boot. It seems
   // we cannot restore a state until the system has
   // passed some level of initialisation - but the
   // point at which a restore becomes 'safe' is
   // unknown (for CD32 content, for example, we have
   // to wait ~300 frames before runahead can be enabled)
   bool success = false;

   // Cannot restore state while any 'savestate'
   // operation is underway
   // > Actual restore is deferred until m68k_go(),
   //   so we have to use a shared shared state file
   //   object - this cannot be modified until the
   //   restore is complete
   // > Note that this condition should never be
   //   true - if a save state operation is underway
   //   at this point then we are dealing with an
   //   unknown error
   if (!savestate_state)
   {
      // Savestates also save CPU prefs, therefore refresh core options, but skip it for now
      //request_check_prefs_timer = 4;

      if (retro_deserialize_file)
      {
         zfile_fclose(retro_deserialize_file);
         retro_deserialize_file = NULL;
      }

      retro_deserialize_file = zfile_fopen_empty("libretro", size);

      if (retro_deserialize_file)
      {
         size_t len = zfile_fwrite(data_, 1, size, retro_deserialize_file);

         if (len == size)
         {
            unsigned frame_counter = 0;
            unsigned max_frames    = 50;

            zfile_fseek(retro_deserialize_file, 0, SEEK_SET);
            savestate_state = STATE_DORESTORE;

            // For correct operation of the frontend,
            // the save state restore must be completed
            // by the time this function returns.
            // Since P-UAE requires several (2) frames to get
            // itself in order during a restore event, we
            // have to keep emulating frames until the
            // restore is complete...
            // > Note that we set a 'timeout' of 50 frames
            //   here (1s of emulated time at 50Hz) to
            //   prevent lock-ups in the event of unexpected
            //   errors
            // > Temporarily 'deactivate' runloop - this lets
            //   us call m68k_go() without accessing frontend
            //   features - specifically, it disables the audio
            //   callback functionality
#if 0 // TEMP
            libretro_runloop_active = 0;
            while (savestate_state && (frame_counter < max_frames))
            {
               // Note that retro_deserialize_file will be
               // closed inside m68k_go() upon successful
               // completion of the restore event
               restart_pending = m68k_go(1, 1);
               frame_counter++;
            }
            libretro_runloop_active = 1;

            // If the above while loop times out, then
            // everything is completely broken. We cannot
            // handle this here, so just assume the restore
            // completed successfully...
            request_reset_drawing = true;
#endif
            success               = true;
         }
         else
         {
            zfile_fclose(retro_deserialize_file);
            retro_deserialize_file = NULL;
         }
      }
   }

   return success;
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

