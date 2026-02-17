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

void BltLineRGB32(uint8_t* dst, const uint32_t* src, const int w)
{
	uint32_t* dst32 = (uint32_t*)dst;
	uint32_t* end = dst32 + w;
	pixrgba pix;

	for (; dst32 < end; dst32++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			*dst32 = pix.u32 & 0x00ffffff;
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

void BltLineAYUV(uint8_t* dst, const uint32_t* src, const int w)
{
	uint32_t* dst32 = (uint32_t*)dst;
	uint32_t* end = dst32 + w;
	pixrgba pix;

	for (; dst32 < end; dst32++) {
		pix.u32 = *src++;
		if (pix.a < 0xff) {
			int y = (c2y_yb[pix.r] + c2y_yg[pix.g] + c2y_yr[pix.b] + 0x108000) & 0x00ff0000;
			*dst32 = 0x00008080 | y; // w/o colors
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

void CDirectVobSubFilter::CopyPlane(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, uint32_t black)
{
	auto& packsize = m_pInputVFormat->packsize;

	int wIn = in.cx;
	int hIn = in.cy;
	int pitchIn = wIn * packsize;

	int wSub = sub.cx;
	int hSub = sub.cy;
	int pitchSub = wSub * packsize;

	bool fScale2x = wIn*2 <= wSub;
	if (fScale2x) {
		wIn <<= 1, hIn <<= 1;
	}

	int left  = ((wSub - wIn)>>1)&~1;
	int mid   = wIn;
	int right = left + ((wSub - wIn)&1);

	int dpLeft  = left * packsize;
	int dpMid   = mid * packsize;
	int dpRight = right * packsize;

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
			if (m_fnScale2x) {
				m_fnScale2x(in.cx, (std::min(j, hSub) - i) >> 1,
					pSub + dpLeft, pitchSub, pIn, pitchIn);
			}

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
}

void CDirectVobSubFilter::SetupInputFunc()
{
	m_fnScale2x = nullptr;
	m_black   = 0;
	m_blackUV = 0;

	auto& subtype = *m_pInputVFormat->subtype;

	switch (m_pInputVFormat->fourcc) {
	case FCC('YV12'):
	case FCC('IYUV'):
	case FCC('I420'):
		m_fnScale2x = Scale2x_YV;
		[[fallthrough]];
	case FCC('NV12'):
		m_black   = 0x10101010;
		m_blackUV = 0x80808080;
		break;
	case FCC('YUY2'):
		m_fnScale2x = Scale2x_YUY2;
		m_black = 0x80108010;
		break;
	case FCC('AYUV'):
		m_black = 0xff108080;
		break;
	case FCC('P010'):
	case FCC('P016'):
		m_black   = 0x10001000;
		m_blackUV = 0x80008000;
		break;
	case BI_RGB:
		if (subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_ARGB32) {
			m_fnScale2x = Scale2x_XRGB32;
		}
		else if (subtype == MEDIASUBTYPE_RGB24) {
			m_fnScale2x = Scale2x_RGB24;
		}
		break;
	}
}

void CDirectVobSubFilter::SetupOutputFunc()
{
	auto& subtype = *m_pOutputVFormat->subtype;

	switch (m_pOutputVFormat->fourcc) {
	case FCC('NV12'):
	case FCC('YV12'):
	case FCC('IYUV'):
	case FCC('I420'):
		m_fnBltLine = BltLineYUVxxxP;
		break;
	case FCC('P010'):
	case FCC('P016'):
		m_fnBltLine = BltLineYUVxxxP16;
		break;
	case FCC('YUY2'):
		m_fnBltLine = BltLineYUY2;
		break;
	case FCC('AYUV'):
		m_fnBltLine = BltLineAYUV;
		break;
	default:
		if (subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_ARGB32) {
			m_fnBltLine = BltLineRGB32;
		}
		else if (subtype == MEDIASUBTYPE_RGB24) {
			m_fnBltLine = BltLineRGB24;
		}
		else {
			m_fnBltLine = nullptr;
		}
	}
}

void CDirectVobSubFilter::PrintMessages(BYTE* pOut)
{
	if (!m_hdc || !m_hbm) {
		return;
	}

	ColorConvInit();

	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	CStringW msg, tmp;

	if (m_bOSD) {
		tmp.Format(
			L"in: %dx%d %s\nout: %dx%d %s\n",
			m_win, m_hout,
			m_pInputVFormat->name,
			bihOut.biWidth, bihOut.biHeight,
			m_pOutputVFormat->name);
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
	int pitchOut = m_pOutputVFormat->GetWidthBytes(bihOut.biWidth);

	if (bihOut.biHeight > 0 && bihOut.biCompression == BI_RGB) { // flip if the dst bitmap is flipped rgb (m_hbm is a top-down bitmap, not like the subpictures)
		pOut += pitchOut * (abs(bihOut.biHeight)-1);
		pitchOut = -pitchOut;
	}

	pIn += pitchIn * r.top;
	pOut += pitchOut * r.top;

	if (m_fnBltLine) {
		const int w = std::min((int)r.right, m_win);
		for (int h = r.Height(); h--; pIn += pitchIn, pOut += pitchOut) {
			m_fnBltLine(pOut, (uint32_t*)pIn, w);
			memset_u32(pIn, 0xff000000, r.right * 4);
		}
	}

	SelectObject(m_hdc, hOldBitmap);
	SelectObject(m_hdc, hOldFont);
}
