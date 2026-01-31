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
#include <afxdlgs.h>
#include <atlpath.h>
#include "resource.h"
#include "Subtitles/VobSubFile.h"
#include "Subtitles/RTS.h"
#include "SubPic/MemSubPicEx.h"
#include "SubPic/SubPicQueueImpl.h"
#include "DSUtil/FileHandle.h"
#include "vfr.h"

#pragma warning(disable: 4706)

// Size of the char buffer according to VirtualDub Filters SDK doc
#define STRING_PROC_BUFFER_SIZE 128

const static uint16_t s_codepages[] = {
	0,
	1250,
	1251,
	1253,
	1252,
	1254,
	1255,
	1256,
	1257,
	1258,
	1361,
	874,
	932,
	936,
	949,
	950,
	54936,
};


//
// Generic interface
//

namespace Plugin
{

	class CFilter : public CAMThread, public CCritSec
	{
	private:
		CString m_fn;

	protected:
		float m_fps;
		CCritSec m_csSubLock;
		CComPtr<ISubPicQueue> m_pSubPicQueue;
		CComPtr<ISubPicProvider> m_pSubPicProvider;
		DWORD_PTR m_SubPicProviderId;

	public:
		CFilter() : m_fps(-1), m_SubPicProviderId(0) {
			CAMThread::Create();
		}
		virtual ~CFilter() {
			CAMThread::CallWorker(0);
		}

		CString GetFileName() {
			CAutoLock cAutoLock(this);
			return m_fn;
		}
		void SetFileName(CString fn) {
			CAutoLock cAutoLock(this);
			m_fn = fn;
		}

		bool Render(SubPicDesc& dst, REFERENCE_TIME rt, float fps) {
			if (!m_pSubPicProvider) {
				return false;
			}

			CSize size(dst.w, dst.h);

			if (!m_pSubPicQueue) {
				CComPtr<ISubPicAllocator> pAllocator = DNew CMemSubPicExAllocator(dst.type, size);

				HRESULT hr;
				if (!(m_pSubPicQueue = DNew CSubPicQueueNoThread(false, pAllocator, &hr)) || FAILED(hr)) {
					m_pSubPicQueue.Release();
					return false;
				}
			}

			if (m_SubPicProviderId != (DWORD_PTR)(ISubPicProvider*)m_pSubPicProvider) {
				m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
				m_SubPicProviderId = (DWORD_PTR)(ISubPicProvider*)m_pSubPicProvider;
			}

			CComPtr<ISubPic> pSubPic;
			if (!m_pSubPicQueue->LookupSubPic(rt, pSubPic)) {
				return false;
			}

			CRect r;
			pSubPic->GetDirtyRect(r);

			if (dst.type == MSP_RGB32 || dst.type == MSP_RGB24 || dst.type == MSP_RGB16 || dst.type == MSP_RGB15) {
				dst.h = -dst.h;
			}

			pSubPic->AlphaBlt(r, r, &dst);

			return true;
		}

		DWORD ThreadProc() {
			SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST);

			std::vector<HANDLE> handles;
			handles.push_back(GetRequestHandle());

			CString fn = GetFileName();
			CFileStatus fs;
			fs.m_mtime = 0;
			CFileGetStatus(fn, fs);

			for (;;) {
				DWORD i = WaitForMultipleObjects(handles.size(), handles.data(), FALSE, 1000);

				if (WAIT_OBJECT_0 == i) {
					Reply(S_OK);
					break;
				} else if (WAIT_OBJECT_0 + 1 >= i && i <= WAIT_OBJECT_0 + handles.size()) {
					if (FindNextChangeNotification(handles[i - WAIT_OBJECT_0])) {
						CFileStatus fs2;
						fs2.m_mtime = 0;
						CFileGetStatus(fn, fs2);

						if (fs.m_mtime < fs2.m_mtime) {
							fs.m_mtime = fs2.m_mtime;

							if (CComQIPtr<ISubStream> pSubStream = m_pSubPicProvider.p) {
								CAutoLock cAutoLock(&m_csSubLock);
								pSubStream->Reload();
							}
						}
					}
				} else if (WAIT_TIMEOUT == i) {
					CString fn2 = GetFileName();

					if (fn != fn2) {
						CPath p(fn2);
						p.RemoveFileSpec();
						HANDLE h = FindFirstChangeNotificationW(p, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
						if (h != INVALID_HANDLE_VALUE) {
							fn = fn2;
							handles.resize(1);
							handles.push_back(h);
						}
					}
				} else { // if(WAIT_ABANDONED_0 == i || WAIT_FAILED == i)
					break;
				}
			}

			m_hThread = 0;

			for (size_t i = 1; i < handles.size(); i++) {
				FindCloseChangeNotification(handles[i]);
			}

			return 0;
		}
	};

