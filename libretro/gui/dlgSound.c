/*
	modded for libretro-frodo
*/

/*
  Hatari - dlgSound.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgSound_fileid[] = "Hatari dlgSound.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"
#include "file.h"

#define DLGSOUND_none       4
#define DLGSOUND_digital    5
#define DLGSOUND_filters    6
#define DLGSOUND_EXIT       8

static char dlgRecordName[35];

/* The sound dialog: */
static SGOBJ sounddlg[] =
{
	{ SGBOX,      0,0,  0, 0, 40,26, NULL }, //0
	{ SGBOX,      0,0,  1, 1, 38,18, NULL },
	{ SGTEXT,     0,0,  4, 2, 13,1, "SOUND" },

	{ SGTEXT,     0,0,  4, 4, 14,1, "SID Emulation:" }, //3
	{ SGRADIOBUT, 0,0,  2, 6, 10,1, "None" },
	{ SGRADIOBUT, 0,0,  2, 7, 10,1, "Digital" },

	{ SGCHECKBOX, 0,0,  2, 9, 10,1, "SID filters" },
	{ SGBOX,      0,0,  1,18, 38,7, NULL }, //
	{ SGBUTTON, SG_EXIT/*SG_DEFAULT*/, 0, 10,23, 24,1, "Back to main menu" },//8
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show and process the sound dialog.
 */
void Dialog_SoundDlg(void)
{
	int but, i;

	SDLGui_CenterDlg(sounddlg);

	sounddlg[DLGSOUND_none].state&= ~SG_SELECTED;
	sounddlg[DLGSOUND_digital].state&= ~SG_SELECTED;
	sounddlg[DLGSOUND_filters].state&= ~SG_SELECTED;
/*
	if (ThePrefs.SIDType==SIDTYPE_NONE) { 
		sounddlg[DLGSOUND_none].state |= SG_SELECTED;
	}
	else if (ThePrefs.SIDType==SIDTYPE_DIGITAL) { 
		sounddlg[DLGSOUND_digital].state |= SG_SELECTED;
	}

	if (ThePrefs.SIDFilters) { // joy-1
		sounddlg[DLGSOUND_filters].state |= SG_SELECTED;
	}
*/
	/* The sound dialog main loop */
	do
	{	
		
		but = SDLGui_DoDialog(sounddlg, NULL);

                gui_poll_events();
	}
	while (but != DLGSOUND_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram );
/*

	if(sounddlg[DLGSOUND_none].state & SG_SELECTED){

		if(ThePrefs.SIDType!=SIDTYPE_NONE){
			prefs->	SIDType=SIDTYPE_NONE;
		}

	}
	else if(sounddlg[DLGSOUND_digital].state & SG_SELECTED){

		if(ThePrefs.SIDType!=SIDTYPE_DIGITAL){
			prefs->	SIDType=SIDTYPE_DIGITAL;
		}

	}

	if(sounddlg[DLGSOUND_filters].state & SG_SELECTED){

		if(!ThePrefs.SIDFilters){
			prefs->	SIDFilters=true;
		}

	}
	else {

		if(ThePrefs.SIDFilters){
			prefs->	SIDFilters=false;
		}

	}
*/

}

