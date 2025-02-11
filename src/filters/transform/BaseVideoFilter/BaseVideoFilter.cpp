/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2023 see Authors.txt
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
#include <mmintrin.h>
#include <memory.h>
#include <mfapi.h>
#include "BaseVideoFilter.h"
#include "DSUtil/DSUtil.h"
#include <moreuuids.h>

//
// CBaseVideoFilter
//

CBaseVideoFilter::CBaseVideoFilter(LPCWSTR pName, LPUNKNOWN lpunk, HRESULT* phr, REFCLSID clsid, long cBuffers)
	: CTransformFilter(pName, lpunk, clsid)
	, m_cBuffers(cBuffers)
{
	if (phr) {
		*phr = S_OK;
	}
}

CBaseVideoFilter::~CBaseVideoFilter()
{
}

void CBaseVideoFilter::SetAspect(CSize aspect)
{
	if (m_arx != aspect.cx || m_ary != aspect.cy) {
		m_arx = aspect.cx;
		m_ary = aspect.cy;
	}
}

int CBaseVideoFilter::GetPinCount()
{
	return 2;
}

CBasePin* CBaseVideoFilter::GetPin(int n)
{
	switch (n) {
		case 0:
			return m_pInput;
		case 1:
			return m_pOutput;
	}
	return nullptr;
}

HRESULT CBaseVideoFilter::Receive(IMediaSample* pIn)
{
#ifndef _WIN64
	// TODOX64 : fixme!
	_mm_empty(); // just for safety
#endif

	CAutoLock cAutoLock(&m_csReceive);

	HRESULT hr;

	AM_SAMPLE2_PROPERTIES* const pProps = m_pInput->SampleProps();
	if (pProps->dwStreamId != AM_STREAM_MEDIA) {
		return m_pOutput->Deliver(pIn);
	}

	AM_MEDIA_TYPE* pmt;
	if (SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt) {
		CMediaType mt(*pmt);
		DeleteMediaType(pmt);
		if (mt != m_pInput->CurrentMediaType()) {
			m_pInput->SetMediaType(&mt);
		}
	}

	if (FAILED(hr = Transform(pIn))) {
		return hr;
	}

	return S_OK;
}

HRESULT CBaseVideoFilter::GetDeliveryBuffer(int w, int h, IMediaSample** ppOut, REFERENCE_TIME AvgTimePerFrame, DXVA2_ExtendedFormat* dxvaExtFlags)
{
	CheckPointer(ppOut, E_POINTER);

	HRESULT hr;

	if (FAILED(hr = ReconnectOutput(w, h, false, AvgTimePerFrame, dxvaExtFlags))) {
		return hr;
	}

	if (FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, nullptr, nullptr, 0))) {
		return hr;
	}

	AM_MEDIA_TYPE* pmt;
	if (SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt) {
		CMediaType mt = *pmt;
		m_pOutput->SetMediaType(&mt);
		DeleteMediaType(pmt);
	}

	if (m_bSendMediaType) {
		CMediaType& mt = m_pOutput->CurrentMediaType();
		AM_MEDIA_TYPE* sendmt = CreateMediaType(&mt);
		(*ppOut)->SetMediaType(sendmt);
		DeleteMediaType(sendmt);
		m_bSendMediaType = false;
	}

	(*ppOut)->SetDiscontinuity(FALSE);
	(*ppOut)->SetSyncPoint(TRUE);

	// FIXME: hell knows why but without this the overlay mixer starts very skippy
	// (don't enable this for other renderers, the old for example will go crazy if you do)
	if (GetCLSID(m_pOutput->GetConnected()) == CLSID_OverlayMixer) {
		(*ppOut)->SetDiscontinuity(TRUE);
	}

	return S_OK;
}