	class CVobSubFilter : virtual public CFilter
	{
	public:
		CVobSubFilter(CString fn = L"") {
			if (!fn.IsEmpty()) {
				Open(fn);
			}
		}

		bool Open(CString fn) {
			SetFileName(L"");
			m_pSubPicProvider.Release();

			if (CVobSubFile* vsf = DNew CVobSubFile(&m_csSubLock)) {
				m_pSubPicProvider = (ISubPicProvider*)vsf;
				if (vsf->Open(CString(fn))) {
					SetFileName(fn);
				} else {
					m_pSubPicProvider.Release();
				}
			}

			return !!m_pSubPicProvider;
		}
	};

	class CTextSubFilter : virtual public CFilter
	{
		UINT m_DefaultCodePage;

	public:
		CTextSubFilter(CString fn = L"", UINT codePage = CP_ACP, float fps = -1)
			: m_DefaultCodePage(ExpandCodePage(codePage))
		{
			m_fps = fps;
			if (!fn.IsEmpty()) {
				Open(fn, m_DefaultCodePage);
			}
		}

		int GetDefaultCodePage() {
			return(m_DefaultCodePage);
		}

		bool Open(CString fn, UINT codePage = CP_ACP) {
			SetFileName(L"");
			m_DefaultCodePage = ExpandCodePage(codePage);
			m_pSubPicProvider.Release();

			if (!m_pSubPicProvider) {
				if (CRenderedTextSubtitle* rts = DNew CRenderedTextSubtitle(&m_csSubLock)) {
					m_pSubPicProvider = (ISubPicProvider*)rts;
					if (rts->Open(CString(fn), m_DefaultCodePage, false, "", "")) {
						SetFileName(fn);
					} else {
						m_pSubPicProvider.Release();
					}
				}
			}

			return !!m_pSubPicProvider;
		}
	};

	//
	// VirtualDub new plugin interface sdk 1.1
	// https://www.virtualdub.org/filtersdk.html
	//
	namespace VirtualDubNew
	{
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>

		class CVirtualDubFilter : virtual public CFilter
		{
		public:
			CVirtualDubFilter() {}
			virtual ~CVirtualDubFilter() {}

			virtual int RunProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff) {
				SubPicDesc dst;
				dst.type = MSP_RGB32;
				dst.w = fa->src.w;
				dst.h = fa->src.h;
				dst.bpp = 32;
				dst.pitch = fa->src.pitch;
				dst.bits = (BYTE*)fa->src.data;

				Render(dst, 10000i64*fa->pfsi->lSourceFrameMS, (float)1000 / fa->pfsi->lMicrosecsPerFrame);

				return 0;
			}

			virtual long ParamProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff) {
				fa->dst.offset	= fa->src.offset;
				fa->dst.modulo	= fa->src.modulo;
				fa->dst.pitch	= fa->src.pitch;

				return 0;
			}

