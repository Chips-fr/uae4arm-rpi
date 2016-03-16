/*
	modded for libretro-frodo
*/

/*
  Hatari - dlgMisc.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Show information about the program and its license.
*/
const char DlgMisc_fileid[] = "Hatari dlgMisc.c : " __DATE__ " " __TIME__;

#include "dialog.h"
#include "sdlgui.h"

#define DLGMISC_LIMSPEED    2
#define DLGMISC_FASTRESET   3
#define DLGMISC_CIAHACK     4
#define DLGMISC_MAPSLSH     5

int DLGMISC_INC[4]={7,11,15,19};
int DLGMISC_TEXT[4]={8,12,16,20};
int DLGMISC_DEC[4]={9,13,17,21};

#define DLGMISC_REUNONE       23
#define DLGMISC_REU128K       24
#define DLGMISC_REU256K       25
#define DLGMISC_REU512K       26
#define DLGMISC_EXIT       28


char mcycle[4][4];
int valcycle[4];

/* The "Misc"-dialog: */
static SGOBJ miscdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 40,25, NULL },
	{ SGTEXT, 0, 0, 13,1, 12,1, NULL },

	{ SGCHECKBOX, 0, 0, 10,3, 12,1, "Limit speeds" },
	{ SGCHECKBOX, 0, 0, 10,5, 11,1, "Fast Reset" },
	{ SGCHECKBOX, 0, 0, 10,7, 13,1, "CIA IRQ Hack" },
	{ SGCHECKBOX, 0, 0, 10,9, 13,1, "Map /" },


	{ SGTEXT, 0, 0, 1,7+4, 38,1, "Nrm Cycle" }, //6
	{ SGBUTTON,   SG_EXIT, 0, 15,7+4,  1,1, "+" },
	{ SGTEXT,     0, 0, 17,7+4,  3,1,mcycle[0] },
	{ SGBUTTON,   SG_EXIT, 0, 23,7+4,  1,1,  "-" },

	{ SGTEXT, 0, 0, 1,9+4, 38,1, "Bad Line" }, //10
	{ SGBUTTON,   SG_EXIT, 0, 15,9+4,  1,1, "+" },
	{ SGTEXT,     0, 0, 17,9+4,  3,1,mcycle[1] },
	{ SGBUTTON,   SG_EXIT, 0, 23,9+4,  1,1,  "-" },

	{ SGTEXT, 0, 0, 1,11+4, 38,1, "CIA cycle" },//14
	{ SGBUTTON,   SG_EXIT, 0, 15,11+4,  1,1, "+" },
	{ SGTEXT,     0, 0, 17,11+4,  3,1,mcycle[2] },
	{ SGBUTTON,   SG_EXIT, 0, 23,11+4,  1,1,  "-" },

	{ SGTEXT, 0, 0, 1,13+4, 38,1, "Flp Cycle" },//18
	{ SGBUTTON,   SG_EXIT, 0, 15,13+4,  1,1, "+" },
	{ SGTEXT,     0, 0, 17,13+4,  3,1,mcycle[3] },
	{ SGBUTTON,   SG_EXIT, 0, 23,13+4,  1,1,  "-" },

	{ SGTEXT,     0,0,  1, 19, 14,1, "REU Size:" }, //22
	{ SGRADIOBUT, 0,0,  2+0, 21, 5,1, "None" },
	{ SGRADIOBUT, 0,0,  2+10, 21, 5,1, "128k" },
	{ SGRADIOBUT, 0,0,  2+20, 21, 5,1, "256k" },
	{ SGRADIOBUT, 0,0,  2+30, 21, 5,1, "512k" },

	{ SGTEXT, 0, 0, 1,22, 38,1, NULL },
	{ SGBUTTON, SG_EXIT, 0, 16,23, 8,1, "OK" }, //28
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

/*-----------------------------------------------------------------------*/
/**
 * Show the "about" dialog:
 */
