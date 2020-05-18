#ifndef GRAPH_H
#define GRAPH_H 1

#include "retroscreen.h"

extern void DrawFBoxBmp(unsigned  short  *buffer,int x,int y,int dx,int dy,unsigned  short color);

extern void DrawBoxBmp(unsigned  short  *buffer,int x,int y,int dx,int dy,unsigned  short  color);

extern void DrawHlineBmp(unsigned  short  *buffer,int x,int y,int dx,int dy,unsigned  short  color);

extern void DrawVlineBmp(unsigned  short *buffer,int x,int y,int dx,int dy,unsigned  short  color);

extern void DrawlineBmp(unsigned  short  *buffer,int x1,int y1,int x2,int y2,unsigned  short  color);

extern void DrawPointBmp(unsigned  short  *buffer,int x,int y,unsigned  short color);

extern void DrawCircle(unsigned short *buf,int x, int y, int radius,unsigned short rgba,int full);

extern void Draw_string(unsigned short *surf, signed short int x, signed short int y, const unsigned char *string, unsigned  short int maxstrlen, unsigned  short int xscale, unsigned  short int yscale,unsigned  short int fg, unsigned  short int bg);

extern void Draw_text(unsigned  short *buffer,int x,int y,unsigned  short  fgcol,unsigned  short int bgcol ,int scalex,int scaley , int max,char *string,...);


#endif

