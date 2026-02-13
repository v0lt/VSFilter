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

#pragma once

#include <atlsync.h>
#include <wmcodecdsp.h>
#include <moreuuids.h>
#include "DirectVobSub.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"
#include "SubPic/ISubPic.h"
#include "Scale2x.h"

struct SystrayIconData {
	HWND hSystrayWnd;
	IFilterGraph* graph;
	IDirectVobSub* dvs;
	bool fRunOnce, fShowIcon;
};

static const VFormatDesc VSFilterDefaultFormats[] = {
	VFormat_P010,
	VFormat_P016,
	VFormat_NV12,
	VFormat_YV12,
	VFormat_YUY2,
	VFormat_IYUV,
	VFormat_YV24,
	VFormat_AYUV,
	VFormat_ARGB32,
	VFormat_RGB32,
	VFormat_RGB24,
};

const VFormatDesc& GetVFormatDesc(const GUID& subtype);

void ShowPPage(CStringW DisplayName, HWND hParentWnd);
void ShowPPage(IUnknown* pUnknown, HWND hParentWnd);

/* This is for graphedit */

class __declspec(uuid("93A22E7A-5091-45ef-BA61-6DA26156A5D0"))
	CDirectVobSubFilter
	: public CBaseVideoFilter
	, public CDirectVobSub
	, public ISpecifyPropertyPages
	, public IAMStreamSelect
	, public CAMThread
{
	friend class CTextInputPin;

	CCritSec m_csQueueLock;
	CComPtr<ISubPicQueue> m_pSubPicQueue;
	void InitSubPicQueue();
	SubPicDesc m_spd;

	bool AdjustFrameSize(CSize& s);

	HANDLE m_hEvtTransform = nullptr;

	HRESULT CopyBuffer(BYTE* pOut, BYTE* pIn, int w, int h, int pitchIn, const GUID& subtype, bool fInterlaced = false);

protected:
	void GetOutputFormats(int& nNumber, VFormatDesc** ppFormats);
	void GetOutputSize(int& w, int& h, int& arx, int& ary) override;
	HRESULT Transform(IMediaSample* pIn);

public:
	CDirectVobSubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(CDirectVobSubFilter));
	virtual ~CDirectVobSubFilter();

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	// CBaseFilter

	CBasePin* GetPin(int n);
	int GetPinCount();

	STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
	STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);

	// CTransformFilter
	HRESULT GetMediaType(int iPosition, CMediaType* pMediaType) override;
	HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType* pMediaType);
	HRESULT CompleteConnect(PIN_DIRECTION dir, IPin* pReceivePin);
	HRESULT BreakConnect(PIN_DIRECTION dir);
	HRESULT StartStreaming();
	HRESULT StopStreaming();
	HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

	HRESULT CheckInputType(const CMediaType* mtIn);
	HRESULT CheckOutputType(const CMediaType& mtOut);
	HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);
	HRESULT DoCheckTransform(const CMediaType* mtIn, const CMediaType* mtOut, bool checkReconnection);

	std::vector<CTextInputPin*> m_pTextInputs;

	// IDirectVobSub
	STDMETHODIMP put_FileName(WCHAR* fn);
	STDMETHODIMP get_LanguageCount(int* nLangs);
	STDMETHODIMP get_LanguageName(int iLanguage, WCHAR** ppName);
	STDMETHODIMP put_SelectedLanguage(int iSelected);
	STDMETHODIMP put_HideSubtitles(bool fHideSubtitles);
	STDMETHODIMP put_PreBuffering(bool fDoPreBuffering); // deprecated
	STDMETHODIMP put_SubPictToBuffer(unsigned int uSubPictToBuffer);
	STDMETHODIMP put_AnimWhenBuffering(bool fAnimWhenBuffering);
	STDMETHODIMP put_AllowDropSubPic(bool fAllowDropSubPic);
	STDMETHODIMP put_Placement(bool fOverridePlacement, int xperc, int yperc);
	STDMETHODIMP put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize);
	STDMETHODIMP put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer);
	STDMETHODIMP put_SubtitleTiming(int delay, int speedmul, int speeddiv);
	STDMETHODIMP get_MediaFPS(bool* fEnabled, double* fps);
	STDMETHODIMP put_MediaFPS(bool fEnabled, double fps);
	STDMETHODIMP get_ZoomRect(NORMALIZEDRECT* rect);
	STDMETHODIMP put_ZoomRect(NORMALIZEDRECT* rect);
	STDMETHODIMP HasConfigDialog(int iSelected);
	STDMETHODIMP ShowConfigDialog(int iSelected, HWND hWndParent);

	// IDirectVobSub2
	STDMETHODIMP put_TextSettings(STSStyle* pDefStyle);
	STDMETHODIMP put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType);

	// IDirectVobSub3
	STDMETHODIMP get_LanguageType(int iLanguage, int* pType);

	// ISpecifyPropertyPages
	STDMETHODIMP GetPages(CAUUID* pPages);

	// IAMStreamSelect
	STDMETHODIMP Count(DWORD* pcStreams);
	STDMETHODIMP Enable(long lIndex, DWORD dwFlags);
	STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk);

	// CPersistStream
	STDMETHODIMP GetClassID(CLSID* pClsid);

