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


#include "stdafx.h"
#include "vd.h"
#include "vd_asm.h"
#include <intrin.h>

#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>
#include <vd2/system/vdstl.h>
//#include <vd2/system/math.h>

#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include "vd2/Kasumi/resample.h"

#include "DSUtil/PixelUtils.h"

void VDCPUTest() {
	SYSTEM_INFO si;

	long lEnableFlags = CPUCheckForExtensions();

	GetSystemInfo(&si);

	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		if (si.wProcessorLevel < 4)
			lEnableFlags &= ~CPUF_SUPPORTS_FPU;		// Not strictly true, but very slow anyway

	// Enable FPU support...

	CPUEnableExtensions(lEnableFlags);

	VDFastMemcpyAutodetect();
}

bool BitBltYUV420P(int w, int h, BYTE* dsty, BYTE* dstu, BYTE* dstv, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch)
{
	CopyPlane(h, dsty, dstpitch, srcy, srcpitch);

	h /= 2;
	dstpitch /= 2;
	srcpitch /= 2;

	CopyPlane(h, dstu, dstpitch, srcu, srcpitch);
	CopyPlane(h, dstv, dstpitch, srcv, srcpitch);

	return true;
}

bool BitBltYUV420PtoNV12(int w, int h, BYTE* dsty, BYTE* dstu, BYTE* dstv, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch)
{
	BYTE* src[3] = { srcy, srcu, srcv };
	CopyI420toNV12(w, h, dsty, dstpitch, src, srcpitch);

	return true;
}

bool BitBltYUV420PtoRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch)
{
	const VDPixmap srcbm = {
		.data   = srcy,
		.w      = w,
		.h      = h,
		.pitch  = srcpitch,
		.format = nsVDPixmap::kPixFormat_YUV420_Planar,
		.data2  = srcu,
		.pitch2 = srcpitch/2,
		.data3  = srcv,
		.pitch3 = srcpitch/2
	};

	VDPixmap dstpxm = {
		.data   = dst + dstpitch * (h - 1),
		.w      = w,
		.h      = h,
		.pitch  = -dstpitch
	};

	switch(dbpp) {
	case 24:
		dstpxm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		dstpxm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltYUV420PtoYUY2(int w, int h, BYTE* dst, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch)
{
	BYTE* src[3] = { srcy, srcu, srcv };
	ConvertI420toYUY2(h, dst, dstpitch, src, srcpitch, false);

	return true;
}

bool BitBltYUV420PtoYUY2Interlaced(int w, int h, BYTE* dst, int dstpitch, BYTE* srcy, BYTE* srcu, BYTE* srcv, int srcpitch)
{
	BYTE* src[3] = { srcy, srcu, srcv };
	ConvertI420toYUY2(h, dst, dstpitch, src, srcpitch, true);

	return true;
}

bool BitBltRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* src, int srcpitch, int sbpp)
{
	VDPixmap srcbm = {
		.data   = src + srcpitch * (h - 1),
		.w      = w,
		.h      = h,
		.pitch  = -srcpitch
	};

	switch(sbpp) {
	case 24:
		srcbm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		srcbm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	VDPixmap dstpxm = {
		.data   = dst + dstpitch * (h - 1),
		.w      = w,
		.h      = h,
		.pitch  = -dstpitch
	};

	switch(dbpp) {
	case 24:
		dstpxm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		dstpxm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltRGBStretch(int dstw, int dsth, BYTE* dst, int dstpitch, int dbpp, int srcw, int srch, BYTE* src, int srcpitch, int sbpp)
{
	VDPixmap srcbm = {
		.data   = src + srcpitch * (srch - 1),
		.w      = srcw,
		.h      = srch,
		.pitch  = -srcpitch
	};

	switch(sbpp) {
	case 24:
		srcbm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		srcbm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	VDPixmap dstpxm = {
		.data   = dst + dstpitch * (dsth - 1),
		.w      = dstw,
		.h      = dsth,
		.pitch  = -dstpitch
	};

	switch(dbpp) {
	case 24:
		dstpxm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		dstpxm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	return VDPixmapResample(dstpxm, srcbm, IVDPixmapResampler::kFilterPoint);
}

bool BitBltYUY2toRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* src, int srcpitch)
{
	if(srcpitch == 0) srcpitch = w;

	const VDPixmap srcbm = {
		.data   = src,
		.w      = w,
		.h      = h,
		.pitch  = srcpitch,
		.format = nsVDPixmap::kPixFormat_YUV422_YUYV
	};

	VDPixmap dstpxm = {
		.data   = dst + dstpitch * (h - 1),
		.w      = w,
		.h      = h,
		.pitch  = -dstpitch
	};

	switch(dbpp) {
	case 24:
		dstpxm.format = nsVDPixmap::kPixFormat_RGB888;
		break;
	case 32:
		dstpxm.format = nsVDPixmap::kPixFormat_XRGB8888;
		break;
	default:
		VDASSERT(false);
	}

	return VDPixmapBlt(dstpxm, srcbm);
}
