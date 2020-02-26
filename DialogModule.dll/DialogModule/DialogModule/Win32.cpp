﻿/*

 MIT License

 Copyright © 2019 Samuel Venable

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include "DialogModule.h"

#include <windows.h>
#include <gdiplus.h>
#include <shobjidl.h>
#include <Commdlg.h>
#include <comdef.h>
#include <atlbase.h>
#include <activscp.h>
#include <Shlobj.h>

#include <cstdlib>
#include <cstdio>
#include <cwchar>

#include <vector>
#include <string>

using namespace Gdiplus;
using std::basic_string;
using std::to_string;
using std::wstring;
using std::string;
using std::vector;
using std::size_t;

#pragma comment(lib, "gdiplus.lib")

namespace dialog_module {

  namespace {

    // misc
    void *owner = NULL;
    bool init = false;
    HWND dlg = NULL;
    HWND win = NULL;
    string caption;
    string tstr_icon;

    // input boxes
    bool init_input = false;
    bool hide_input = false;
    HHOOK hook_handle = NULL;

    // get color
    string tstr_gctitle;
    wstring cpp_wstr_gctitle;

    // file dialogs
    wchar_t wstr_filter[512];
    wchar_t wstr_fname[4096];
    wstring cpp_wstr_dir;
    wstring cpp_wstr_title;

    wstring widen(string tstr) {
      size_t wchar_count = tstr.size() + 1;
      vector<wchar_t> buf(wchar_count);
      return wstring{ buf.data(), (size_t)MultiByteToWideChar(CP_UTF8, 0, tstr.c_str(), -1, buf.data(), (int)wchar_count) };
    }

    string narrow(wstring wstr) {
      int nbytes = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
      vector<char> buf(nbytes);
      return string{ buf.data(), (size_t)WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), buf.data(), nbytes, NULL, NULL) };
    }

    HWND owner_window() {
      win = owner ? (HWND)owner : GetActiveWindow();
      return win;
    }

    int show_message_helper(char *str, bool cancelable) {
      string tstr = str; wstring wstr = widen(tstr);

      string title = (caption == "") ? (cancelable ? "Question" : "Information") : caption;
      wstring wtitle = widen(title);

      UINT flags = MB_DEFBUTTON1 | MB_APPLMODAL;
      flags |= cancelable ? (MB_OKCANCEL | MB_ICONQUESTION) : (MB_OK | MB_ICONINFORMATION);

      int result = MessageBoxW(owner_window(), wstr.c_str(), wtitle.c_str(), flags);
      return cancelable ? ((result == IDOK) ? 1 : -1) : 1;
    }

    int show_question_helper(char *str, bool cancelable) {
      string tstr = str; wstring wstr = widen(tstr);

      string title = (caption == "") ? "Question" : caption;
      wstring wtitle = widen(title);

      UINT flags = MB_DEFBUTTON1 | MB_APPLMODAL | MB_ICONQUESTION;
      flags |= cancelable ? MB_YESNOCANCEL : MB_YESNO;

      int result = MessageBoxW(owner_window(), wstr.c_str(), wtitle.c_str(), flags);
      return cancelable ? ((result == IDYES) ? 1 : ((result == IDNO) ? 0 : -1)) : (result == IDYES);
    }

    int show_error_helper(char *str, bool abort, bool attempt) {
      string tstr = str; wstring wstr = widen(tstr);

      string title = (caption == "") ? "Error" : caption;
      wstring wtitle = widen(title);

      if (attempt) {
        UINT flags = MB_RETRYCANCEL | MB_ICONERROR | MB_DEFBUTTON1 | MB_APPLMODAL;
        int result = MessageBoxW(owner_window(), wstr.c_str(), wtitle.c_str(), flags);
        return (result == IDRETRY) ? 0 : -1;
      }

      UINT flags = MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_DEFBUTTON1 | MB_APPLMODAL;
      int result = MessageBoxW(owner_window(), wstr.c_str(), wtitle.c_str(), flags);
      result = abort ? 1 : ((result == IDABORT) ? 1 : ((result == IDRETRY) ? 0 : -1));

      if (result == 1) exit(0);
      return result;
    }

    class CSimpleScriptSite :
      public IActiveScriptSite,
      public IActiveScriptSiteWindow {
    public:
      CSimpleScriptSite() : m_cRefCount(1), m_hWnd(NULL) { }

      // IUnknown
    
      STDMETHOD_(ULONG, AddRef)();
      STDMETHOD_(ULONG, Release)();
      STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);

      // IActiveScriptSite

      STDMETHOD(GetLCID)(LCID *plcid) { *plcid = 0; return S_OK; }
      STDMETHOD(GetItemInfo)(LPCOLESTR pstrName, DWORD dwReturnMask, IUnknown **ppiunkItem, ITypeInfo **ppti) { return TYPE_E_ELEMENTNOTFOUND; }
      STDMETHOD(GetDocVersionString)(BSTR *pbstrVersion) { *pbstrVersion = SysAllocString(L"1.0"); return S_OK; }
      STDMETHOD(OnScriptTerminate)(const VARIANT *pvarResult, const EXCEPINFO *pexcepinfo) { return S_OK; }
      STDMETHOD(OnStateChange)(SCRIPTSTATE ssScriptState) { return S_OK; }
      STDMETHOD(OnScriptError)(IActiveScriptError *pIActiveScriptError) { return S_OK; }
      STDMETHOD(OnEnterScript)(void) { return S_OK; }
      STDMETHOD(OnLeaveScript)(void) { return S_OK; }

      // IActiveScriptSiteWindow

      STDMETHOD(GetWindow)(HWND *phWnd) { *phWnd = m_hWnd; return S_OK; }
      STDMETHOD(EnableModeless)(BOOL fEnable) { return S_OK; }

      // Miscellaneous

      STDMETHOD(SetWindow)(HWND hWnd) { m_hWnd = hWnd; return S_OK; }

    public:
      LONG m_cRefCount;
      HWND m_hWnd;
    };

    STDMETHODIMP_(ULONG) CSimpleScriptSite::AddRef() {
      return InterlockedIncrement(&m_cRefCount);
    }

    STDMETHODIMP_(ULONG) CSimpleScriptSite::Release() {
      if (!InterlockedDecrement(&m_cRefCount))
      {
        delete this;
        return 0;
      }
      return m_cRefCount;
    }

    STDMETHODIMP CSimpleScriptSite::QueryInterface(REFIID riid, void **ppvObject) {
      if (riid == IID_IUnknown || riid == IID_IActiveScriptSiteWindow) {
        *ppvObject = (IActiveScriptSiteWindow *)this;
        AddRef();
        return NOERROR;
      }
      if (riid == IID_IActiveScriptSite) {
        *ppvObject = (IActiveScriptSite *)this;
        AddRef();
        return NOERROR;
      }
      return E_NOINTERFACE;
    }

    HICON GetIcon(HWND hwnd) {
      HICON icon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
      if (icon == NULL)
        icon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
      if (icon == NULL)
        icon = LoadIcon(GetModuleHandleW(NULL), MAKEINTRESOURCE(0));
      if (icon == NULL)
        icon = LoadIcon(NULL, IDI_APPLICATION);
      return icon;
    }

    LRESULT CALLBACK InputBoxProc(int nCode, WPARAM wParam, LPARAM lParam) {
      if (nCode < HC_ACTION)
        return CallNextHookEx(hook_handle, nCode, wParam, lParam);

      if (nCode == HCBT_CREATEWND) {
        CBT_CREATEWNDW *cbtcr = (CBT_CREATEWNDW *)lParam;
        if (win != (HWND)wParam && cbtcr->lpcs->hwndParent == win) {
          dlg = (HWND)wParam;
          init_input = true;
        }
      }

      if (dlg != NULL) {
        if (nCode == HCBT_ACTIVATE && init_input == true) {
          RECT wrect; GetWindowRect(dlg, &wrect);
          unsigned width = wrect.right - wrect.left;
          unsigned height = wrect.bottom - wrect.top;
          int xpos = wrect.left - (width / 2);
          int ypos = wrect.top - (height / 2);
          MoveWindow(dlg, xpos, ypos, width, height, true);
          if (hide_input == true)
            SendDlgItemMessageW(dlg, 1000, EM_SETPASSWORDCHAR, L'●', 0);
          init_input = false;
        }
        wstring cpp_wstr_icon = widen(tstr_icon);
        if (PathFileExistsW(cpp_wstr_icon.c_str())) {
          HICON hIcon;
          Bitmap *png = Bitmap::FromFile(cpp_wstr_icon.c_str());
          png->GetHICON(&hIcon);
          PostMessage(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
          DeleteObject(hIcon);
          delete png;
        } else {
          HICON hIcon = GetIcon(win);
          PostMessage(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
          DeleteObject(hIcon);
        }
      }
      return CallNextHookEx(hook_handle, nCode, wParam, lParam);
    }

    LRESULT CALLBACK DialogProc(int nCode, WPARAM wParam, LPARAM lParam) {
      if (nCode < HC_ACTION)
        return CallNextHookEx(hook_handle, nCode, wParam, lParam);

      if (nCode == HCBT_CREATEWND) {
        CBT_CREATEWNDW *cbtcr = (CBT_CREATEWNDW *)lParam;
        if (win != (HWND)wParam && cbtcr->lpcs->hwndParent == win) {
          dlg = (HWND)wParam;
          init = true;
        }
      }

      if (dlg != NULL) {
        if (nCode == HCBT_ACTIVATE && init == true) {
          RECT wrect1; GetWindowRect(dlg, &wrect1);
          RECT wrect2; GetWindowRect(win, &wrect2);
          unsigned width1 = wrect1.right - wrect1.left;
          unsigned height1 = wrect1.bottom - wrect1.top;
          unsigned width2 = wrect2.right - wrect2.left;
          unsigned height2 = wrect2.bottom - wrect2.top;
          int xpos = wrect2.left + (width2 / 2) - (width1 / 2);
          int ypos = wrect2.top + (height2 / 2) - (height1 / 2);
          MoveWindow(dlg, xpos, ypos, width1, height1, true);
          init = false;
        }
        wstring cpp_wstr_icon = widen(tstr_icon);
        if (PathFileExistsW(cpp_wstr_icon.c_str())) {
          HICON hIcon;
          Bitmap *png = Bitmap::FromFile(cpp_wstr_icon.c_str());
          png->GetHICON(&hIcon);
          PostMessage(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
          DeleteObject(hIcon);
          delete png;
        } else {
          HICON hIcon = GetIcon(win);
          PostMessage(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
          DeleteObject(hIcon);
        }
      }
      return CallNextHookEx(hook_handle, nCode, wParam, lParam);
    }

    string string_replace_all(string str, string substr, string newstr) {
      size_t pos = 0;
      const size_t sublen = substr.length(), newlen = newstr.length();

      while ((pos = str.find(substr, pos)) != string::npos) {
        str.replace(pos, sublen, newstr);
        pos += newlen;
      }

      return str;
    }

    char *InputBox(char *Prompt, char *Title, char *Default) {
      HRESULT hr = S_OK;
      hr = CoInitialize(NULL);

      // Initialize
      CSimpleScriptSite *pScriptSite = new CSimpleScriptSite();
      CComPtr<IActiveScript> spVBScript;
      CComPtr<IActiveScriptParse> spVBScriptParse;
      HWND parent_window = owner_window();
      hr = pScriptSite->SetWindow(parent_window);
      hr = spVBScript.CoCreateInstance(OLESTR("VBScript"));
      hr = spVBScript->SetScriptSite(pScriptSite);
      hr = spVBScript->QueryInterface(&spVBScriptParse);
      hr = spVBScriptParse->InitNew();

      // Replace quotes with double quotes
      string strPrompt = string_replace_all(Prompt, "\"", "\"\"");
      string strTitle = string_replace_all(Title, "\"", "\"\"");
      string strDefault = string_replace_all(Default, "\"", "\"\"");

      // Dialog position
      RECT wrect; GetWindowRect(parent_window, &wrect);
      RECT crect; GetWindowRect(parent_window, &crect);
      string XPos = to_string(((wrect.left + crect.right) / 2) * 15);
      string YPos = to_string(((wrect.top + crect.bottom) / 2) * 15);

      // Create evaluation string
      string Evaluation = "InputBox(\"" + strPrompt + "\", \"" + strTitle + "\", \"" + strDefault + "\", " + XPos + ", " + YPos + ")";
      Evaluation = string_replace_all(Evaluation, "\r\n", "\" + vbNewLine + \"");
      wstring WideEval = widen(Evaluation);

      // Run InpuBox
      CComVariant result;
      EXCEPINFO ei = {};

      DWORD ThreadID = GetCurrentThreadId();
      HINSTANCE ModHwnd = GetModuleHandle(NULL);
      hook_handle = SetWindowsHookEx(WH_CBT, &InputBoxProc, ModHwnd, ThreadID);
      hr = spVBScriptParse->ParseScriptText(WideEval.c_str(), NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &result, &ei);
      UnhookWindowsHookEx(hook_handle);

      // Cleanup
      spVBScriptParse = NULL;
      spVBScript = NULL;
      pScriptSite->Release();
      pScriptSite = NULL;

      ::CoUninitialize();
      static string strResult;
      _bstr_t bstrResult = (_bstr_t)result;
      strResult = narrow((wchar_t *)bstrResult);
      return (char *)strResult.c_str();
    }

    char *get_string_helper(char *str, char *def, bool hidden) {
      hide_input = hidden; string title = (caption == "") ? "Input Query" : caption;
      return InputBox(str, (char *)title.c_str(), def);
    }

    string remove_trailing_zeros(double numb) {
      string strnumb = to_string(numb);

      while (!strnumb.empty() && strnumb.find('.') != string::npos && (strnumb.back() == '.' || strnumb.back() == '0'))
        strnumb.pop_back();

      return strnumb;
    }

    double get_integer_helper(char *str, double def, bool hidden) {
      double DIGITS_MIN = -999999999999999; 
      double DIGITS_MAX = 999999999999999;

      if (def < DIGITS_MIN) def = DIGITS_MIN; 
      if (def > DIGITS_MAX) def = DIGITS_MAX;

      string cpp_tdef = remove_trailing_zeros(def);
      double result = strtod(get_string_helper(str, (char *)cpp_tdef.c_str(), hidden), NULL);

      if (result < DIGITS_MIN) result = DIGITS_MIN; 
      if (result > DIGITS_MAX) result = DIGITS_MAX;

      return result;
    }

    string add_slash(const string& dir) {
      if (dir.empty() || *dir.rbegin() != '\\') return dir + '\\';
      return dir;
    }

    string remove_slash(string dir) {
      while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
        dir.pop_back();
      return dir;
    }

    UINT_PTR CALLBACK GetColorProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
      if (uiMsg == WM_INITDIALOG) {
        if (tstr_gctitle != "")
          SetWindowTextW(hdlg, cpp_wstr_gctitle.c_str());
        PostMessageW(hdlg, WM_SETFOCUS, 0, 0);
      }
      return false;
    }

    OPENFILENAMEW get_filename_or_filenames_helper(string filter, string fname, string dir, string title, DWORD flags) {
      OPENFILENAMEW ofn;

      filter = filter.append("||");
      fname = remove_slash(fname);

      wstring cpp_wstr_filter = widen(filter);
      wstring cpp_wstr_fname = widen(fname);
      cpp_wstr_dir = widen(dir);
      cpp_wstr_title = widen(title);

      wcsncpy_s(wstr_filter, cpp_wstr_filter.c_str(), 512);
      wcsncpy_s(wstr_fname, cpp_wstr_fname.c_str(), 4096);

      int i = 0;
      while (wstr_filter[i] != '\0') {
        if (wstr_filter[i] == '|') {
          wstr_filter[i] = '\0';
        }
        i += 1;
      }

      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = owner_window();
      ofn.lpstrFile = wstr_fname;
      ofn.nMaxFile = 4096;
      ofn.lpstrFilter = wstr_filter;
      ofn.nMaxCustFilter = 512;
      ofn.nFilterIndex = 0;
      ofn.lpstrTitle = cpp_wstr_title.c_str();
      ofn.lpstrInitialDir = cpp_wstr_dir.c_str();
      ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | flags;

      return ofn;
    }

    string get_open_filename_helper(string filter, string fname, string dir, string title) {
      OPENFILENAMEW ofn;
      ofn = get_filename_or_filenames_helper(filter, fname, dir, title, 0);

      if (GetOpenFileNameW(&ofn) != 0)
        return narrow(wstr_fname);

      return "";
    }

    string get_open_filenames_helper(string filter, string fname, string dir, string title) {
      OPENFILENAMEW ofn;
      ofn = get_filename_or_filenames_helper(filter, fname, dir, title, OFN_ALLOWMULTISELECT);

      if (GetOpenFileNameW(&ofn) != 0) {
        wstring cpp_wstr_fname1 = wstr_fname;
        cpp_wstr_fname1 += '\\';

        size_t pos = 0;
        size_t prevlen = pos;
        size_t len = wcslen(wstr_fname);

        while (pos < len) {
          if (wstr_fname[len - 1] != '\n' && wstr_fname[len] == '\0')
            wstr_fname[len] = '\n';

          prevlen = len;
          len = wcslen(wstr_fname);
          pos += (len - prevlen) + 1;
        }

        wstring cpp_wstr_fname2 = wstr_fname;
        if (cpp_wstr_fname2[len - 1] == '\n') cpp_wstr_fname2[len - 1] = '\0';
        if (cpp_wstr_fname2[len - 2] == '\\') cpp_wstr_fname2[len - 2] = '\0';
        cpp_wstr_fname2 = widen(string_replace_all(narrow(cpp_wstr_fname2), narrow(L"\n"), narrow(L"\n" + cpp_wstr_fname1)));
        size_t rm = cpp_wstr_fname2.find_first_of(L'\n');

        if (rm != string::npos)
          cpp_wstr_fname2 = cpp_wstr_fname2.substr(rm + 1, cpp_wstr_fname2.length() - (rm + 1));

        cpp_wstr_fname2.append(L"\0");
        if (cpp_wstr_fname2.length() >= 4095) {
          cpp_wstr_fname2 = cpp_wstr_fname2.substr(0, 4095);
          size_t end = cpp_wstr_fname2.find_last_of(L"\n");
          cpp_wstr_fname2 = cpp_wstr_fname2.substr(0, end);
          cpp_wstr_fname2.append(L"\0");
        }

        return string_replace_all(narrow(cpp_wstr_fname2), "\\\\", "\\");
      }

      return "";
    }

    string get_save_filename_helper(string filter, string fname, string dir, string title) {
      OPENFILENAMEW ofn;
      ofn = get_filename_or_filenames_helper(filter, fname, dir, title, OFN_OVERWRITEPROMPT);

      if (GetSaveFileNameW(&ofn) != 0)
        return narrow(wstr_fname);

      return "";
    }

    string get_directory_helper(string dname, string title) {
      IFileDialog *selectDirectory;
      CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&selectDirectory));

      DWORD options;
      selectDirectory->GetOptions(&options);
      selectDirectory->SetOptions(options | FOS_PICKFOLDERS | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);

      wstring cpp_wstr_dname = widen(dname);
      LPWSTR szFilePath = (wchar_t *)cpp_wstr_dname.c_str();

      if (dname == "") {
        wchar_t buffer[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, buffer)) {
          szFilePath = buffer;
        }
      }

      IShellItem *pItem = nullptr;
      HRESULT hr = ::SHCreateItemFromParsingName(szFilePath, nullptr, IID_PPV_ARGS(&pItem));

      if (SUCCEEDED(hr)) {
        LPWSTR szName = nullptr;
        hr = pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &szName);
        if (SUCCEEDED(hr)) {
          selectDirectory->SetFolder(pItem);
          CoTaskMemFree(szName);
        }
        pItem->Release();
      }

      if (title == "") title = "Select Directory";
      wstring cpp_wstr_capt = widen(title);

      selectDirectory->SetOkButtonLabel(L"Select");
      selectDirectory->SetTitle(cpp_wstr_capt.c_str());
      selectDirectory->Show(owner_window());

      pItem = nullptr;
      hr = selectDirectory->GetResult(&pItem);

      if (SUCCEEDED(hr)) {
        LPWSTR wstr_result;
        pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &wstr_result);
        pItem->Release();

        return add_slash(narrow(wstr_result));
      }

      return "";
    }

    int get_color_helper(int defcol, string title) {
      CHOOSECOLORW cc;

      COLORREF DefColor = defcol;
      static COLORREF CustColors[16];

      tstr_gctitle = title;
      cpp_wstr_gctitle = widen(tstr_gctitle);

      ZeroMemory(&cc, sizeof(cc));
      cc.lStructSize = sizeof(CHOOSECOLORW);
      cc.hwndOwner = owner_window();
      cc.rgbResult = DefColor;
      cc.lpCustColors = CustColors;
      cc.Flags = CC_RGBINIT | CC_ENABLEHOOK;
      cc.lpfnHook = GetColorProc;

      return (ChooseColorW(&cc) != 0) ? cc.rgbResult : -1;
    }

  } // anonymous namespace

  int show_message(char *str) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_message_helper(str, false);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int show_message_cancelable(char *str) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_message_helper(str, true);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int show_question(char *str) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_question_helper(str, false);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int show_question_cancelable(char *str) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_question_helper(str, true);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int show_attempt(char *str) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_error_helper(str, false, true);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int show_error(char *str, bool abort) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = show_error_helper(str, abort, false);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  char *get_string(char *str, char *def) {
    return get_string_helper(str, def, false);
  }

  char *get_password(char *str, char *def) {
    return get_string_helper(str, def, true);
  }

  double get_integer(char *str, double def) {
    return get_integer_helper(str, def, false);
  }

  double get_passcode(char *str, double def) {
    return get_integer_helper(str, def, true);
  }

  char *get_open_filename(char *filter, char *fname) {
    string str_filter = filter; string str_fname = fname; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_open_filename_helper(str_filter, str_fname, "", "");
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
    string str_filter = filter; string str_fname = fname; 
    string str_dir = dir; string str_title = title; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_open_filename_helper(str_filter, str_fname, str_dir, str_title);
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_open_filenames(char *filter, char *fname) {
    string str_filter = filter; string str_fname = fname; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_open_filenames_helper(str_filter, str_fname, "", "");
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
    string str_filter = filter; string str_fname = fname;
    string str_dir = dir; string str_title = title; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_open_filenames_helper(str_filter, str_fname, str_dir, str_title);
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_save_filename(char *filter, char *fname) {
    string str_filter = filter; string str_fname = fname; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_save_filename_helper(str_filter, str_fname, "", "");
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
    string str_filter = filter; string str_fname = fname;
    string str_dir = dir; string str_title = title; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_save_filename_helper(str_filter, str_fname, str_dir, str_title);
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_directory(char *dname) {
    string str_dname = dname;  static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_directory_helper(str_dname, "");
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  char *get_directory_alt(char *capt, char *root) {
    string str_dname = root; string str_title = capt; static string result;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    result = get_directory_helper(str_dname, str_title);
    UnhookWindowsHookEx(hook_handle);
    return (char *)result.c_str();
  }

  int get_color(int defcol) {
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = get_color_helper(defcol, "");
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  int get_color_ext(int defcol, char *title) {
    string str_title = title;
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(NULL);
    hook_handle = SetWindowsHookEx(WH_CBT, &DialogProc, ModHwnd, ThreadID);
    int result = get_color_helper(defcol, str_title);
    UnhookWindowsHookEx(hook_handle);
    return result;
  }

  char *widget_get_caption() {
    return (char *)caption.c_str();
  }

  void widget_set_caption(char *str) {
    caption = str;
  }

  void *widget_get_owner() {
    return owner;
  }

  void widget_set_owner(void *hwnd) {
    owner = hwnd;
  }

  char *widget_get_icon() {
    wchar_t wstr_icon[MAX_PATH];
    wstring cpp_wstr_icon = widen(tstr_icon);
    GetFullPathNameW(cpp_wstr_icon.c_str(), MAX_PATH, wstr_icon, NULL);
    static string tstr_result; tstr_result = narrow(wstr_icon);
    return (char *)tstr_result.c_str();
  }

  void widget_set_icon(char *icon) {
    wchar_t wstr_icon[MAX_PATH];
    wstring cpp_wstr_icon = widen(icon);
    GetFullPathNameW(cpp_wstr_icon.c_str(), MAX_PATH, wstr_icon, NULL);
    if (PathFileExistsW(wstr_icon)) tstr_icon = icon;
  }

  char *widget_get_system() {
    return (char *)"Win32";
  }

  void widget_set_system(char *sys) {

  }

} // namespace dialog_module
