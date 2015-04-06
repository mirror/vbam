//Credits:
//  Kawaks' Mr. K for the code
//  Incorporated into vba by Anthony Di Franco
//  Converted to C++ by Arthur Moore
#include "../System.h"
#include <stdlib.h>
#include <memory.h>
#include <stdexcept>

#include "new_interframe.hpp"

void interframe_filter::setWidth(unsigned int _width)
{
    width = _width;

    //32 bit filter, so 4 bytes per pixel
    //  The +1 is for a 1 pixel border that the emulator spits out
    horiz_bytes = (width+1) * 4;
    //Unfortunately, the filter keeps the border on the scaled output, but DOES NOT scale it
//     horiz_bytes_out = width * 4 * myScale + 4;

}


SmartIB::SmartIB()
{
  frm1 = (u8 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u8 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u8 *)calloc(322*242,4);
  // 3 frames ago
}

SmartIB::~SmartIB()
{
  //\HACK to prevent double freeing.  (It looks like this is not being called in a thread safe manner!!!)

  if(frm1)
    free(frm1);
  if( frm2 && (frm1 != frm2) )
    free(frm2);
  if( frm3 && (frm1 != frm3) && (frm2 != frm3) )
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

void SmartIB::run(u8 *srcPtr, int starty, int height)
{
    //Actual width needs to take into account the +1 border
    unsigned int width = getWidth() +1;

    u32 *src0 = (u32 *)srcPtr + starty * width;
    u32 *src1 = (u32 *)frm1 + starty * width;
    u32 *src2 = (u32 *)frm2 + starty * width;
    u32 *src3 = (u32 *)frm3 + starty * width;

  u32 colorMask = 0xfefefe;

  int pos = 0;

  for (int j = 0; j < height;  j++)
    for (int i = 0; i < width; i++) {
      u32 color = src0[pos];
      src0[pos] =
        (src1[pos] != src2[pos]) &&
        (src3[pos] != color) &&
        ((color == src2[pos]) || (src1[pos] == src3[pos]))
        ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1)) :
        color;
      src3[pos] = color; /* oldest buffer now holds newest frame */
      pos++;
    }

  /* Swap buffers around */
  u8 *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;
}


MotionBlurIB::MotionBlurIB()
{
  frm1 = (u8 *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (u8 *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (u8 *)calloc(322*242,4);
  // 3 frames ago
}

MotionBlurIB::~MotionBlurIB()
{
  //\HACK to prevent double freeing.  (It looks like this is not being called in a thread safe manner!!!)

  if(frm1)
    free(frm1);
  if( frm2 && (frm1 != frm2) )
    free(frm2);
  if( frm3 && (frm1 != frm3) && (frm2 != frm3) )
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

void MotionBlurIB::run(u8 *srcPtr, int starty, int height)
{
    //Actual width needs to take into account the +1 border
    unsigned int width = getWidth() +1;

    u32 *src0 = (u32 *)srcPtr + starty * width;
    u32 *src1 = (u32 *)frm1 + starty * width;

    u32 colorMask = 0xfefefe;

    int pos = 0;

    for (int j = 0; j < height;  j++)
        for (int i = 0; i < width; i++) {
            u32 color = src0[pos];
            src0[pos] = (((color & colorMask) >> 1) +
                   ((src1[pos] & colorMask) >> 1));
            src1[pos] = color;
            pos++;
        }
}