HRESULT CBaseVideoFilter::ReconnectOutput(int width, int height, bool bForce/* = false*/, REFERENCE_TIME AvgTimePerFrame/* = 0*/, DXVA2_ExtendedFormat* dxvaExtFormat/* = nullptr*/)
{
	CMediaType& mt = m_pOutput->CurrentMediaType();

	bool bNeedReconnect = bForce;
	{
		int wout = 0, hout = 0, arxout = 0, aryout = 0;
		ExtractDim(&mt, wout, hout, arxout, aryout);
		if (arxout != m_arx || aryout != m_ary) {
			bNeedReconnect = true;
		}
	}

	REFERENCE_TIME nAvgTimePerFrame = 0;
	ExtractAvgTimePerFrame(&mt, nAvgTimePerFrame);
	if (!AvgTimePerFrame) {
		AvgTimePerFrame = nAvgTimePerFrame;
	}

	if (!bNeedReconnect && abs(nAvgTimePerFrame - AvgTimePerFrame) > 10) {
		bNeedReconnect = true;
	}

	if (!bNeedReconnect &&
			(width != m_wout || height != m_hout || m_arx != m_arxout || m_ary != m_aryout)) {
		bNeedReconnect = true;
	}

	if (!bNeedReconnect && dxvaExtFormat) {
		VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)mt.Format();
		if (vih2->dwControlFlags != dxvaExtFormat->value) {
			bNeedReconnect = true;
		}
	}

	if (bNeedReconnect) {
		const CLSID clsid = GetCLSID(m_pOutput->GetConnected());
		if (clsid == CLSID_VideoRenderer) {
			NotifyEvent(EC_ERRORABORT, 0, 0);
			return E_FAIL;
		}
		const bool m_bOverlayMixer = !!(clsid == CLSID_OverlayMixer);

		CRect vih_rect(0, 0, width, height);
		VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)mt.Format();

#ifndef NDEBUG
		CStringW debugMessage(L"CBaseVideoFilter::ReconnectOutput() : Performing reconnect\n");
		debugMessage.AppendFormat(L"    => SIZE  : %d:%d -> %d:%d\n", m_wout, m_hout, vih_rect.Width(), vih_rect.Height());
		debugMessage.AppendFormat(L"    => AR    : %d:%d -> %d:%d\n", m_arxout, m_aryout, m_arx, m_ary);
		debugMessage.AppendFormat(L"    => FPS   : %I64d -> %I64d\n", nAvgTimePerFrame, AvgTimePerFrame);
		debugMessage.AppendFormat(L"    => FLAGS : 0x%0.8x -> 0x%0.8x", vih2->dwControlFlags, dxvaExtFormat ? dxvaExtFormat->value : vih2->dwControlFlags);
		DLog(debugMessage);
