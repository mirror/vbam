// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004-2008 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef VBA_SDL_FILTERS_H
#define VBA_SDL_FILTERS_H

#include "../System.h"
#include "../filters/filters.hpp"
#include "../filters/interframe.hpp"

      //
      // Screen filters
      //

// List of available filters
enum Filter { kStretch1x, kStretch2x, k2xSaI, kSuper2xSaI, kSuperEagle, kPixelate,
				kAdMame2x, kBilinear, kBilinearPlus, kScanlines, kScanlinesTV,
				klq2x, khq2x, xbrz2x, kStretch3x, khq3x, xbrz3x, kStretch4x, khq4x, xbrz4x, xbrz5x, kInvalidFilter };

// Initialize a filter and get the corresponding filter function pointer
FilterFunc initFilter(const Filter f, const int colorDepth, const int srcWidth);

// Get the enlarge factor of a filter
int getFilterEnlargeFactor(const Filter f);

// Get the display name for a filter
char* getFilterName(const Filter f);

      //
      // Interframe filters
      //

// List of available IFB filters
enum IFBFilter { kIFBNone, kIBMotionBlur, kIBSmart, kInvalidIFBFilter };

// Function pointer type for an IFB filter function
typedef void(*IFBFilterFunc)(u8*, u32, int, int);

// Initialize an IFB filter and get the corresponding filter function pointer
IFBFilterFunc initIFBFilter(const IFBFilter f, const int colorDepth);

// Get the display name for an IFB filter
char* getIFBFilterName(const IFBFilter f);

#endif // VBA_SDL_FILTERS_H
