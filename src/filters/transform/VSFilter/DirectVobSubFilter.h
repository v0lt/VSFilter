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

#pragma once

#include <atlsync.h>
#include <wmcodecdsp.h>
#include <moreuuids.h>
#include "DirectVobSub.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"
#include "SubPic/ISubPic.h"

struct SystrayIconData {
	HWND hSystrayWnd;
	IFilterGraph* graph;
	IDirectVobSub* dvs;
	bool fRunOnce, fShowIcon;
};

static const VIDEO_OUTPUT_FORMATS VSFilterDefaultFormats[] = {
	{&MEDIASUBTYPE_P010,  FCC('P010'),  24, 2},
	{&MEDIASUBTYPE_P016,  FCC('P016'),  24, 2},
	{&MEDIASUBTYPE_NV12,  FCC('NV12'),  12, 1},
	{&MEDIASUBTYPE_YV12,  FCC('YV12'),  12, 1},
	{&MEDIASUBTYPE_YUY2,  FCC('YUY2'),  16, 2},
	{&MEDIASUBTYPE_I420,  FCC('I420'),  12, 1},
	{&MEDIASUBTYPE_IYUV,  FCC('IYUV'),  12, 1},
	{&MEDIASUBTYPE_ARGB32,BI_RGB,       32, 4},
	{&MEDIASUBTYPE_RGB32, BI_RGB,       32, 4},
	{&MEDIASUBTYPE_RGB24, BI_RGB,       24, 3},
	{&MEDIASUBTYPE_RGB565,BI_RGB,       16, 2},
	{&MEDIASUBTYPE_RGB555,BI_RGB,       16, 2},
	{&MEDIASUBTYPE_ARGB32,BI_BITFIELDS, 32, 4},
	{&MEDIASUBTYPE_RGB32, BI_BITFIELDS, 32, 4},
	{&MEDIASUBTYPE_RGB24, BI_BITFIELDS, 24, 3},
	{&MEDIASUBTYPE_RGB565,BI_BITFIELDS, 16, 2},
	{&MEDIASUBTYPE_RGB555,BI_BITFIELDS, 16, 2},
};

LPCWSTR MediaSubtype2String(const GUID& subtype);

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

	HANDLE m_hEvtTransform;

	HRESULT CopyBuffer(BYTE* pOut, BYTE* pIn, int w, int h, int pitchIn, const GUID& subtype, bool fInterlaced = false);

protected:
	void GetOutputFormats(int& nNumber, VIDEO_OUTPUT_FORMATS** ppFormats);
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
	std::vector<VIDEO_OUTPUT_FORMATS> m_VideoOutputFormats;

	HDC m_hdc;
	HBITMAP m_hbm;
	HFONT m_hfont;
	void PrintMessages(BYTE* pOut);

	/* ResX2 */
	std::unique_ptr<BYTE> m_pTempPicBuff;
	HRESULT Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black);

	// segment start time, absolute time
	CRefTime m_tPrev;
	REFERENCE_TIME CalcCurrentTime();

	double m_fps;

	// 3.x- versions of microsoft's mpeg4 codec output flipped image
	bool m_bMSMpeg4Fix;

	// don't set the "hide subtitles" stream until we are finished with loading
	bool m_bLoading;

	CString m_videoFileName;

	bool Open();

	int FindPreferedLanguage(bool fHideToo = true);
	void UpdatePreferedLanguages(CString lang);

	CCritSec m_csSubLock;
	CInterfaceList<ISubStream> m_pSubStreams;
	DWORD_PTR m_nSubtitleId;
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
	HANDLE m_hSystrayThread;
	SystrayIconData m_tbid;

	VIDEOINFOHEADER2 m_CurrentVIH2;

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