#endif

		const bool bVideoSizeChanged = (width != m_wout || height != m_hout);
		if (bVideoSizeChanged) {
			vih2->rcSource = vih2->rcTarget = vih_rect;
		}
		vih2->AvgTimePerFrame    = AvgTimePerFrame;
		vih2->dwPictAspectRatioX = m_arx;
		vih2->dwPictAspectRatioY = m_ary;

		if (dxvaExtFormat) {
			vih2->dwControlFlags = dxvaExtFormat->value;
		}

		auto pBMI         = &vih2->bmiHeader;
		pBMI->biWidth     = width;
		pBMI->biHeight    = height;
		pBMI->biSizeImage = DIBSIZE(*pBMI);

		HRESULT hrQA = m_pOutput->GetConnected()->QueryAccept(&mt);
		ASSERT(SUCCEEDED(hrQA)); // should better not fail, after all "mt" is the current media type, just with a different resolution
		HRESULT hr = S_OK;

		if (m_nDecoderMode != MODE_SOFTWARE) {
			m_pOutput->SetMediaType(&mt);
			m_bSendMediaType = true;
		} else {
			bool tryReceiveConnection = true;
			for (;;) {
				hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt);
				if (SUCCEEDED(hr)) {
					CComPtr<IMediaSample> pOut;
					if (SUCCEEDED(hr = m_pOutput->GetDeliveryBuffer(&pOut, nullptr, nullptr, 0))) {
						AM_MEDIA_TYPE* pmt;
						if (SUCCEEDED(pOut->GetMediaType(&pmt)) && pmt) {
							BITMAPINFOHEADER bihOut;
							if (ExtractBIH(pmt, &bihOut)) {
								DLog(L"CBaseVideoFilter::ReconnectOutput() : new MediaType from IMediaSample negotiated, actual width: %d, requested: %ld", width, bihOut.biWidth);
							}

							CMediaType mt2 = *pmt;
							m_pOutput->SetMediaType(&mt2);
							DeleteMediaType(pmt);
						} else {
							if (m_bOverlayMixer) {
								// stupid overlay mixer won't let us know the new pitch...
								long size = pOut->GetSize();
								pBMI->biWidth = size ? (size / abs(pBMI->biHeight) * 8 / pBMI->biBitCount) : pBMI->biWidth;
							}
							m_pOutput->SetMediaType(&mt);
						}
						m_bSendMediaType = true;
					} else {
						return E_FAIL;
					}
				} else if (hr == VFW_E_BUFFERS_OUTSTANDING && tryReceiveConnection) {
					DLog(L"CBaseVideoFilter::ReconnectOutput() : VFW_E_BUFFERS_OUTSTANDING, flushing data ...");
					m_pOutput->DeliverBeginFlush();
					m_pOutput->DeliverEndFlush();
					tryReceiveConnection = false;

					continue;
				} else if (hrQA == S_OK) {
					m_pOutput->SetMediaType(&mt);
					m_bSendMediaType = true;
				} else {
					DLog(L"CBaseVideoFilter::ReconnectOutput() : ReceiveConnection() failed (hr: %x); QueryAccept: %x", hr, hrQA);
					return E_FAIL;
				}

				break;
			}
		}

		m_wout   = width;
		m_hout   = height;
		m_arxout = m_arx;
		m_aryout = m_ary;

		// some renderers don't send this
		if (m_nDecoderMode == MODE_SOFTWARE) {
			NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(width, height), 0);
		}

		return S_OK;
	}

	return S_FALSE;
}

HRESULT CBaseVideoFilter::CheckInputType(const CMediaType* mtIn)
{
	return S_OK;
}

HRESULT CBaseVideoFilter::CheckOutputType(const CMediaType& mtOut)
{
	return S_OK;
}

HRESULT CBaseVideoFilter::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
	return S_OK;
}

HRESULT CBaseVideoFilter::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
	if (m_pInput->IsConnected() == FALSE) {
		return E_UNEXPECTED;
	}

	BITMAPINFOHEADER bih;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bih);

	pProperties->cBuffers	= m_cBuffers;
	pProperties->cbBuffer	= bih.biSizeImage;
	pProperties->cbAlign	= 1;
	pProperties->cbPrefix	= 0;

	HRESULT hr;
	ALLOCATOR_PROPERTIES Actual;
	if (FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
		return hr;
	}

	return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
		   ? E_FAIL
		   : NOERROR;
}