void Dialog_MiscDlg(void)
{ 
        int i,but;

	SDLGui_CenterDlg(miscdlg);

	miscdlg[DLGMISC_LIMSPEED].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_FASTRESET].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_CIAHACK].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_MAPSLSH].state &= ~SG_SELECTED;

	miscdlg[DLGMISC_REUNONE].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_REU128K].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_REU256K].state &= ~SG_SELECTED;
	miscdlg[DLGMISC_REU512K].state &= ~SG_SELECTED;
/*
	switch (ThePrefs.REUSize)
	{
		case REU_NONE:
			miscdlg[DLGMISC_REUNONE].state |= SG_SELECTED;	
			break;
		case REU_128K:
			miscdlg[DLGMISC_REU128K].state |= SG_SELECTED;
			break;
		case REU_256K:
			miscdlg[DLGMISC_REU256K].state |= SG_SELECTED;
			break;
		case REU_512K:
			miscdlg[DLGMISC_REU512K].state |= SG_SELECTED;
			break;		
	}

	if (ThePrefs.LimitSpeed) { 
		miscdlg[DLGMISC_LIMSPEED].state |= SG_SELECTED;
	}
	if (ThePrefs.FastReset) { 
		miscdlg[DLGMISC_FASTRESET].state |= SG_SELECTED;
	}
	if (ThePrefs.CIAIRQHack) { 
		miscdlg[DLGMISC_CIAHACK].state |= SG_SELECTED;
	}
	if (ThePrefs.MapSlash) { 
		miscdlg[DLGMISC_MAPSLSH].state |= SG_SELECTED;
	}

	sprintf(mcycle[0], "%3i", ThePrefs.NormalCycles);
	valcycle[0]=ThePrefs.NormalCycles;
	sprintf(mcycle[1], "%3i", ThePrefs.BadLineCycles);
	valcycle[1]=ThePrefs.BadLineCycles;
	sprintf(mcycle[2], "%3i", ThePrefs.CIACycles);
	valcycle[2]=ThePrefs.CIACycles;
	sprintf(mcycle[3], "%3i", ThePrefs.FloppyCycles);
	valcycle[3]=ThePrefs.FloppyCycles;
*/
    do
	{
        but=SDLGui_DoDialog(miscdlg, NULL);

	 	for(i=0;i<4;i++){
			if(but==DLGMISC_INC[i]){
				valcycle[i]++;//if(valcycle[i]>100)valcycle[i]=100;
				sprintf(mcycle[i], "%3i", valcycle[i]);
			}
			else if(but== DLGMISC_DEC[i]){
				valcycle[i]--;//if(valcycle[i]<1)valcycle[i]=1;
				sprintf(mcycle[i], "%3i", valcycle[i]);
			}
		}

        gui_poll_events();

     }
     while (but != DLGMISC_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
/*
	if(miscdlg[DLGMISC_LIMSPEED].state & SG_SELECTED){
		if(!ThePrefs.LimitSpeed)
			prefs->	LimitSpeed=true;
	}		
	else if(ThePrefs.LimitSpeed)
			prefs->	LimitSpeed=false;	
	
	if(miscdlg[DLGMISC_FASTRESET].state & SG_SELECTED){
		if(!ThePrefs.FastReset)
			prefs->FastReset	=true;	
	}
	else if(ThePrefs.FastReset)
			prefs->FastReset	=false;
	
	if(miscdlg[DLGMISC_CIAHACK].state & SG_SELECTED){
		if(!ThePrefs.CIAIRQHack)
			prefs->CIAIRQHack	=true;
	}
	else if(ThePrefs.CIAIRQHack)
			prefs->CIAIRQHack	=false;

	if(miscdlg[DLGMISC_MAPSLSH].state & SG_SELECTED){
		if(!ThePrefs.MapSlash)		
			prefs->MapSlash	=true;
	}
	else if(ThePrefs.MapSlash)
			prefs->MapSlash	=false;

	prefs->NormalCycles=valcycle[0];
	prefs->BadLineCycles=valcycle[1];
	prefs->CIACycles=valcycle[2];
	prefs->FloppyCycles=valcycle[3];

	for(i=0;i<4;i++)
		if(miscdlg[DLGMISC_REUNONE+i].state & SG_SELECTED)
			prefs->REUSize=i;
*/
}
