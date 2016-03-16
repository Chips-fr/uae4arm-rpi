/*
	modded for libretro-frodo
*/

/*
  Hatari - dlgVideo.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Show information about the program and its license.
*/
const char DlgVideo_fileid[] = "Hatari dlgVideo.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"

#define DLGVIDEO_SPRITES	3
#define DLGVIDEO_SPRITECOL	4
#define DLGVIDEO_INC	    6
#define DLGVIDEO_SKIPFRAME	7
#define DLGVIDEO_DEC	    8
#define DLGVIDEO_LEDS	    9
#define DLGVIDEO_EXIT       12

char mskip[4];
int valskip;

/* The "Video"-dialog: */
static SGOBJ videodlg[] =
{
	{ SGBOX, 0, 0, 0,0, 40,25, NULL },				//0
	{ SGTEXT, 0, 0, 13,1, 12,1, NULL },				//1

	{ SGTEXT, 0, 0, 13,2, 22,1, "Sprites & Collisions:" },			//2
	{ SGCHECKBOX, 0, 0, 10   ,4, 11,1, "Sprites" },			//3
	{ SGCHECKBOX, 0, 0, 10   ,5, 12,1, "Collisions" },		//4

	{ SGTEXT, 0, 0, 13,8, 20,1, "Skip Frames:"},		//5
	{ SGBUTTON,   SG_EXIT, 0, 15,10,  1,1, "+" },
	{ SGTEXT,     0, 0, 17,10,  3,1,mskip },
	{ SGBUTTON,   SG_EXIT, 0, 23,10,  1,1,  "-" },

	{ SGCHECKBOX, 0, 0, 13 ,16, 11,1, "Show Leds" },			//9
	//{ SGTEXT, 0, 0, 1,16, 38,1, NULL },				//9
	{ SGTEXT, 0, 0, 1,17, 38,1, NULL },				
	{ SGTEXT, 0, 0, 1,18, 38,1, NULL },				
	{ SGBUTTON, SG_EXIT/*SG_DEFAULT*/, 0, 19,23, 8,1, "OK" },	//12
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show the "about" dialog:
 */
void Dialog_VideoDlg(void)
{ 
    int but;

	SDLGui_CenterDlg(videodlg);

	videodlg[DLGVIDEO_LEDS].state &= ~SG_SELECTED;
/*
	if (ThePrefs.ShowLEDs) { 
		videodlg[DLGVIDEO_LEDS].state |= SG_SELECTED;
	}


	videodlg[DLGVIDEO_SPRITES].state &= ~SG_SELECTED;
	videodlg[DLGVIDEO_SPRITECOL].state &= ~SG_SELECTED;

	if (ThePrefs.SpritesOn) { 
		videodlg[DLGVIDEO_SPRITES].state |= SG_SELECTED;
	}

	if (ThePrefs.SpriteCollisions) { 
		videodlg[DLGVIDEO_SPRITECOL].state |= SG_SELECTED;
	}


	sprintf(mskip, "%3i", ThePrefs.SkipFrames);
	valskip=ThePrefs.SkipFrames;
*/
    do
	{
                but=SDLGui_DoDialog(videodlg, NULL);

			if(but==DLGVIDEO_INC){
				valskip++;if(valskip>10)valskip=10;
				sprintf(mskip, "%3i", valskip);
			}
			else if(but== DLGVIDEO_DEC){
				valskip--;if(valskip<1)valskip=1;
				sprintf(mskip, "%3i", valskip);
			}

                gui_poll_events();

    }
    while (but != DLGVIDEO_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
/*
	prefs->SkipFrames=valskip;

	if(videodlg[DLGVIDEO_SPRITECOL].state & SG_SELECTED){

		if(!ThePrefs.SpriteCollisions){
			prefs->	SpriteCollisions=true;
		}

	}
	else {

		if(ThePrefs.SpriteCollisions){
			prefs->	SpriteCollisions=false;
		}

	}

	if(videodlg[DLGVIDEO_SPRITES].state & SG_SELECTED){

		if(!ThePrefs.SpritesOn){
			prefs->	SpritesOn=true;
		}

	}
	else {

		if(ThePrefs.SpritesOn){
			prefs->	SpritesOn=false;
		}

	}

	if(videodlg[DLGVIDEO_LEDS].state & SG_SELECTED){

		if(!ThePrefs.ShowLEDs){
			prefs->ShowLEDs=true;
		}

	}
	else {

		if(ThePrefs.ShowLEDs){
			prefs->ShowLEDs=false;
		}

	}
*/
}
