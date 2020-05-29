#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "graph.h"
#include "libretro-core.h"
#include <SDL.h>

#define  Uint8 unsigned char

typedef struct{
     signed short int x, y;
     unsigned short int w, h;
} SDL_Rect;


void DrawFBoxBmp(char  *buffer,int x,int y,int dx,int dy,unsigned   color){
	
	int i,j,idx;
		
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif

	for(i=x;i<x+dx;i++){
		for(j=y;j<y+dy;j++){
			
			idx=i+j*VIRTUAL_WIDTH;
			mbuffer[idx]=color;	
		}
	}
	
}

void DrawBoxBmp(char  *buffer,int x,int y,int dx,int dy,unsigned    color){
	
	int i,j,idx;
	
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif
	
	for(i=x;i<x+dx;i++){
		idx=i+y*VIRTUAL_WIDTH;
		mbuffer[idx]=color;
		idx=i+(y+dy)*VIRTUAL_WIDTH;
		mbuffer[idx]=color;
	}

	for(j=y;j<y+dy;j++){
			
		idx=x+j*VIRTUAL_WIDTH;
		mbuffer[idx]=color;	
		idx=(x+dx)+j*VIRTUAL_WIDTH;
		mbuffer[idx]=color;	
	}
	
}

void DrawPointBmp(char  *buffer,int x,int y,unsigned   color){
	
	int idx;
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif

		idx=x+y*VIRTUAL_WIDTH;
		mbuffer[idx]=color;		
	
}

void DrawHlineBmp(char  *buffer,int x,int y,int dx,int dy,unsigned    color){
	
	int i,j,idx;
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif
		
	for(i=x;i<x+dx;i++){
		idx=i+y*VIRTUAL_WIDTH;
		mbuffer[idx]=color;		
	}
}

void DrawVlineBmp(char *buffer,int x,int y,int dx,int dy,unsigned    color){
	
	int i,j,idx;
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif
	for(j=y;j<y+dy;j++){			
		idx=x+j*VIRTUAL_WIDTH;
		mbuffer[idx]=color;		
	}	
}

void DrawlineBmp(char  *buffer,int x1,int y1,int x2,int y2,unsigned    color){
		
	int pixx, pixy;
 	int x, y;
 	int dx, dy; 	
 	int sx, sy;
 	int swaptmp;
 	int idx;
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buffer;
#else
unsigned short *mbuffer=(unsigned short *)buffer;
#endif

	dx = x2 - x1;
	dy = y2 - y1;
	sx = (dx >= 0) ? 1 : -1;
	sy = (dy >= 0) ? 1 : -1;

	if (dx==0) {
 		if (dy>0) {
 			DrawVlineBmp(buffer, x1, y1,0, dy, color);
			return;

 		} else if (dy<0) {
 			DrawVlineBmp(buffer, x1, y2,0, -dy, color);
			return;

 		} else {
			idx=x1+y1*VIRTUAL_WIDTH;
 			mbuffer[idx]=color;
			return ;
 		}
 	}
 	if (dy == 0) {
 		if (dx>0) {
 			DrawHlineBmp(buffer, x1, y1, dx, 0, color);
			return;

 		} else if (dx<0) {
 			DrawHlineBmp(buffer, x2, y1, -dx,0, color);
			return;
 		}
 	}

	dx = sx * dx + 1;
 	dy = sy * dy + 1;
	
	pixx = 1;
 	pixy = VIRTUAL_WIDTH;

 	pixx *= sx;
 	pixy *= sy;

 	if (dx < dy) {
	 	swaptmp = dx;
	 	dx = dy;
	 	dy = swaptmp;
	 	swaptmp = pixx;
	 	pixx = pixy;
	 	pixy = swaptmp;
 	}

	x = 0;
 	y = 0;

	idx=x1+y1*VIRTUAL_WIDTH;

	for (; x < dx; x++, idx +=pixx) {
		mbuffer[idx]=color;
 		y += dy;
 		if (y >= dx) {
 			y -= dx;
 			idx += pixy;
 		}
	}

}
/*
void DrawBox(char   *buffer,box b,char t[],unsigned    color){


	DrawBoxBmp(mbuffer,b.x,b.y,b.dx,b.dy,color); 	
	textCpixel(mbuffer,b.x, 3*b.x + b.dx ,b.y+2,color,1,1,4,"%s",t);

}

void DrawBoxF(char    *buffer,box b,char t[],unsigned   color,unsigned    border){

	int ydec=b.y+(b.dy/2)-4;


	if(ydec<b.y+2)ydec=b.y+2;

	DrawBoxBmp(mbuffer,b.x,b.y,b.dx,b.dy,border);
	DrawFBoxBmp(mbuffer,b.x+1,b.y+1,b.dx-2,b.dy-2,color);
 	
	textCpixel(mbuffer,b.x, 3*b.x + b.dx , ydec ,border,1,1,4,"%s",t);

}
*/

