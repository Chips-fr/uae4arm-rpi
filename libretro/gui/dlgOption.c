/*
	modded for vice-libretro
*/

/*
  Hatari - dlgOption.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgOption_fileid[] = "Hatari dlgOption.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"
#include "file.h"

#define DLGOPTION_Mouse      3
#define DLGOPTION_MousePort  4
#define DLGOPTION_STATUS     5
#define DLGOPTION_EXIT       7

static char dlgRecordName[35];

/* The sound dialog: */
static SGOBJ optiondlg[] =
{
	{ SGBOX,      0,0,  0, 0, 40,26, NULL }, //0
	{ SGBOX,      0,0,  1, 1, 38,18, NULL },
	{ SGTEXT,     0,0,  4, 2, 13,1, "OPTION" },
	{ SGCHECKBOX, 0,0,  2, 4, 10,1, "MOUSE" },//3
	{ SGCHECKBOX, 0,0,  2, 5, 10,1, "MOUSEPORT 2" },//4
	{ SGCHECKBOX, 0,0,  2, 9, 10,1, "StatusBar" }, //
	{ SGBOX,      0,0,  1,18, 38,7, NULL }, //6
	{ SGBUTTON, SG_EXIT/*SG_DEFAULT*/, 0, 10,23, 24,1, "Back to main menu" },//7
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show and process the sound dialog.
 */
void Dialog_OptionDlg(void)
{
	int but, i;
	static int mouseport=1;

	SDLGui_CenterDlg(optiondlg);

	optiondlg[DLGOPTION_Mouse].state&= ~SG_SELECTED;
	optiondlg[DLGOPTION_STATUS].state&= ~SG_SELECTED;
	optiondlg[DLGOPTION_MousePort].state&= ~SG_SELECTED;

	//resources_get_int( "Mouse",&c64mouse_enable);
	c64mouse_enable=0;

	if (c64mouse_enable) { // Mouse on
		optiondlg[DLGOPTION_Mouse].state |= SG_SELECTED;
	}

	//resources_get_int( "Mouseport",&mouseport);
	mouseport=0;
	if(mouseport==2)optiondlg[DLGOPTION_MousePort].state |= SG_SELECTED;
 
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
		
		but = SDLGui_DoDialog(optiondlg, NULL);

                gui_poll_events();
	}
	while (but != DLGOPTION_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram );
/*
	if(optiondlg[DLGOPTION_Mouse].state & SG_SELECTED){

		if(!c64mouse_enable){
			resources_set_int( "Mouse",1);
		}

	}
	else {

		if(c64mouse_enable){
			resources_set_int( "Mouse",0);
		}

	}

	if(optiondlg[DLGOPTION_MousePort].state & SG_SELECTED){

		if(mouseport==1){
			resources_set_int( "Mouseport",2);
		}

	}
	else {

		if(mouseport==2){
			resources_set_int( "Mouseport",1);
		}

	}
*/
/*
	if(optiondlg[DLGOPTION_STATUS].state & SG_SELECTED){

		if(vice_statusbar==0){
			vice_statusbar=1;
		}

	}
	else {

		if(vice_statusbar==1){
			vice_statusbar=0;
		}

	}
*/
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