protected:
	std::vector<VFormatDesc> m_VideoOutputFormats;

	HDC m_hdc     = nullptr;
	HBITMAP m_hbm = nullptr;
	HFONT m_hfont = nullptr;
	void PrintMessages(BYTE* pOut);

	/* ResX2 */
	std::unique_ptr<BYTE> m_pTempPicBuff;
	void CopyPlane(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, uint32_t black);

	// segment start time, absolute time
	CRefTime m_tPrev;
	REFERENCE_TIME CalcCurrentTime();

	double m_fps = 25.0;

	// 3.x- versions of microsoft's mpeg4 codec output flipped image
	bool m_bMSMpeg4Fix = false;

	// don't set the "hide subtitles" stream until we are finished with loading
	bool m_bLoading = false;

	CString m_videoFileName;

	bool Open();

	int FindPreferedLanguage(bool fHideToo = true);
	void UpdatePreferedLanguages(CString lang);

	CCritSec m_csSubLock;
	CInterfaceList<ISubStream> m_pSubStreams;
	DWORD_PTR m_nSubtitleId = (DWORD_PTR)-1;
	void UpdateSubtitle(bool fApplyDefStyle = true);
	void SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle = true);
	void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);

	// the text input pin is using these
	void AddSubStream(ISubStream* pSubStream);
	void RemoveSubStream(ISubStream* pSubStream);
	void Post_EC_OLE_EVENT(CString str, DWORD_PTR nSubtitleId = -1);

private:
	class CFileReloaderData
	{
	public:
		ATL::CEvent EndThreadEvent, RefreshEvent;
		std::list<CString> files;
		std::vector<CTime> mtime;
	} m_frd;

	void SetupFRD(CStringArray& paths, std::vector<HANDLE>& handles);
	DWORD ThreadProc();

private:
	typedef void(*BltLineFn)(uint8_t* dst, const uint32_t* src, const int w);

	void SetupInputFunc();
	void SetupOutputFunc();

	HANDLE m_hSystrayThread = nullptr;
	SystrayIconData m_tbid = {};

	const VFormatDesc* m_pInputVFormat = &VFormat_None;
	const VFormatDesc* m_pOutputVFormat = &VFormat_None;
	uint32_t m_black   = 0;
	uint32_t m_blackUV = 0;

	Scale2xFn m_fnScale2x = nullptr;

	BltLineFn m_fnBltLine = nullptr;

	VIDEOINFOHEADER2 m_CurrentVIH2 = {};

	bool m_bExternalSubtitle = false;
	std::list<ISubStream*> m_ExternalSubstreams;
	int get_ExternalSubstreamsLanguageCount();
};

/* The "auto-loading" version */

class __declspec(uuid("9852A670-F845-491b-9BE6-EBD841B8A613"))
	CDirectVobSubFilter2 : public CDirectVobSubFilter
{
	bool ShouldWeAutoload(IFilterGraph* pGraph);
	void GetRidOfInternalScriptRenderer();

public:
	CDirectVobSubFilter2(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(CDirectVobSubFilter2));

	HRESULT CheckConnect(PIN_DIRECTION dir, IPin* pPin);
	STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
	HRESULT CheckInputType(const CMediaType* mtIn);
};