HRESULT CBaseVideoFilter::GetMediaType(int iPosition, CMediaType* pmt)
{
	VIDEO_OUTPUT_FORMATS* fmts;
	int                   nFormatCount;

	if (m_pInput->IsConnected() == FALSE) {
		return E_UNEXPECTED;
	}

	GetOutputFormats(nFormatCount, &fmts);
	if (iPosition < 0) {
		return E_INVALIDARG;
	}
	if (iPosition >= nFormatCount) {
		return VFW_S_NO_MORE_ITEMS;
	}

	pmt->majortype = MEDIATYPE_Video;
	pmt->subtype   = *fmts[iPosition].subtype;

	int w = m_win;
	int h = m_hin;
	int arx = m_arxin;
	int ary = m_aryin;
	GetOutputSize(w, h, arx, ary);

	m_wout = m_win;
	m_hout = m_hin;
	m_arxout = m_arxin;
	m_aryout = m_aryin;

	if (m_bMVC_Output_TopBottom) {
		h *= 2;
		m_hout *= 2;

		ary *= 2;
		m_aryout *= 2;

		ReduceDim(arx, ary);
		ReduceDim(m_arxout, m_aryout);
	}

	BITMAPINFOHEADER bihOut = { 0 };
	bihOut.biSize        = sizeof(bihOut);
	bihOut.biWidth       = w;
	bihOut.biHeight      = h;
	bihOut.biPlanes      = 1; // this value must be set to 1
	bihOut.biBitCount    = fmts[iPosition].biBitCount;
	bihOut.biCompression = fmts[iPosition].biCompression;
	bihOut.biSizeImage   = DIBSIZE(bihOut);

	pmt->formattype = FORMAT_VideoInfo2;
	VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
	memset(vih2, 0, sizeof(VIDEOINFOHEADER2));
	vih2->bmiHeader = bihOut;
	vih2->dwPictAspectRatioX = arx;
	vih2->dwPictAspectRatioY = ary;
	if (IsVideoInterlaced()) {
		vih2->dwInterlaceFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;
	}

	if (m_dxvaExtFormat.value && pmt->subtype != MEDIASUBTYPE_RGB32 && pmt->subtype != MEDIASUBTYPE_RGB48) {
		vih2->dwControlFlags = m_dxvaExtFormat.value;
	}

	const CMediaType& mtInput = m_pInput->CurrentMediaType();

	// these fields have the same field offset in all four structs
	((VIDEOINFOHEADER*)pmt->Format())->AvgTimePerFrame = ((VIDEOINFOHEADER*)mtInput.Format())->AvgTimePerFrame;
	((VIDEOINFOHEADER*)pmt->Format())->dwBitRate       = ((VIDEOINFOHEADER*)mtInput.Format())->dwBitRate;
	((VIDEOINFOHEADER*)pmt->Format())->dwBitErrorRate  = ((VIDEOINFOHEADER*)mtInput.Format())->dwBitErrorRate;

	pmt->SetSampleSize(bihOut.biSizeImage);

	{
		// copy source and target rectangles from input pin
		VIDEOINFOHEADER* vih      = (VIDEOINFOHEADER*)pmt->Format();
		VIDEOINFOHEADER* vihInput = (VIDEOINFOHEADER*)mtInput.Format();

		ASSERT(vih);
		if (vihInput) {
			vih->rcSource = vihInput->rcSource;
			vih->rcTarget = vihInput->rcTarget;

			// fix the bad source rectangle
			vih->rcSource.left   = std::clamp(vih->rcSource.left  , 0L, (LONG)m_win);
			vih->rcSource.top    = std::clamp(vih->rcSource.top   , 0L, (LONG)m_hin);
			vih->rcSource.right  = std::clamp(vih->rcSource.right , 0L, (LONG)m_win);
			vih->rcSource.bottom = std::clamp(vih->rcSource.bottom, 0L, (LONG)m_hin);

			if (m_bMVC_Output_TopBottom) {
				vih->rcSource.bottom *= 2;
				vih->rcTarget.bottom *= 2;
			}
		} else {
			vih->rcSource.right  = vih->rcTarget.right  = m_win;
			vih->rcSource.bottom = vih->rcTarget.bottom = m_hin;
		}
	}

	return S_OK;
}

HRESULT CBaseVideoFilter::SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt)
{
	if (dir == PINDIR_INPUT) {
		m_arx = m_ary = 0;
		ExtractDim(pmt, m_win, m_hin, m_arx, m_ary);
		m_arxin = m_arx;
		m_aryin = m_ary;
		GetOutputSize(m_wout, m_hout, m_arx, m_ary);

		ReduceDim(m_arx, m_ary);

		m_nDecoderMode = MODE_SOFTWARE;
	}

	return __super::SetMediaType(dir, pmt);
}

//
// CBaseVideoInputAllocator
//

