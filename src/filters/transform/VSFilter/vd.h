//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2007 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  Notes:
//  - VDPixmapBlt is from VirtualDub
//  - sse2 yv12 to yuy2 conversion by Haali
//	(- vd.cpp/h should be renamed to something more sensible already :)

#pragma once

void VDCPUTest();

extern bool BitBltYUV420P(int w, int h, BYTE* dsty, BYTE* dstu, BYTE* dstv, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch);
extern bool BitBltYUV420PtoNV12(int w, int h, BYTE* dsty, BYTE* dstu, BYTE* dstv, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch);
extern bool BitBltYUV420PtoYUY2(int w, int h, BYTE* dst, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch);
extern bool BitBltYUV420PtoYUY2Interlaced(int w, int h, BYTE* dst, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch);
extern bool BitBltYUV420PtoRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch /* TODO: , bool fInterlaced = false */);

extern bool BitBltNV12orP01x(int w, int h, BYTE* dsty, BYTE* dstuv, int dstpitch, BYTE* srcy, BYTE* srcuv, int srcpitch);

extern bool BitBltYUY2(int w, int h, BYTE* dst, int dstpitch, BYTE* src, int srcpitch);
extern bool BitBltYUY2toRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* src, int srcpitch);

extern bool BitBltRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* src, int srcpitch, int sbpp);
extern bool BitBltRGBStretch(int dstw, int dsth, BYTE* dst, int dstpitch, int dbpp, int srcw, int srch, BYTE* src, int srcpitch, int sbpp);
