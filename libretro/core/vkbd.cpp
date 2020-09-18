#include "libretro-core.h"

#include "vkbd_def.h"
#include "graph.h"

extern int NPAGE;
extern int KCOL;
extern int BKGCOLOR;
extern int SHIFTON;

int kbd_alpha=1;

void virtual_kdb(char *pixels,int vx,int vy)
{
   int x, y, posx,posy,page;
   short unsigned *pix, coul;

   int scale;
   int XSIDE2;
   int YSIDE2;
   int xoff;
   int XBASE;
   int YBASE;

   scale = (retrow < 640) ? 1 : 2;
   xoff = (scale == 1) ? 1 : 2;

   XSIDE2 = (5*scale*3)+xoff;
   YSIDE2 = (8*scale)+2;

   XBASE = (retrow-NPLGN*XSIDE2)/2;  
   YBASE = retroh/2 + (retroh/2 -NLIGN*YSIDE2)/2;  

   page = (NPAGE == -1) ? 0 : NPLGN*NLIGN;
   pix= (short unsigned *)&pixels[0];

   coul = RGB565(7, 7, 7);

   if(kbd_alpha){
	DrawFBoxBmpRGBA(pix,XBASE,YBASE,-1+XSIDE2*NPLGN,YSIDE2*NLIGN,RGB565(29,29,29),128);
	BKGCOLOR = 0;
   }
   else   BKGCOLOR = (KCOL>0?RGB565(29,29,29):0);

   for(x=0;x<NPLGN;x++)
   {
      for(y=0;y<NLIGN;y++)
      {

	posx=XBASE+x*XSIDE2 ;
	posy=YBASE+YSIDE2*y ;


        if(MVk[(y*NPLGN)+x+page].box>1 && MVk[(y*NPLGN)+x+page].color!=1 )DrawFBoxBmpRGBA(pix,posx/*x*XSIDE2*/,posy/*YBASE+y*YSIDE2*/+1, -2+XSIDE2*MVk[(y*NPLGN)+x+page].box,YSIDE2-2,RGB565(22,20,18),128);
	else {
		if(MVk[(y*NPLGN)+x+page].color==2 && MVk[(y*NPLGN)+x+page].box!=0 )
			DrawFBoxBmpRGBA(pix,posx/*x*XSIDE2*/,posy/*YBASE+y*YSIDE2*/+1, -2+XSIDE2,YSIDE2-2,RGB565(22,20,18),128);
		else if(MVk[(y*NPLGN)+x+page].color==3 && MVk[(y*NPLGN)+x+page].box!=0 )
			DrawFBoxBmpRGBA(pix,posx/*x*XSIDE2*/,posy/*YBASE+y*YSIDE2*/+1, -2+XSIDE2,YSIDE2-2,RGB565(31,31,27),128);
	}

	Gui_Text(pix,posx, posy+1,SHIFTON==-1?MVk[(y*NPLGN)+x+page].norml:MVk[(y*NPLGN)+x+page].shift,coul,BKGCOLOR,scale);
	
      }
   }

   DrawBoxBmp((char*)pix,XBASE+vx*XSIDE2,YBASE+vy*YSIDE2+1,-2+ XSIDE2*MVk[(vy*NPLGN)+vx+page].box,YSIDE2-2, RGB565(2,31,1));


}

int check_vkey2(int x,int y)
{
   int page;
   //check which key is press
   page= (NPAGE==-1) ? 0 : NPLGN*NLIGN;
   return MVk[y*NPLGN+x+page].val;
}