CBaseVideoInputAllocator::CBaseVideoInputAllocator(HRESULT* phr)
	: CMemAllocator(L"CBaseVideoInputAllocator", nullptr, phr)
{
	if (phr) {
		*phr = S_OK;
	}
}

void CBaseVideoInputAllocator::SetMediaType(const CMediaType& mt)
{
	m_mt = mt;
}

STDMETHODIMP CBaseVideoInputAllocator::GetBuffer(IMediaSample** ppBuffer, REFERENCE_TIME* pStartTime, REFERENCE_TIME* pEndTime, DWORD dwFlags)
{
	if (!m_bCommitted) {
		return VFW_E_NOT_COMMITTED;
	}

	HRESULT hr = __super::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);

	if (SUCCEEDED(hr) && m_mt.majortype != GUID_NULL) {
		(*ppBuffer)->SetMediaType(&m_mt);
		m_mt.majortype = GUID_NULL;
	}

	return hr;
}

//
// CBaseVideoInputPin
//

CBaseVideoInputPin::CBaseVideoInputPin(LPCWSTR pObjectName, CBaseVideoFilter* pFilter, HRESULT* phr, LPCWSTR pName)
	: CTransformInputPin(pObjectName, pFilter, phr, pName)
	, m_pAllocator(nullptr)
{
}

CBaseVideoInputPin::~CBaseVideoInputPin()
{
	delete m_pAllocator;
}

STDMETHODIMP CBaseVideoInputPin::GetAllocator(IMemAllocator** ppAllocator)
{
	CheckPointer(ppAllocator, E_POINTER);

	if (m_pAllocator == nullptr) {
		HRESULT hr = S_OK;
		m_pAllocator = DNew CBaseVideoInputAllocator(&hr);
		m_pAllocator->AddRef();
	}

	(*ppAllocator = m_pAllocator)->AddRef();

	return S_OK;
}

STDMETHODIMP CBaseVideoInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
	CAutoLock cObjectLock(m_pLock);

	if (m_Connected) {
		CMediaType mt(*pmt);

		if (FAILED(CheckMediaType(&mt))) {
			return VFW_E_TYPE_NOT_ACCEPTED;
		}

		ALLOCATOR_PROPERTIES props, actual;

		CComPtr<IMemAllocator> pMemAllocator;
		if (FAILED(GetAllocator(&pMemAllocator))
				|| FAILED(pMemAllocator->Decommit())
				|| FAILED(pMemAllocator->GetProperties(&props))) {
			return E_FAIL;
		}

		BITMAPINFOHEADER bih;
		if (ExtractBIH(pmt, &bih) && bih.biSizeImage) {
			props.cbBuffer = bih.biSizeImage;
		}

		if (FAILED(pMemAllocator->SetProperties(&props, &actual))
				|| FAILED(pMemAllocator->Commit())
				|| props.cbBuffer != actual.cbBuffer) {
			return E_FAIL;
		}

		if (m_pAllocator) {
			m_pAllocator->SetMediaType(mt);
		}

		return SetMediaType(&mt) == S_OK
			   ? S_OK
			   : VFW_E_TYPE_NOT_ACCEPTED;
	}

	return __super::ReceiveConnection(pConnector, pmt);
}

//
// CBaseVideoOutputPin
//

CBaseVideoOutputPin::CBaseVideoOutputPin(LPCWSTR pObjectName, CBaseVideoFilter* pFilter, HRESULT* phr, LPCWSTR pName)
	: CTransformOutputPin(pObjectName, pFilter, phr, pName)
{
}

HRESULT CBaseVideoOutputPin::CheckMediaType(const CMediaType* mtOut)
{
	if (IsConnected()) {
		HRESULT hr = (static_cast<CBaseVideoFilter*>(m_pFilter))->CheckOutputType(*mtOut);
		if (FAILED(hr)) {
			return hr;
		}
	}

	return __super::CheckMediaType(mtOut);
}
