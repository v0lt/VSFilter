/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2026 see Authors.txt
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

#include "stdafx.h"
#include <moreuuids.h>
#include "DirectVobSubFilter.h"
#include "Scale2x.h"

extern int c2y_yb[256];
extern int c2y_yg[256];
extern int c2y_yr[256];
extern void ColorConvInit();

union pixrgba {
	uint32_t u32;
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
};

typedef void(*BltLineFn)(uint8_t* dst, const uint32_t* src, const int w);


void BltLineRGB32(uint8_t* dst, const uint32_t* src, const int w)
{
	uint32_t* dst32 = (uint32_t*)dst;
	uint32_t* end = dst32 + w;
	pixrgba pix;

	for (; dst32 < end; dst32++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			*dst32 = pix.u32 & 0xffffff;
		}
	}
}

void BltLineRGB24(uint8_t* dst, const uint32_t* src, const int w)
{
	uint8_t* end = dst + w * 3;
	pixrgba pix;

	for (; dst < end; dst += 3) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			dst[0] = pix.r;
			dst[1] = pix.g;
			dst[2] = pix.b;
		}
	}
}

void BltLineYUY2(uint8_t* dst, const uint32_t* src, const int w)
{
	uint16_t* dst16 = (uint16_t*)dst;
	uint16_t* end = dst16 + w;
	pixrgba pix;

	for (; dst16 < end; dst16++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			int y = (c2y_yb[pix.r] + c2y_yg[pix.g] + c2y_yr[pix.b] + 0x108000) >> 16;
			*dst16 = 0x8000 | y; // w/o colors
		}
	}
}

void BltLineYUVxxxP(uint8_t* dst, const uint32_t* src, const int w)
{
	uint8_t* end = dst + w;
	pixrgba pix;

	for (; dst < end; dst++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			int y = (c2y_yb[pix.r] + c2y_yg[pix.g] + c2y_yr[pix.b] + 0x108000) >> 16;
			*dst = (uint8_t)y; // w/o colors
		}
	}
}

void BltLineYUVxxxP16(uint8_t* dst, const uint32_t* src, const int w)
{
	uint16_t* dst16 = (uint16_t*)dst;
	uint16_t* end = dst16 + w;
	pixrgba pix;

	for (; dst16 < end; dst16++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			int y = (c2y_yb[pix.r] + c2y_yg[pix.g] + c2y_yr[pix.b] + 0x108000) >> 8;
			*dst16 = (uint16_t)y; // w/o colors
		}
	}
}

HRESULT CDirectVobSubFilter::Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
	int wIn = in.cx, hIn = in.cy, pitchIn = wIn*bpp>>3;
	int wSub = sub.cx, hSub = sub.cy, pitchSub = wSub*bpp>>3;
	bool fScale2x = wIn*2 <= wSub;

	if (fScale2x) {
		wIn <<= 1, hIn <<= 1;
	}

	int left = ((wSub - wIn)>>1)&~1;
	int mid = wIn;
	int right = left + ((wSub - wIn)&1);

	int dpLeft = left*bpp>>3;
	int dpMid = mid*bpp>>3;
	int dpRight = right*bpp>>3;

	ASSERT(wSub >= wIn);

	{
		int i = 0, j = 0;

		j += (hSub - hIn) >> 1;

		for (; i < j; i++, pSub += pitchSub) {
			memset_u32(pSub, black, dpLeft+dpMid+dpRight);
		}

		j += hIn;

		if (hIn > hSub) {
			pIn += pitchIn * ((hIn - hSub) >> (fScale2x?2:1));
		}

		if (fScale2x) {
			Scale2x(subtype,
					pSub + dpLeft, pitchSub, pIn, pitchIn,
					in.cx, (std::min(j, hSub) - i) >> 1);

			for (int k = std::min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub) {
				memset_u32(pSub, black, dpLeft);
				memset_u32(pSub + dpLeft+dpMid, black, dpRight);
			}
		} else {
			for (int k = std::min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub) {
				memset_u32(pSub, black, dpLeft);
				memcpy(pSub + dpLeft, pIn, dpMid);
				memset_u32(pSub + dpLeft+dpMid, black, dpRight);
			}
		}

		j = hSub;

		for (; i < j; i++, pSub += pitchSub) {
			memset_u32(pSub, black, dpLeft+dpMid+dpRight);
		}
	}

	return NOERROR;
}

