/*
	modded for libretro-frodo
*/

/*
  Hatari - dlgJoystick.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgJoystick_fileid[] = "Hatari dlgJoystick.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"

#define DLGJOY_MSJY  3
#define DLGJOY_JYJY  4  
#define DLGJOY_MSMS  5
#define DLGJOY_EXIT  6

/* The joysticks dialog: */

static char sSdlStickName[20];

static SGOBJ joydlg[] =
{
	{ SGBOX, 0, 0, 0,0, 32,18, NULL },
	{ SGTEXT, 0, 0, 8,1, 15,1, "Joysticks setup" },

	{ SGBOX, 0, 0, 1,4, 30,11, NULL },
	{ SGCHECKBOX, 0, 0, 2,5, 10,1, "Enable Joy1" },
	{ SGCHECKBOX, 0, 0, 2,6, 20,1, "Enable Joy2" },
	{ SGCHECKBOX, 0, 0, 2,7, 14,1, "Swap Joy" },

	{ SGBUTTON, SG_EXIT/*SG_DEFAULT*/, 0, 6,16, 24,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

#ifdef DBGFONT
#include "graph.h"
#include "libretro-core.h"
#define bmp (char*)Retro_Screen
#endif

/*-----------------------------------------------------------------------*/
/**
 * Show and process the joystick dialog.
 */
void Dialog_JoyDlg(void)
{
	int but;

	SDLGui_CenterDlg(joydlg);

	joydlg[DLGJOY_MSJY].state &= ~SG_SELECTED;
	joydlg[DLGJOY_JYJY].state &= ~SG_SELECTED;
	joydlg[DLGJOY_MSMS].state &= ~SG_SELECTED;
/*
	if (ThePrefs.Joystick1Port) { // joy-1
		joydlg[DLGJOY_MSJY].state |= SG_SELECTED;
	}
	if (ThePrefs.Joystick2Port) { //joy-2
		joydlg[DLGJOY_JYJY].state |= SG_SELECTED;
	}
	if ( ThePrefs.JoystickSwap){ //swap
		joydlg[DLGJOY_MSMS].state |= SG_SELECTED;
	}
*/
	do
	{
    		but = SDLGui_DoDialog(joydlg, NULL);

#ifdef DBGFONT
for(int i=0;i<16;i++)
	for(int j=0;j<16;j++){
		Draw_text(bmp,i*16,j*16,1,0xffff0000,1,1,16,"%c",i+j*16);
	}
#endif

 		gui_poll_events();
	}
	while (but != DLGJOY_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
	
/*
	if(joydlg[DLGJOY_MSJY].state & SG_SELECTED){

		if(!ThePrefs.Joystick1Port)
			prefs->	Joystick1Port =1;

	}
	else if(ThePrefs.Joystick1Port)
			prefs->	Joystick1Port =0;

	if(joydlg[DLGJOY_JYJY].state & SG_SELECTED){

		if(!ThePrefs.Joystick2Port)
			prefs->	Joystick2Port =1;

	}
	else if(ThePrefs.Joystick2Port)
			prefs->	Joystick2Port =0;

	if(joydlg[DLGJOY_MSMS].state & SG_SELECTED){

		if(!ThePrefs.JoystickSwap)
			prefs->	JoystickSwap = true;

	}
	else if(ThePrefs.JoystickSwap)
		prefs->	JoystickSwap =false;
*/

}

