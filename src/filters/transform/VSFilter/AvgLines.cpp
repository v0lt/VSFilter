//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"

void AvgLines8(BYTE* dst, DWORD h, DWORD pitch)
{
	if (h <= 1) {
		return;
	}

	BYTE* s = dst;
	BYTE* d = dst + (h-2)*pitch;

	for (; s < d; s += pitch*2) {
		BYTE* tmp = s;

#ifndef _WIN64
		if (!((DWORD)tmp&0xf) && !((DWORD)pitch&0xf)) {
			__asm {
				mov		esi, tmp
				mov		ebx, pitch

				mov		ecx, ebx
				shr		ecx, 4

				AvgLines8_sse2_loop:
				movdqa	xmm0, [esi]
				pavgb	xmm0, [esi+ebx*2]
				movdqa	[esi+ebx], xmm0
				add		esi, 16

				dec		ecx
				jnz		AvgLines8_sse2_loop

				mov		tmp, esi
			}

			for (ptrdiff_t i = pitch&7; i--; tmp++) {
				tmp[pitch] = (tmp[0] + tmp[pitch<<1] + 1) >> 1;
			}
		} else {
			__asm {
				mov		esi, tmp
				mov		ebx, pitch

				mov		ecx, ebx
				shr		ecx, 3

				pxor	mm7, mm7
				AvgLines8_mmx_loop:
				movq	mm0, [esi]
				movq	mm1, mm0

				punpcklbw	mm0, mm7
				punpckhbw	mm1, mm7

				movq	mm2, [esi+ebx*2]
				movq	mm3, mm2

				punpcklbw	mm2, mm7
				punpckhbw	mm3, mm7

				paddw	mm0, mm2
				psrlw	mm0, 1

				paddw	mm1, mm3
				psrlw	mm1, 1

				packuswb	mm0, mm1

				movq	[esi+ebx], mm0

				lea		esi, [esi+8]

				dec		ecx
				jnz		AvgLines8_mmx_loop

				mov		tmp, esi
			}

			for (ptrdiff_t i = pitch&7; i--; tmp++) {
				tmp[pitch] = (tmp[0] + tmp[pitch<<1] + 1) >> 1;
			}
		}
#else
		{
			for (ptrdiff_t i = pitch; i--; tmp++) {
				tmp[pitch] = (tmp[0] + tmp[pitch<<1] + 1) >> 1;
			}
		}
#endif
	}

	if (!(h&1) && h >= 2) {
		dst += (h-2)*pitch;
		memcpy(dst + pitch, dst, pitch);
	}

#ifndef _WIN64
	__asm emms;
#endif
}
