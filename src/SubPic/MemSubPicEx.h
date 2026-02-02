/*
 * (C) 2022-2026 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "MemSubPic.h"

enum {
    MSP_RGB32,
    MSP_RGB24,
    MSP_RGB16, // obsolete, no longer supported
    MSP_RGB15, // obsolete, no longer supported
    MSP_YUY2,
    MSP_YV12,
    MSP_IYUV,
    MSP_AYUV,
    MSP_RGBA,        //pre-multiplied alpha. Use A*g + RGB to mix
    MSP_RGBA_F,      //pre-multiplied alpha. Use (0xff-A)*g + RGB to mix
    MSP_AYUV_PLANAR, //AYUV in planar form
    MSP_XY_AUYV,
    MSP_P010,
    MSP_P016,
    MSP_NV12,
    MSP_NV21,
    MSP_P210, // 4:2:2 10 bits
    MSP_P216, // 4:2:2 16 bits
    MSP_YV16, // 4:2:2 8 bits
    MSP_YV24  // 4:4:4 8 bits
};

// CMemSubPicEx

class CMemSubPicEx : public CMemSubPic
{
public:
	CMemSubPicEx(SubPicDesc& spd);

	// ISubPic
	STDMETHODIMP Unlock(RECT* pDirtyRect) override;
	STDMETHODIMP AlphaBlt(RECT* pSrc, RECT* pDst, SubPicDesc* pTarget) override;
};

// CMemSubPicExAllocator

class CMemSubPicExAllocator : public CMemSubPicAllocator
{
//protected:
	// CSubPicAllocatorImpl
	bool Alloc(bool fStatic, ISubPic** ppSubPic) override;

public:
	CMemSubPicExAllocator(int type, SIZE maxsize);
};