void CDirectVobSubFilter::PrintMessages(BYTE* pOut)
{
	if (!m_hdc || !m_hbm) {
		return;
	}

	ColorConvInit();

	auto vfInput = GetVFormatDesc(m_pInput->CurrentMediaType().subtype);
	auto vfOutput = GetVFormatDesc(m_pOutput->CurrentMediaType().subtype);

	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	CStringW msg, tmp;

	if (m_bOSD) {
		tmp.Format(
			L"in: %dx%d %s\nout: %dx%d %s\n",
			m_win, m_hout,
			vfInput.name,
			bihOut.biWidth, bihOut.biHeight,
			vfOutput.name);
		msg += tmp;

		tmp.Format(L"real fps: %.3f, current fps: %.3f\nmedia time: %d, subtitle time: %d [ms]\nframe number: %d (calculated)\nrate: %.4f\n",
				   m_fps, m_bMediaFPSEnabled?m_MediaFPS:fabs(m_fps),
				   (int)m_tPrev.Millisecs(), (int)(CalcCurrentTime()/10000),
				   (int)(m_tPrev.m_time * m_fps / 10000000),
				   m_pInput->CurrentRate());
		msg += tmp;

		CAutoLock cAutoLock(&m_csQueueLock);

		if (m_pSubPicQueue) {
			int nSubPics = -1;
			REFERENCE_TIME rtNow = -1, rtStart = -1, rtStop = -1;
			m_pSubPicQueue->GetStats(nSubPics, rtNow, rtStart, rtStop);
			tmp.Format(L"queue stats: %I64d - %I64d [ms]\n", rtStart/10000, rtStop/10000);
			msg += tmp;

			for (int i = 0; i < nSubPics; i++) {
				m_pSubPicQueue->GetStats(i, rtStart, rtStop);
				tmp.Format(L"%d: %I64d - %I64d [ms]\n", i, rtStart/10000, rtStop/10000);
				msg += tmp;
			}

		}
	}

	if (msg.IsEmpty()) {
		return;
	}

	HANDLE hOldBitmap = SelectObject(m_hdc, m_hbm);
	HANDLE hOldFont = SelectObject(m_hdc, m_hfont);

	SetTextColor(m_hdc, 0xffffff);
	SetBkMode(m_hdc, TRANSPARENT);
	SetMapMode(m_hdc, MM_TEXT);

	BITMAP bm;
	GetObjectW(m_hbm, sizeof(BITMAP), &bm);

	CRect r(0, 0, bm.bmWidth, bm.bmHeight);
	DrawTextW(m_hdc, msg, wcslen(msg), &r, DT_CALCRECT|DT_EXTERNALLEADING|DT_NOPREFIX|DT_WORDBREAK);

	r += CPoint(10, 10);
	r &= CRect(0, 0, bm.bmWidth, bm.bmHeight);

	DrawTextW(m_hdc, msg, wcslen(msg), &r, DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);

	BYTE* pIn = (BYTE*)bm.bmBits;
	int pitchIn = bm.bmWidthBytes;
	int pitchOut = vfOutput.GetWidthBytes(bihOut.biWidth);

	if (bihOut.biHeight > 0 && bihOut.biCompression <= 3) { // flip if the dst bitmap is flipped rgb (m_hbm is a top-down bitmap, not like the subpictures)
		pOut += pitchOut * (abs(bihOut.biHeight)-1);
		pitchOut = -pitchOut;
	}

	pIn += pitchIn * r.top;
	pOut += pitchOut * r.top;

	BltLineFn fnBltLine = nullptr;

	if (*vfOutput.subtype == MEDIASUBTYPE_NV12 || *vfOutput.subtype == MEDIASUBTYPE_YV12
			|| *vfOutput.subtype == MEDIASUBTYPE_I420 || *vfOutput.subtype == MEDIASUBTYPE_IYUV) {
		fnBltLine = BltLineYUVxxxP;
	}
	else if (*vfOutput.subtype == MEDIASUBTYPE_P010 || *vfOutput.subtype == MEDIASUBTYPE_P016)
	{
		fnBltLine = BltLineYUVxxxP16;
	}
	else if (*vfOutput.subtype == MEDIASUBTYPE_YUY2) {
		fnBltLine = BltLineYUY2;
	}
	else if (*vfOutput.subtype == MEDIASUBTYPE_RGB24) {
		fnBltLine = BltLineRGB24;
	}
	else if (*vfOutput.subtype == MEDIASUBTYPE_RGB32 || *vfOutput.subtype == MEDIASUBTYPE_ARGB32) {
		fnBltLine = BltLineRGB32;
	}

	if (fnBltLine) {
		for (int w = std::min((int)r.right, m_win), h = r.Height(); h--; pIn += pitchIn, pOut += pitchOut) {
			fnBltLine(pOut, (uint32_t*)pIn, w);
			memset_u32(pIn, 0xff000000, r.right * 4);
		}
	}

	SelectObject(m_hdc, hOldBitmap);
	SelectObject(m_hdc, hOldFont);
}
