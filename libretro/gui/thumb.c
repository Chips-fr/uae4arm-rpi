/*
	modded for libretro-frodo
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "libretro-core.h"

#include "graph.h"

typedef unsigned  int  PIXEL;

#define AVERAGE(a, b)   ( (((a) & 0xfffefefe) + ((b) & 0xfffefefe)) >> 1 )

void ScaleLine(PIXEL *Target, PIXEL *Source, int SrcWidth, int TgtWidth)
{
   int NumPixels = TgtWidth;
   int IntPart = SrcWidth / TgtWidth;
   int FractPart = SrcWidth % TgtWidth;
   int E = 0;

   while (NumPixels-- > 0)
   {
      *Target++ = *Source;
      Source += IntPart;
      E += FractPart;
      if (E >= TgtWidth)
      {
         E -= TgtWidth;
         Source++;
      } /* if */
   } /* while */
}

void ScaleRect(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight,
      int TgtWidth, int TgtHeight)
{
   int NumPixels = TgtHeight;
   int IntPart = (SrcHeight / TgtHeight) * SrcWidth;
   int FractPart = SrcHeight % TgtHeight;
   int E = 0;
   PIXEL *PrevSource = NULL;

   while (NumPixels-- > 0)
   {
      if (Source == PrevSource)
         memcpy(Target, Target-TgtWidth, TgtWidth*sizeof(*Target));
      else
      {
         ScaleLine(Target, Source, SrcWidth, TgtWidth);
         PrevSource = Source;
      } /* if */
      Target += TgtWidth;
      Source += IntPart;
      E += FractPart;
      if (E >= TgtHeight)
      {
         E -= TgtHeight;
         Source += SrcWidth;
      } /* if */
   } /* while */
}

void dothumb(char *name,int sw,int sh,int tw,int th)
{
#if 0
   unsigned  int target[th*tw];
   unsigned  int buffer[2]={tw,th};

   char *savebmp=malloc(sizeof(Retro_Screen));
   memcpy(savebmp, Retro_Screen,sizeof(Retro_Screen));

   memset(target,0,4*tw*th);

   PIXEL *Source=(unsigned *)&savebmp[0];
   PIXEL *Target=&target[0];

   ScaleRect(Target, Source, sw, sh,tw,th);

   free(savebmp);

   FILE * pFile;
   pFile = fopen (name, "wb");
   if (!pFile)
      return;
   fwrite (buffer , 1,2*2    , pFile);
   fwrite (target , 1,tw*th*4, pFile);
   fclose (pFile);
#endif
}

void loadthumb(char *name,int x,int y)
{
#if 0
   unsigned int buffer[2];
   FILE * pFile;
   pFile = fopen (name, "rb");
   if (pFile==NULL)return;

   int i,j,idx;
   int dx,dy;

   i = fread (buffer,1,2*2,pFile);
   dx=buffer[0];
   dy=buffer[1];

   unsigned  int target[dx*dy];

   i = fread (target,1,dx*dy*4,pFile);
   fclose (pFile);

   idx=0;
   for(j=y;j<y+dy;j++)
   {
      for(i=x;i<x+dx;i++)
      {
         DrawPointBmp((char *)Retro_Screen,i,j,target[idx]);
         idx++;			
      }
   }
#endif
}


void ScaleMinifyByTwo(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight)
{
   int x, y, x2, y2;
   int TgtWidth, TgtHeight;
   PIXEL p, q;

   TgtWidth = SrcWidth / 2;
   TgtHeight = SrcHeight / 2;
   for (y = 0; y < TgtHeight; y++)
   {
      y2 = 2 * y;
      for (x = 0; x < TgtWidth; x++)
      {
         x2 = 2 * x;
         p = AVERAGE(Source[y2*SrcWidth + x2], Source[y2*SrcWidth + x2 + 1]);
         q = AVERAGE(Source[(y2+1)*SrcWidth + x2], Source[(y2+1)*SrcWidth + x2 + 1]);
         Target[y*TgtWidth + x] = AVERAGE(p, q);
      } /* for */
   } /* for */
}

