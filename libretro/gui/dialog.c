/*
	modded for libretro-vice
*/

/*
  Hatari - dialog.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Code to handle our options dialog.
*/
const char Dialog_fileid[] = "Hatari dialog.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"

bool bQuitProgram=false;
extern int pauseg;
int retro_quit=0;

#ifdef SNAP_BMP 
#include "libretro-core.h"
unsigned int emubkg[96*72];
typedef unsigned  int  PIXEL;
extern void ScaleRect(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight,
      int TgtWidth, int TgtHeight);
extern void Screen_SetFullUpdate(int scr);
#endif

/*-----------------------------------------------------------------------*/
/**
 * Open Property sheet Options dialog.
 * 
 * We keep all our configuration details in a structure called
 * 'ConfigureParams'. When we open our dialog we make a backup
 * of this structure. When the user finally clicks on 'OK',
 * we can compare and makes the necessary changes.
 * 
 * Return true if user chooses OK, or false if cancel!
 */
bool Dialog_DoProperty(void)
{
   bool bOKDialog;  /* Did user 'OK' dialog? */	
   bool bForceReset;
   bool bLoadedSnapshot;

   bQuitProgram=false;


#ifdef SNAP_BMP 
   PIXEL *Source=(unsigned *)&Retro_Screen[0];
   PIXEL *Target=&emubkg[0];

	ScaleRect(Target, Source, 384, 272,96,72);
#endif

   bOKDialog = Dialog_MainDlg(&bForceReset, &bLoadedSnapshot);

   Screen_SetFullUpdate(2); //reset c64 src & emu scr

   if(bForceReset)
   {
	//  machine_trigger_reset(MACHINE_RESET_MODE_HARD);

      return bOKDialog;
   }

   if(bQuitProgram)
   {
		retro_quit=1;

   }

   return bOKDialog;
}