const float DEG2RAD = 3.14159/180;

void DrawCircle(char *buf,int x, int y, int radius,unsigned  rgba,int full)
{ 
	int i;
	float degInRad; 
	int x1,y1;
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)buf;
#else
unsigned short *mbuffer=(unsigned short *)buf;
#endif

   	for ( i=0; i < 360; i++){

		degInRad = i*DEG2RAD;
   		x1=x+cos(degInRad)*radius;
		y1=y+sin(degInRad)*radius;

		if(full)DrawlineBmp(buf,x,y, x1,y1,rgba); 
		else {
			mbuffer[x1+y1*VIRTUAL_WIDTH]=rgba;
		}
     		
     	}
    	
}
#ifdef FILTER16X2
//UINT16
void filter_scale2x(unsigned char *srcPtr, unsigned srcPitch, 
                      unsigned char *dstPtr, unsigned dstPitch,
		      int width, int height)
{
	unsigned int nextlineSrc = srcPitch / sizeof(short);
	short *p = (short *)srcPtr;

	unsigned int nextlineDst = dstPitch / sizeof(short);
	short *q = (short *)dstPtr;
  
	while(height--) {
		int i = 0, j = 0;
		for(i = 0; i < width; ++i, j += 2) {
			short B = *(p + i - nextlineSrc);
			short D = *(p + i - 1);
			short E = *(p + i);
			short F = *(p + i + 1);
			short H = *(p + i + nextlineSrc);

			*(q + j) = D == B && B != F && D != H ? D : E;
			*(q + j + 1) = B == F && B != D && F != H ? F : E;
			*(q + j + nextlineDst) = D == H && D != B && H != F ? D : E;
			*(q + j + nextlineDst + 1) = H == F && D != H && B != F ? F : E;
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}
}
#endif

#include "font2.i"


void Retro_Draw_string(char *surf, signed short int x, signed short int y, const  char *string,unsigned short maxstrlen,unsigned short xscale, unsigned short yscale, unsigned  fg, unsigned  bg)
{
    	int k,strlen;
    	unsigned char *linesurf;
    	signed  int ypixel;
    	int col, bit;
    	unsigned char b;

    	int xrepeat, yrepeat;

#if defined PITCH && PITCH == 4
   	unsigned  *yptr; 
#elif defined PITCH && PITCH == 2
	unsigned short *yptr; 
#elif defined PITCH && PITCH == 1
	unsigned char *yptr; 
#endif
			
#if defined PITCH && PITCH == 4
unsigned *mbuffer=(unsigned*)surf;
#elif defined PITCH && PITCH == 2
unsigned short *mbuffer=(unsigned short *)surf;
#elif defined PITCH && PITCH == 1
unsigned char *mbuffer=(unsigned char *)surf;
#endif

    	if(string==NULL)return;
    	for(strlen = 0; strlen<maxstrlen && string[strlen]; strlen++) {}

	int surfw=strlen * 7 * xscale;
	int surfh=8 * yscale;

#if defined PITCH && PITCH == 4	
        linesurf =(unsigned char *)malloc(sizeof(unsigned )*surfw*surfh );
    	yptr = (unsigned *)&linesurf[0];
#elif defined PITCH && PITCH == 2
        linesurf =(unsigned char *)malloc(sizeof(unsigned short)*surfw*surfh );
    	yptr = (unsigned short *)&linesurf[0];
#elif defined PITCH && PITCH == 1
        linesurf =(unsigned char *)malloc(sizeof(unsigned char)*surfw*surfh );
    	yptr = (unsigned char *)&linesurf[0];
#endif

	for(ypixel = 0; ypixel<8; ypixel++) {

        	for(col=0; col<strlen; col++) {

            		b = font_array[(string[col]^0x80)*8 + ypixel];

            		for(bit=0; bit<7; bit++, yptr++) {              
				*yptr = (b & (1<<(7-bit))) ? fg : bg;
                		for(xrepeat = 1; xrepeat < xscale; xrepeat++, yptr++)
                    			yptr[1] = *yptr;
                        }
        	}

        	for(yrepeat = 1; yrepeat < yscale; yrepeat++) 
            		for(xrepeat = 0; xrepeat<surfw; xrepeat++, yptr++)
                		*yptr = yptr[-surfw];
           
    	}

#if defined PITCH && PITCH == 4	
    	yptr = (unsigned *)&linesurf[0];
#elif defined PITCH && PITCH == 2
    	yptr = (unsigned short*)&linesurf[0];
#elif defined PITCH && PITCH == 1
	yptr = (unsigned char*)&linesurf[0];
#endif

    	for(yrepeat = y; yrepeat < y+ surfh; yrepeat++) 
        	for(xrepeat = x; xrepeat< x+surfw; xrepeat++,yptr++)
             		if(*yptr!=0)mbuffer[xrepeat+yrepeat*VIRTUAL_WIDTH] = *yptr;

	free(linesurf);

}


void Draw_text(char *buffer,int x,int y,unsigned    fgcol,unsigned   int bgcol ,int scalex,int scaley , int max,const  char *string,...)
{
	int boucle=0;  
   	char text[256];	   	
   	va_list	ap;	

   	if (string == NULL)return;		
		
   	va_start(ap, string);		
      		vsprintf(text, string, ap);	
   	va_end(ap);	

   	Retro_Draw_string(buffer, x,y, (const  char *)text,max, scalex, scaley,fgcol,bgcol);	
}




#include "font5x8.h"
#include "font10x16.h"

#define UNDERLINE_INDICATOR '_'
#define sm_fontwidth  5
#define	sm_fontheight 8

unsigned char smfont[font5x8_height*font5x8_width];
unsigned char mfont[font10x16_height*font10x16_width];
int gui_fontwidth;
int gui_fontheight;

void initsmfont(){

	int h=font5x8_height;
	int w=font5x8_width;

	unsigned char *dstbits;
	const unsigned char *srcbits;
	int x, y, srcpitch;
	int mask;

	srcbits = font5x8_bits;

	srcpitch = ((w + 7) / 8);
	dstbits = (unsigned char *)smfont;
	mask = 1;

	for (y = 0 ; y < h ; y++)
	{
		for (x = 0 ; x < w ; x++)
		{
			dstbits[x] = (srcbits[x / 8] & mask) ? 1 : 0;
			mask <<= 1;
			mask |= (mask >> 8);
			mask &= 0xFF;
		}
		dstbits += font5x8_width;
		srcbits += srcpitch;
	}

}

void initmfont(){

	int h=font10x16_height;
	int w=font10x16_width;

	unsigned char *dstbits;
	const unsigned char *srcbits;
	int x, y, srcpitch;
	int mask;

	srcbits = font10x16_bits;

	srcpitch = ((w + 7) / 8);
	dstbits = (unsigned char *)mfont;
	mask = 1;

	/* Copy the pixels */
	for (y = 0 ; y < h ; y++)
	{
		for (x = 0 ; x < w ; x++)
		{
			dstbits[x] = (srcbits[x / 8] & mask) ? 1 : 0;
			mask <<= 1;
			mask |= (mask >> 8);
			mask &= 0xFF;
		}
		dstbits += font10x16_width;
		srcbits += srcpitch;
	}

}

void draw_guifont(unsigned short *buffer,int x,int y,int sx,int sy,unsigned short col,unsigned short bg) {

	int i,j,xx;
	int si,sj;
	int dx,dy;

	dx=gui_fontwidth;
	dy=gui_fontheight;

	for(j=y,sj=sy;j<y+dy;j++,sj++){
		
		for(i=x,si=sx;i<x+dx;i++,si++){

			if(bg){
				DrawPointBmp((char*)buffer,i,j, bg);
			}

			
			if(gui_fontwidth==5){
				if(smfont[(sj)*font5x8_width+(si)]==1)DrawPointBmp((char*)buffer,i,j, col);
			}
			else if(gui_fontwidth==10){
				if(mfont[(sj)*font10x16_width+(si)]==1)DrawPointBmp((char*)buffer,i,j, col);
			}

			
					
		}
		
	}
}

static void Gui_TextInt(unsigned short *buffer,int x, int y, const char *txt, int underline,unsigned short col,unsigned short bg)
{
	int i, offset;
	unsigned char c;
	SDL_Rect sr, dr;

	/* underline offset needs to go outside the box for smaller font */
	if (gui_fontheight < 16)
		offset = gui_fontheight - 1;
	else
		offset = gui_fontheight - 2;

	i = 0;
	while (txt[i])
	{
		dr.x=x;
		dr.y=y;
		dr.w=gui_fontwidth;
		dr.h=gui_fontheight;

		c = txt[i++];
/*
		if (c == UNDERLINE_INDICATOR && underline)
		{
			dr.h = 1;
			dr.y += offset;
			SDL_FillRect(pSdlGuiScrn, &dr, colors.underline);
			continue;
		}
*/
		/* for now, assume (only) Linux file paths are UTF-8 */
#if !(defined(WIN32) || defined(USE_LOCALE_CHARSET))
		/* Quick and dirty conversion for latin1 characters only... */
		if ((c & 0xc0) == 0xc0)
		{
			c = c << 6;
			c |= (txt[i++]) & 0x7f;
		}
		else if (c >= 0x80)
		{
		//	Log_Printf(LOG_WARN, "Unsupported character '%c' (0x%x)\n", c, c);
		}
#endif
		x += gui_fontwidth;

		sr.x=gui_fontwidth*(c%16);
		sr.y=gui_fontheight*(c/16);
		sr.w=gui_fontwidth;
		sr.h=gui_fontheight;
		//SDL_BlitSurface(pFontGfx, &sr, pSdlGuiScrn, &dr);
		draw_guifont(buffer,dr.x,dr.y,sr.x,sr.y,col,bg);
	}

}

void Gui_Text(unsigned short *buffer,int x, int y, const char *txt,unsigned short col,unsigned short bg,int fscale)
{
	if(fscale>2)fscale=2;
	if(fscale<1)fscale=1;

	gui_fontwidth=sm_fontwidth*fscale;
	gui_fontheight=sm_fontheight*fscale;

	Gui_TextInt(buffer,x, y, txt, 0,col,bg);
}

int _filledRectAlpha16(unsigned short * dst, int x1, int y1, int x2, int y2, unsigned int color, unsigned char alpha)
{
	unsigned int Rmask, Gmask, Bmask, Amask;
	unsigned int Rshift, Gshift, Bshift, Ashift;
	unsigned int sR, sG, sB, sA;
	unsigned int dR, dG, dB, dA;
	int x, y;

	if (dst == NULL) {
		return (-1);
	}

   	Rmask=0x0000F800;
   	Gmask=0x000007E0;
   	Bmask=0x0000001F;
   	Amask=0x00000000;

//printf("(%x,%x,%x,%x)%x\n",(color & Rmask),(color & Gmask),(color & Bmask),(color & Amask),alpha);


		{			/* 16-bpp */
			unsigned short *row, *pixel;
			unsigned short R, G, B, A;
			unsigned short dc;

			sR = (color & Rmask); 
			sG = (color & Gmask);
			sB = (color & Bmask);
			sA = (color & Amask);

			for (y = y1; y <= y2; y++) {
				row = (unsigned short *) dst + y * VIRTUAL_WIDTH*2 / 2;
				for (x = x1; x <= x2; x++) {
					if (alpha == 255) {
						*(row + x) = color;
					} else {
						pixel = row + x;
						dc = *pixel;

						dR = (dc & Rmask);
						dG = (dc & Gmask);
						dB = (dc & Bmask);

						R = (dR + ((sR - dR) * alpha >> 8)) & Rmask;
						G = (dG + ((sG - dG) * alpha >> 8)) & Gmask;
						B = (dB + ((sB - dB) * alpha >> 8)) & Bmask;
						*pixel = R | G | B;
						if (Amask!=0) {
							dA = (dc & Amask);
							A = (dA + ((sA - dA) * alpha >> 8)) & Amask;
							*pixel |= A;
						} 
					}
				}
			}
		}


	return (0);
}


void DrawFBoxBmpRGBA(unsigned short *dst,int x,int y,int dx,int dy,unsigned /*short*/ int color,unsigned char alpha){

        _filledRectAlpha16(dst,x,y,x+dx,y+dy, color, alpha);

}