			virtual int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) = 0;
			virtual void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) = 0;
			virtual bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) = 0;
		};

		class CVobSubVirtualDubFilter : public CVobSubFilter, public CVirtualDubFilter
		{
		public:
			CVobSubVirtualDubFilter(CString fn = L"")
				: CVobSubFilter(fn) {}

			int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) {
				AFX_MANAGE_STATE(AfxGetStaticModuleState());

				CFileDialog fd(TRUE, nullptr, GetFileName(), OFN_EXPLORER|OFN_ENABLESIZING|OFN_HIDEREADONLY,
							   L"VobSub files (*.idx;*.sub)|*.idx;*.sub||", CWnd::FromHandle((HWND)hwnd), 0);

				if (fd.DoModal() != IDOK) {
					return 1;
				}

				return Open(fd.GetPathName()) ? 0 : 1;
			}

			void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) {
				CStringA fn = WStrToUTF8(GetFileName());
				if (fn.GetLength()) {
					_snprintf_s(str, STRING_PROC_BUFFER_SIZE, _TRUNCATE, " (%s)", fn.GetString());
				} else {
					strcpy_s(str, STRING_PROC_BUFFER_SIZE, " (empty)");
				}
			}

			bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) {
				CStringA fn = WStrToUTF8(GetFileName());
				fn.Replace("\\", "\\\\");
				_snprintf_s(buf, buflen, buflen, "Config(\"%s\")", fn.GetString());
				return true;
			}
		};

		class CTextSubVirtualDubFilter : public CTextSubFilter, public CVirtualDubFilter
		{
		public:
			CTextSubVirtualDubFilter(CString fn = L"", UINT codePage = CP_ACP)
				: CTextSubFilter(fn, codePage) {}

			int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) {
				AFX_MANAGE_STATE(AfxGetStaticModuleState());

#if 0 // The 'lpTemplateName' method has been disabled because it no longer works.
				const WCHAR formats[] = L"TextSub files (*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt)|*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt||";
				CFileDialog fd(TRUE, nullptr, GetFileName(), OFN_EXPLORER|OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_ENABLETEMPLATE|OFN_ENABLEHOOK,
							   formats, CWnd::FromHandle((HWND)hwnd), sizeof(OPENFILENAME));
				UINT_PTR CALLBACK OpenHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

				fd.m_pOFN->hInstance = AfxGetResourceHandle();
				fd.m_pOFN->lpTemplateName = MAKEINTRESOURCEW(IDD_TEXTSUBOPENTEMPLATE);
				fd.m_pOFN->lpfnHook = (LPOFNHOOKPROC)OpenHookProc;
				fd.m_pOFN->lCustData = (LPARAM)CP_ACP;

				if (fd.DoModal() != IDOK) {
					return 1;
				}

				return Open(fd.GetPathName(), fd.m_pOFN->lCustData) ? 0 : 1;

#else // This method works, but there are problems with displaying long text.
				const WCHAR formats[] = L"TextSub files (*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt)|*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt||";
				CFileDialog fd(TRUE, nullptr, GetFileName(), OFN_ENABLESIZING|OFN_HIDEREADONLY,
							   formats, CWnd::FromHandle((HWND)hwnd), sizeof(OPENFILENAME));

				fd.AddText(IDC_STATIC, L"Default code page:");
				fd.AddComboBox(IDC_COMBO1);

				CPINFOEX cpinfoex;
				for (const auto& codepage : s_codepages) {
					if (codepage == 0) {
						CStringW str;
						str.Format(L"System code page - %u", GetSystemCodePage());
						fd.AddControlItem(IDC_COMBO1, CP_ACP, str);
					}
					else if (GetCPInfoExW(codepage, 0, &cpinfoex)) {
						fd.AddControlItem(IDC_COMBO1, codepage, cpinfoex.CodePageName);
					}
				}
				fd.SetSelectedControlItem(IDC_COMBO1, CP_ACP);

				if (fd.DoModal() != IDOK) {
					return 1;
				}
				DWORD dwIDItem = 0;
				fd.GetSelectedControlItem(IDC_COMBO1, dwIDItem);

				return Open(fd.GetPathName(), dwIDItem) ? 0 : 1;
#endif
			}

			void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) {
				CStringA fn = WStrToUTF8(GetFileName());
				if (fn.GetLength()) {
					_snprintf_s(str, STRING_PROC_BUFFER_SIZE, _TRUNCATE, " (%s, %d)", fn.GetString(), GetDefaultCodePage());
				} else {
					strcpy_s(str, STRING_PROC_BUFFER_SIZE, " (empty)");
				}
			}

			bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) {
				CStringA fn = WStrToUTF8(GetFileName());
				fn.Replace("\\", "\\\\");
				_snprintf_s(buf, buflen, buflen, "Config(\"%s\", %d)", fn.GetString(), GetDefaultCodePage());
				return true;
			}
		};

		int vobsubInitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
		{
			return !(*(CVirtualDubFilter**)fa->filter_data = DNew CVobSubVirtualDubFilter());
		}

		int textsubInitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
		{
			return !(*(CVirtualDubFilter**)fa->filter_data = DNew CTextSubVirtualDubFilter());
		}

		void baseDeinitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			if (f) {
				delete f, f = nullptr;
			}
		}

		int baseRunProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			return f ? f->RunProc(fa, ff) : 1;
		}

		long baseParamProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			return f ? f->ParamProc(fa, ff) : 1;
		}

		int baseConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			return f ? f->ConfigProc(fa, ff, hwnd) : 1;
		}

		void baseStringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			if (f) {
				f->StringProc(fa, ff, str);
			}
		}

		bool baseFssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen)
		{
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			return f ? f->FssProc(fa, ff, buf, buflen) : false;
		}

		void vobsubScriptConfig(IVDXScriptInterpreter* isi, void* lpVoid, VDXScriptValue* argv, int argc)
		{
			VDXFilterActivation* fa = (VDXFilterActivation*)lpVoid;
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			if (f) {
				delete f;
			}
			f = DNew CVobSubVirtualDubFilter(UTF8ToWStr(*argv[0].asString()));
			*(CVirtualDubFilter**)fa->filter_data = f;
		}

		void textsubScriptConfig(IVDXScriptInterpreter* isi, void* lpVoid, VDXScriptValue* argv, int argc)
		{
			VDXFilterActivation* fa = (VDXFilterActivation*)lpVoid;
			CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
			if (f) {
				delete f;
			}
			f = DNew CTextSubVirtualDubFilter(UTF8ToWStr(*argv[0].asString()), argv[1].asInt());
			*(CVirtualDubFilter**)fa->filter_data = f;
		}

		VDXScriptFunctionDef vobsub_func_defs[]= {
			{ (VDXScriptFunctionPtr)vobsubScriptConfig, (char*)"Config", (char*)"0s" },
			{ nullptr },
		};

		VDXScriptObject vobsub_obj= {
			nullptr, vobsub_func_defs
		};

		struct VDXFilterDefinition filterDef_vobsub = {
			nullptr, nullptr, nullptr,	// next, prev, module
			"VobSub",					// name
			"Adds subtitles from a vob sequence.", // desc
			"MPC-BE Team",				// maker
			nullptr,					// private_data
			sizeof(CVirtualDubFilter**), // inst_data_size
			vobsubInitProc,				// initProc
			baseDeinitProc,				// deinitProc
			baseRunProc,				// runProc
			baseParamProc,				// paramProc
			baseConfigProc,				// configProc
			baseStringProc,				// stringProc
			nullptr,					// startProc
			nullptr,					// endProc
			&vobsub_obj,				// script_obj
			baseFssProc,				// fssProc
		};

		VDXScriptFunctionDef textsub_func_defs[]= {
			{ (VDXScriptFunctionPtr)textsubScriptConfig, (char*)"Config", (char*)"0si" },
			{ nullptr },
		};

		VDXScriptObject textsub_obj= {
			nullptr, textsub_func_defs
		};

		struct VDXFilterDefinition filterDef_textsub = {
			nullptr, nullptr, nullptr,	// next, prev, module
			"TextSub",					// name
			"Adds subtitles from srt, sub, psb, smi, ssa, ass file formats.", // desc
			"MPC-BE Team",				// maker
			nullptr,					// private_data
			sizeof(CVirtualDubFilter**), // inst_data_size
			textsubInitProc,			// initProc
			baseDeinitProc,				// deinitProc
			baseRunProc,				// runProc
			baseParamProc,				// paramProc
			baseConfigProc,				// configProc
			baseStringProc,				// stringProc
			nullptr,					// startProc
			nullptr,					// endProc
			&textsub_obj,				// script_obj
			baseFssProc,				// fssProc
		};

		static VDXFilterDefinition* fd_vobsub;
		static VDXFilterDefinition* fd_textsub;

		extern "C" __declspec(dllexport) int __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat)
		{
			if (!(fd_vobsub = ff->addFilter(fm, &filterDef_vobsub, sizeof(VDXFilterDefinition)))
					|| !(fd_textsub = ff->addFilter(fm, &filterDef_textsub, sizeof(VDXFilterDefinition)))) {
				return 1;
			}

			vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
			vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

			return 0;
		}

		extern "C" __declspec(dllexport) void __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff)
		{
			ff->removeFilter(fd_textsub);
			ff->removeFilter(fd_vobsub);
		}
	}

	//
	// Avisynth interface
	//
	// PF20180224 Avs2.6 interface using AVS+ headers
	namespace AviSynth26
	{
#include <avisynth/avisynth.h>

		static bool s_fSwapUV = false;

		class CAvisynthFilter : public GenericVideoFilter, virtual public CFilter
		{
		public:
			bool has_at_least_v8; // avs interface version check
			bool useRGBAwhenRGB32; // instead of old method: bool "RGBA" Avisynth variable. default false for TextSub, true for MaskSub

			VFRTranslator *vfr;

			CAvisynthFilter(PClip c, IScriptEnvironment* env, VFRTranslator *_vfr=0) : GenericVideoFilter(c), vfr(_vfr)
			{
				has_at_least_v8 = true;
				try { env->CheckVersion(8); }
				catch (const AvisynthError&) { has_at_least_v8 = false; }
			}

			PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
				PVideoFrame frame = child->GetFrame(n, env);

				SubPicDesc dst;
				dst.w = vi.width;
				dst.h = vi.height;

				dst.type =
					vi.IsRGB32() ? (env->GetVar("RGBA").AsBool() ? MSP_RGBA : MSP_RGB32) :
					vi.IsRGB24() ? MSP_RGB24 :
					vi.IsYUY2() ? MSP_YUY2 :
					/*vi.IsYV12()*/ vi.pixel_type == VideoInfo::CS_YV12 ? (s_fSwapUV ? MSP_IYUV : MSP_YV12) :
					/*vi.IsIYUV()*/ vi.pixel_type == VideoInfo::CS_IYUV ? (s_fSwapUV ? MSP_YV12 : MSP_IYUV) :
					-1;

				if (dst.type == -1) {
					env->ThrowError("Format not supported. Use RGB24, RGB32, YUY2, YV12.");
				}

				dst.bpp = frame->GetRowSize() / dst.w * 8;

				{
					// 8 bit classic
					env->MakeWritable(&frame);

					dst.pitch = frame->GetPitch();
					dst.pitchUV = frame->GetPitch(PLANAR_U); // n/a for RGB
					dst.bits = frame->GetWritePtr();
					dst.bitsU = frame->GetWritePtr(PLANAR_U); // n/a for RGB
					dst.bitsV = frame->GetWritePtr(PLANAR_V); // n/a for RGB
					if (vi.IsRGB32() && useRGBAwhenRGB32) {
						// MSP_RGB32 is flipped, MSP_RGBA is not flipped
						// but since both is RGB32 in Avisynth, it must be adjusted here
						dst.bits += (vi.height - 1) * dst.pitch;
						dst.pitch = -dst.pitch;
					}
				}

				// Common part
				float fps = m_fps > 0 ? m_fps : (float)vi.fps_numerator / vi.fps_denominator;

				REFERENCE_TIME timestamp;

				if (!vfr) {
					timestamp = (REFERENCE_TIME)(10000000i64 * n / fps);
				} else {
					timestamp = (REFERENCE_TIME)(10000000 * vfr->TimeStampFromFrameNumber(n));
				}

				Render(dst, timestamp, fps);

				return(frame);
			}
		};

		class CVobSubAvisynthFilter : public CVobSubFilter, public CAvisynthFilter
		{
		public:
			CVobSubAvisynthFilter(PClip c, const char* fn, IScriptEnvironment* env)
				: CVobSubFilter(UTF8ToWStr(fn))
				, CAvisynthFilter(c, env) {
				if (!m_pSubPicProvider) {
					env->ThrowError("VobSub: Can't open \"%s\"", fn);
				}
			}
		};

		AVSValue __cdecl VobSubCreateS(AVSValue args, void* user_data, IScriptEnvironment* env)
		{
			return(DNew CVobSubAvisynthFilter(args[0].AsClip(), args[1].AsString(), env));
		}

		class CTextSubAvisynthFilter : public CTextSubFilter, public CAvisynthFilter
		{
		public:
			CTextSubAvisynthFilter(PClip c, IScriptEnvironment* env, const char* fn, UINT codePage = CP_ACP, float fps = -1, VFRTranslator *vfr = 0) //vfr patch
				: CTextSubFilter(CString(fn), codePage, fps)
				, CAvisynthFilter(c, env, vfr) {
				if (!m_pSubPicProvider) {
					env->ThrowError("TextSub: Can't open \"%s\"", fn);
				}
			}
		};

		AVSValue __cdecl TextSubCreateGeneral(AVSValue args, void* user_data, IScriptEnvironment* env)
		{
			if (!args[1].Defined())
				env->ThrowError("TextSub: You must specify a subtitle file to use");
			VFRTranslator *vfr = 0;
			if (args[4].Defined()) {
				vfr = GetVFRTranslator(args[4].AsString());
			}

			return(DNew CTextSubAvisynthFilter(
					   args[0].AsClip(),
					   env,
					   args[1].AsString(),
					   args[2].AsInt(CP_ACP),
					   args[3].AsFloat(-1),
					   vfr));
		}

		AVSValue __cdecl TextSubSwapUV(AVSValue args, void* user_data, IScriptEnvironment* env)
		{
			s_fSwapUV = args[0].AsBool(false);
			return(AVSValue());
		}

		AVSValue __cdecl MaskSubCreate(AVSValue args, void* user_data, IScriptEnvironment* env)/*SIIFI*/
		{
			if (!args[0].Defined())
				env->ThrowError("MaskSub: You must specify a subtitle file to use");
			if (!args[3].Defined() && !args[6].Defined())
				env->ThrowError("MaskSub: You must specify either FPS or a VFR timecodes file");
			VFRTranslator *vfr = 0;
			if (args[6].Defined()) {
				vfr = GetVFRTranslator(args[6].AsString());
			}

			AVSValue rgb32("RGB32");
			AVSValue fps(args[3].AsFloat(25));
			AVSValue  tab[6] = {
				args[1],
				args[2],
				args[3],
				args[4],
				rgb32
			};
			AVSValue value(tab,5);
			const char * nom[5]= {
				"width",
				"height",
				"fps",
				"length",
				"pixel_type"
			};
			AVSValue clip(env->Invoke("Blackness",value,nom));
			env->SetVar(env->SaveString("RGBA"),true);
			//return(new CTextSubAvisynthFilter(clip.AsClip(), env, args[0].AsString()));
			return(DNew CTextSubAvisynthFilter(
					   clip.AsClip(),
					   env,
					   args[0].AsString(),
					   args[5].AsInt(CP_ACP),
					   args[3].AsFloat(-1),
					   vfr));
		}

		/* New 2.6 requirement!!! */
		// Declare and initialise server pointers static storage.
		const AVS_Linkage* AVS_linkage = 0;

		/* New 2.6 requirement!!! */
		// DLL entry point called from LoadPlugin() to setup a user plugin.
		extern "C" __declspec(dllexport) const char* __stdcall
			AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
		{
			/* New 2.6 requirement!!! */
			// Save the server pointers.
			AVS_linkage = vectors;
			env->AddFunction("VobSub", "cs", VobSubCreateS, 0);
			env->AddFunction("TextSub", "c[file]s[defcodepage]i[fps]f[vfr]s", TextSubCreateGeneral, 0);
			env->AddFunction("TextSubSwapUV", "b", TextSubSwapUV, 0);
			env->AddFunction("MaskSub", "[file]s[width]i[height]i[fps]f[length]i[defcodepage]i[vfr]s", MaskSubCreate, 0);
			env->SetVar(env->SaveString("RGBA"),false);
			return nullptr;
		}
	}
}

UINT_PTR CALLBACK OpenHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uiMsg) {
		case WM_NOTIFY: {
			OPENFILENAME* ofn = ((OFNOTIFY *)lParam)->lpOFN;

			if (((NMHDR *)lParam)->code == CDN_FILEOK) {
				ofn->lCustData = (LPARAM)s_codepages[SendMessageW(GetDlgItem(hDlg, IDC_COMBO1), CB_GETCURSEL, 0, 0)];
			}

			break;
		}

		case WM_INITDIALOG: {
			SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);

			CPINFOEX cpinfoex;
			for (const auto& codepage : s_codepages) {
				if (codepage == 0) {
					CStringW str;
					str.Format(L"System code page - %u", GetSystemCodePage());
					SendMessageW(GetDlgItem(hDlg, IDC_COMBO1), CB_ADDSTRING, 0, (LPARAM)str.GetString());
				}
				else if (GetCPInfoExW(codepage, 0, &cpinfoex)) {
					SendMessageW(GetDlgItem(hDlg, IDC_COMBO1), CB_ADDSTRING, 0, (LPARAM)(LPCWSTR)cpinfoex.CodePageName);
				}
			}

			break;
		}

		default:
			break;
	}

	return FALSE;
}
