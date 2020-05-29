 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Joystick emulation for Linux and BSD. They share too much code to
  * split this file.
  * 
  * Copyright 1997 Bernd Schmidt
  * Copyright 1998 Krister Walfridsson
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "joystick.h"

extern unsigned char MXjoy[2];
#include "core-log.h"

void read_joystick(int nr, unsigned int *dir, int *button)
{
int    up  = (MXjoy[nr]>>0)&1;
int  down  = (MXjoy[nr]>>1)&1;
int  left  = (MXjoy[nr]>>2)&1;   
int  right = (MXjoy[nr]>>3)&1;
int b1	   = (MXjoy[nr]>>4)&1;
int b2	   = (MXjoy[nr]>>5)&1;

*dir = 0;
*button = 0;

/*
*button |= (MXjoy[nr]>>4)&1;
*button |= ((MXjoy[nr]>>5)&1) << 1;
*/
//if(MXjoy[nr]!=0)LOGI("AV joy%d u:%d d:%d r:%d l:%d b1:%d b2:%d \n",up,down,right,left,b1,b2);

  // normal joystick movement
  if (left)
    up = !up;
    if (right)
      down = !down;
//  *dir = down | (right << 1) | (up << 8) | (left << 9);

unsigned int dir2 = down | (right << 1) | (up << 8) | (left << 9);
int but2= b1| b2<< 1;

*dir = dir2;
*button =but2;

//if(MXjoy[nr]!=0)LOGI("joy%d rt:%d db(%d,%d) %d %d\n",nr,MXjoy[nr],*dir,*button,dir2,but2);
//if(MXjoy[nr]!=0)LOGI("AP joy%d u:%d d:%d r:%d l:%d b1:%d b2:%d \n",nr,up,down,right,left,b1,b2);
/*
if((nr == 0 && currprefs.pandora_joyPort == 1) || (nr == 1 && currprefs.pandora_joyPort == 0) )
    {
      *dir = 0;
      *button = 0;
    }
*/
/*
  #ifdef RASPBERRY
 // *button |= (SDL_JoystickGetButton(joy, 0)) & 1;
 // *button |= ((SDL_JoystickGetButton(joy, 1)) & 1) << 1;
  #endif

  // normal joystick movement
  if (left) 
    top = !top;
	if (right) 
	  bot = !bot;
  *dir = bot | (right << 1) | (top << 8) | (left << 9);

  if(currprefs.pandora_joyPort != 0)
  {
    // Only one joystick active    
    if((nr == 0 && currprefs.pandora_joyPort == 2) || (nr == 1 && currprefs.pandora_joyPort == 1))
    {
      *dir = 0;
      *button = 0;
    }
  }
*/

}

void handle_joymouse(void)
{
/*
	if (currprefs.pandora_custom_dpad == 1)
	{
    int mouseScale = currprefs.input_joymouse_multiplier / 2;

		if (buttonY) // slow mouse active
			mouseScale = currprefs.input_joymouse_multiplier / 10;

		if (dpadLeft)
		{
			lastmx -= mouseScale;
			newmousecounters=1;
		}
		if (dpadRight)
		{
			lastmx += mouseScale;
			newmousecounters=1;
		}
		if (dpadUp)
		{    
			lastmy -= mouseScale;
			newmousecounters=1;
		}
		if (dpadDown)
		{
			lastmy += mouseScale;
			newmousecounters=1;
		}
	}
*/
}

void init_joystick(void)
{
   
}

void close_joystick(void)
{

}
