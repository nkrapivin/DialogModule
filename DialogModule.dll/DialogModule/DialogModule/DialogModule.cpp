/******************************************************************************

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

******************************************************************************/

#include "DialogModule.h"
#include <windows.h>
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
#include <algorithm>
using namespace std;
using std::size_t;

typedef basic_string<wchar_t> tstring;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#pragma warning(disable: 4047)
HINSTANCE hInstance = (HINSTANCE)&__ImageBase;
#pragma warning(default: 4047)

#define DIGITS_MAX 999999999999999
#define MONITOR_CENTER 0x0001

static string message_caption;
static tstring dialog_caption = L"";
static tstring error_caption = L"";
static bool message_cancel = false;
static bool question_cancel = false;

static HHOOK hHook = 0;
static bool HideInput = 0;

static string str_cctitle;
static tstring tstr_cctitle;

static tstring widen(string str) {
  size_t wchar_count = str.size() + 1;

  vector<wchar_t> buf(wchar_count);

  return tstring{ buf.data(), (size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), wchar_count) };
}

static string shorten(tstring str) {
  int nbytes = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);

  vector<char> buf(nbytes);

  return string{ buf.data(), (size_t)WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), buf.data(), nbytes, NULL, NULL) };
}

static HWND window_handle() {
  return GetAncestor(GetActiveWindow(), GA_ROOTOWNER);
}

static string string_replace_all(string str, string substr, string newstr) {
  size_t pos = 0;
  const size_t sublen = substr.length(), newlen = newstr.length();

  while ((pos = str.find(substr, pos)) != string::npos) {
    str.replace(pos, sublen, newstr);
    pos += newlen;
  }

  return str;
}

static tstring tstring_replace_all(tstring str, tstring substr, tstring newstr) {
  size_t pos = 0;
  const size_t sublen = substr.length(), newlen = newstr.length();

  while ((pos = str.find(substr, pos)) != tstring::npos) {
    str.replace(pos, sublen, newstr);
    pos += newlen;
  }

  return str;
}

static string remove_trailing_zeros(double numb) {
  string strnumb = std::to_string(numb);

  while (!strnumb.empty() && strnumb.find('.') != string::npos && (strnumb.back() == '.' || strnumb.back() == '0'))
    strnumb.pop_back();

  return strnumb;
}

static string CPPNewLineToVBSNewLine(string NewLine) {
  size_t pos = 0;

  while (pos < NewLine.length()) {
    if (NewLine[pos] == '\n' || NewLine[pos] == '\r')
      NewLine.replace(pos, 2, "\" + vbNewLine + \"");

    pos += 1;
  }

  return NewLine;
}

static void CenterRectToMonitor(LPRECT prc, UINT flags) {
  HMONITOR hMonitor;
  MONITORINFO mi;
  RECT        rc;
  int         w = prc->right - prc->left;
  int         h = prc->bottom - prc->top;

  hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hMonitor, &mi);
  rc = mi.rcMonitor;

  if (flags & MONITOR_CENTER) {
    prc->left = rc.left + (rc.right - rc.left - w) / 2;
    prc->top = rc.top + (rc.bottom - rc.top - h) / 2;
    prc->right = prc->left + w;
    prc->bottom = prc->top + h;
  }
  else {
    prc->left = rc.left + (rc.right - rc.left - w) / 2;
    prc->top = rc.top + (rc.bottom - rc.top - h) / 3;
    prc->right = prc->left + w;
    prc->bottom = prc->top + h;
  }
}

static void CenterWindowToMonitor(HWND hwnd, UINT flags) {
  RECT rc;
  GetWindowRect(hwnd, &rc);
  CenterRectToMonitor(&rc, flags);
  SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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

static LRESULT CALLBACK InputBoxProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < HC_ACTION)
    return CallNextHookEx(hHook, nCode, wParam, lParam);

  if (nCode == HCBT_ACTIVATE) {
    if (HideInput == true) {
      HWND TextBox = FindWindowEx((HWND)wParam, NULL, "Edit", NULL);
      SendDlgItemMessage((HWND)wParam, GetDlgCtrlID(TextBox), EM_SETPASSWORDCHAR, '*', 0);
    }
  }

  if (nCode == HCBT_CREATEWND) {
    if (!(GetWindowLongPtr((HWND)wParam, GWL_STYLE) & WS_CHILD))
      SetWindowLongPtr((HWND)wParam, GWL_EXSTYLE, GetWindowLongPtr((HWND)wParam, GWL_EXSTYLE) | WS_EX_DLGMODALFRAME);
  }

  return CallNextHookEx(hHook, nCode, wParam, lParam);
}

static char *InputBox(char *Prompt, char *Title, char *Default) {
  HRESULT hr = S_OK;
  hr = CoInitialize(NULL);

  // Initialize
  CSimpleScriptSite *pScriptSite = new CSimpleScriptSite();
  CComPtr<IActiveScript> spVBScript;
  CComPtr<IActiveScriptParse> spVBScriptParse;
  hr = spVBScript.CoCreateInstance(OLESTR("VBScript"));
  hr = spVBScript->SetScriptSite(pScriptSite);
  hr = spVBScript->QueryInterface(&spVBScriptParse);
  hr = spVBScriptParse->InitNew();

  // Replace quotes with double quotes
  string strPrompt = string_replace_all(Prompt, "\"", "\"\"");
  string strTitle = string_replace_all(Title, "\"", "\"\"");
  string strDefault = string_replace_all(Default, "\"", "\"\"");

  // Create evaluation string
  string Evaluation = "InputBox(\"" + strPrompt + "\", \"" + strTitle + "\", \"" + strDefault + "\")";
  Evaluation = CPPNewLineToVBSNewLine(Evaluation);
  tstring WideEval = widen(Evaluation);

  // Run InpuBox
  CComVariant result;
  EXCEPINFO ei = {};

  DWORD ThreadID = GetCurrentThreadId();
  HINSTANCE ModHwnd = GetModuleHandle(NULL);
  hr = pScriptSite->SetWindow(window_handle());
  hHook = SetWindowsHookEx(WH_CBT, &InputBoxProc, ModHwnd, ThreadID);
  hr = spVBScriptParse->ParseScriptText(WideEval.c_str(), NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &result, &ei);
  UnhookWindowsHookEx(hHook);


  // Cleanup
  spVBScriptParse = NULL;
  spVBScript = NULL;
  pScriptSite->Release();
  pScriptSite = NULL;

  ::CoUninitialize();
  static string strResult;
  _bstr_t bstrResult = (_bstr_t)result;
  strResult = shorten((wchar_t *)bstrResult);
  return (char *)strResult.c_str();
}

static UINT_PTR CALLBACK CCHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
  if (uiMsg == WM_INITDIALOG) {
    CenterWindowToMonitor(hdlg, 0);
    PostMessageW(hdlg, WM_SETFOCUS, 0, 0);
  }

  return false;
}

static UINT_PTR CALLBACK CCExtHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
  if (uiMsg == WM_INITDIALOG) {
    CenterWindowToMonitor(hdlg, 0);
    if (str_cctitle != "")
      SetWindowTextW(hdlg, tstr_cctitle.c_str());
    PostMessageW(hdlg, WM_SETFOCUS, 0, 0);
  }

  return false;
}

static string remove_slash(string dir) {
  while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
    dir.pop_back();

  return dir;
}

static double show_message_helperfunc(char *str) {
  string strStr = str;
  tstring tstrStr = widen(strStr);

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  if (message_cancel) {
    int result = MessageBoxW(window_handle(), tstrStr.c_str(), wstrWindowCaption, MB_OKCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON1 | MB_APPLMODAL);
    if (result == IDOK) return 1; else return -1;
  }

  MessageBoxW(window_handle(), tstrStr.c_str(), wstrWindowCaption, MB_OK | MB_ICONINFORMATION | MB_DEFBUTTON1 | MB_APPLMODAL);
  return 1;
}

static double show_question_helperfunc(char *str) {
  string strStr = str;
  tstring tstrStr = widen(strStr);

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  int result;
  if (question_cancel) {
    result = MessageBoxW(window_handle(), tstrStr.c_str(), wstrWindowCaption, MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_APPLMODAL);
    if (result == IDYES) return 1; else if (result == IDNO) return 0; else return -1;
  }

  result = MessageBoxW(window_handle(), tstrStr.c_str(), wstrWindowCaption, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_APPLMODAL);
  if (result == IDYES) return 1;
  return 0;
}

double show_message(char *str) {
  message_cancel = false;
  return show_message_helperfunc(str);
}

double show_message_cancelable(char *str) {
  message_cancel = true;
  return show_message_helperfunc(str);
}

double show_question(char *str) {
  question_cancel = false;
  return show_question_helperfunc(str);
}

double show_question_cancelable(char *str) {
  question_cancel = true;
  return show_question_helperfunc(str);
}

double show_attempt(char *str) {
  string strStr = str;
  string strWindowCaption = "Error";

  tstring tstrStr = widen(strStr);
  tstring tstrWindowCaption = widen(strWindowCaption);

  if (error_caption != L"")
    tstrWindowCaption = error_caption;

  int result;
  result = MessageBoxW(window_handle(), tstrStr.c_str(), tstrWindowCaption.c_str(), MB_RETRYCANCEL | MB_ICONERROR | MB_DEFBUTTON1 | MB_APPLMODAL);
  if (result == IDRETRY) return 0;
  return -1;
}

double show_error(char *str, double abort) {
  string strStr = str;
  string strWindowCaption = "Error";

  tstring tstrStr = widen(strStr);
  tstring tstrWindowCaption = widen(strWindowCaption);

  if (error_caption != L"")
    tstrWindowCaption = error_caption;

  int result;
  result = MessageBoxW(window_handle(), tstrStr.c_str(), tstrWindowCaption.c_str(), MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_DEFBUTTON1 | MB_APPLMODAL);
  if (result == IDABORT || abort) quick_exit(0);
  if (result == IDRETRY) return 0;
  return -1;
}

char *get_string(char *str, char *def) {
  HideInput = 0;

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  message_caption = shorten(wstrWindowCaption);

  char *result = InputBox(str, (char *)message_caption.c_str(), def);
  return result;
}

char *get_password(char *str, char *def) {
  HideInput = 1;

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  message_caption = shorten(wstrWindowCaption);

  char *result = InputBox(str, (char *)message_caption.c_str(), def);
  return result;
}

double get_integer(char *str, double def) {
  string strStr = str;
  if (def < -DIGITS_MAX) def = -DIGITS_MAX;
  if (def > DIGITS_MAX) def = DIGITS_MAX;
  string strDef = remove_trailing_zeros(def);

  HideInput = 0;

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  message_caption = shorten(wstrWindowCaption);

  char *result = InputBox(str, (char *)message_caption.c_str(), (char *)strDef.c_str());
  if (strtod(result, NULL) < -DIGITS_MAX) return -DIGITS_MAX;
  if (strtod(result, NULL) > DIGITS_MAX) return DIGITS_MAX;
  return strtod(result, NULL);
}

double get_passcode(char *str, double def) {
  string strStr = str;
  if (def < -DIGITS_MAX) def = -DIGITS_MAX;
  if (def > DIGITS_MAX) def = DIGITS_MAX;
  string strDef = remove_trailing_zeros(def);

  HideInput = 1;

  wchar_t wstrWindowCaption[512];
  GetWindowTextW(window_handle(), wstrWindowCaption, 512);

  if (dialog_caption != L"")
    wcsncpy_s(wstrWindowCaption, dialog_caption.c_str(), 512);

  message_caption = shorten(wstrWindowCaption);

  char *result = InputBox(str, (char *)message_caption.c_str(), (char *)strDef.c_str());
  if (strtod(result, NULL) < -DIGITS_MAX) return -DIGITS_MAX;
  if (strtod(result, NULL) > DIGITS_MAX) return DIGITS_MAX;
  return strtod(result, NULL);
}

char *get_open_filename(char *filter, char *fname) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);

  wchar_t wstr_fname[MAX_PATH];
  wcsncpy_s(wstr_fname, tstr_fname.c_str(), MAX_PATH);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = NULL;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

  if (GetOpenFileNameW(&ofn) != 0) {
    static string result;
    result = shorten(wstr_fname);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);
  string str_dir = dir;
  string str_title = title;

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);
  tstring tstr_dir = widen(str_dir);
  tstring tstr_title = widen(str_title);

  wchar_t wstr_fname[MAX_PATH];
  wcsncpy_s(wstr_fname, tstr_fname.c_str(), MAX_PATH);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = tstr_title.c_str();
  ofn.lpstrInitialDir = tstr_dir.c_str();
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

  if (GetOpenFileNameW(&ofn) != 0) {
    static string result;
    result = shorten(wstr_fname);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_open_filenames(char *filter, char *fname) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);

  wchar_t wstr_fname1[4096];
  wcsncpy_s(wstr_fname1, tstr_fname.c_str(), 4096);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname1;
  ofn.nMaxFile = 4096;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = NULL;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;

  if (GetOpenFileNameW(&ofn) != 0) {
    tstring tstr_fname1 = wstr_fname1;
    tstr_fname1 += '\\';

    size_t pos = 0;
    size_t prevlen = pos;
    size_t len = wcslen(wstr_fname1);

    while (pos < len) {
      if (wstr_fname1[len - 1] != '\n' && wstr_fname1[len] == '\0')
        wstr_fname1[len] = '\n';

      prevlen = len;
      len = wcslen(wstr_fname1);
      pos += (len - prevlen) + 1;
    }

    tstring tstr_fname2 = wstr_fname1;
    if (tstr_fname2[len - 1] == '\n') tstr_fname2[len - 1] = '\0';
    if (tstr_fname2[len - 2] == '\\') tstr_fname2[len - 2] = '\0';
    tstr_fname2 = tstring_replace_all(tstr_fname2, L"\n", L"\n" + tstr_fname1);
    size_t rm = tstr_fname2.find_first_of(L'\n');

    if (rm != string::npos)
      tstr_fname2 = tstr_fname2.substr(rm + 1, tstr_fname2.length() - (rm + 1));

    tstr_fname2.append(L"\0");
    if (tstr_fname2.length() >= 4095) {
      tstr_fname2 = tstr_fname2.substr(0, 4095);
      size_t end = tstr_fname2.find_last_of(L"\n");
      tstr_fname2 = tstr_fname2.substr(0, end);
      tstr_fname2.append(L"\0");
    }

    static string result;
    tstr_fname2 = tstring_replace_all(tstr_fname2, L"\\\\", L"\\");
    result = shorten(tstr_fname2);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);
  string str_dir = dir;
  string str_title = title;

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);
  tstring tstr_dir = widen(str_dir);
  tstring tstr_title = widen(str_title);

  wchar_t wstr_fname1[4096];
  wcsncpy_s(wstr_fname1, tstr_fname.c_str(), 4096);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname1;
  ofn.nMaxFile = 4096;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = tstr_title.c_str();
  ofn.lpstrInitialDir = tstr_dir.c_str();
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;

  if (GetOpenFileNameW(&ofn) != 0) {
    tstring tstr_fname1 = wstr_fname1;
    tstr_fname1 += '\\';

    size_t pos = 0;
    size_t prevlen = pos;
    size_t len = wcslen(wstr_fname1);

    while (pos < len) {
      if (wstr_fname1[len - 1] != '\n' && wstr_fname1[len] == '\0')
        wstr_fname1[len] = '\n';

      prevlen = len;
      len = wcslen(wstr_fname1);
      pos += (len - prevlen) + 1;
    }

    tstring tstr_fname2 = wstr_fname1;
    if (tstr_fname2[len - 1] == '\n') tstr_fname2[len - 1] = '\0';
    if (tstr_fname2[len - 2] == '\\') tstr_fname2[len - 2] = '\0';
    tstr_fname2 = tstring_replace_all(tstr_fname2, L"\n", L"\n" + tstr_fname1);
    size_t rm = tstr_fname2.find_first_of(L'\n');

    if (rm != string::npos)
      tstr_fname2 = tstr_fname2.substr(rm + 1, tstr_fname2.length() - (rm + 1));

    tstr_fname2.append(L"\0");
    if (tstr_fname2.length() >= 4096) {
      tstr_fname2 = tstr_fname2.substr(0, 4095);
      size_t end = tstr_fname2.find_last_of(L"\n");
      tstr_fname2 = tstr_fname2.substr(0, end);
      tstr_fname2.append(L"\0");
    }

    static string result;
    tstr_fname2 = tstring_replace_all(tstr_fname2, L"\\\\", L"\\");
    result = shorten(tstr_fname2);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_save_filename(char *filter, char *fname) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);

  wchar_t wstr_fname[MAX_PATH];
  wcsncpy_s(wstr_fname, tstr_fname.c_str(), MAX_PATH);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = NULL;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

  if (GetSaveFileNameW(&ofn) != 0) {
    static string result;
    result = shorten(wstr_fname);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  OPENFILENAMEW ofn;

  string str_filter = string(filter).append("||");
  string str_fname = remove_slash(fname);
  string str_dir = dir;
  string str_title = title;

  tstring tstr_filter = widen(str_filter);
  replace(tstr_filter.begin(), tstr_filter.end(), '|', '\0');
  tstring tstr_fname = widen(str_fname);
  tstring tstr_dir = widen(str_dir);
  tstring tstr_title = widen(str_title);

  wchar_t wstr_fname[MAX_PATH];
  wcsncpy_s(wstr_fname, tstr_fname.c_str(), MAX_PATH);

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_handle();
  ofn.lpstrFile = wstr_fname;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = tstr_filter.c_str();
  ofn.nFilterIndex = 0;
  ofn.lpstrTitle = tstr_title.c_str();
  ofn.lpstrInitialDir = tstr_dir.c_str();
  ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

  if (GetSaveFileNameW(&ofn) != 0) {
    static string result;
    result = shorten(wstr_fname);
    return (char *)result.c_str();
  }

  return (char *)"";
}

char *get_directory(char *dname) {
  IFileDialog *SelectDirectory;
  CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&SelectDirectory));

  DWORD options;
  SelectDirectory->GetOptions(&options);
  SelectDirectory->SetOptions(options | FOS_PICKFOLDERS | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);

  string str_dname = dname;
  tstring tstr_dname = widen(str_dname);
  LPWSTR szFilePath = (wchar_t *)tstr_dname.c_str();

  IShellItem *pItem = nullptr;
  HRESULT hr = ::SHCreateItemFromParsingName(szFilePath, nullptr, IID_PPV_ARGS(&pItem));

  if (SUCCEEDED(hr)) {
    LPWSTR szName = nullptr;
    hr = pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &szName);
    if (SUCCEEDED(hr)) {
      SelectDirectory->SetFolder(pItem);
      ::CoTaskMemFree(szName);
    }
    pItem->Release();
  }

  SelectDirectory->SetOkButtonLabel(L"Select");
  SelectDirectory->SetTitle(L"Select Directory");
  SelectDirectory->Show(window_handle());

  pItem = nullptr;
  hr = SelectDirectory->GetResult(&pItem);

  if (SUCCEEDED(hr)) {
    LPWSTR wstr_result;
    pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &wstr_result);
    pItem->Release();

    static string str_result;
    str_result = string_replace_all(shorten(wstr_result) + "\\", "\\\\", "\\");
    return (char *)str_result.c_str();
  }

  return (char *)"";
}

char *get_directory_alt(char *capt, char *root) {
  IFileDialog *SelectDirectory;
  CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&SelectDirectory));

  DWORD options;
  SelectDirectory->GetOptions(&options);
  SelectDirectory->SetOptions(options | FOS_PICKFOLDERS | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);

  string str_dname = root;
  tstring tstr_dname = widen(str_dname);
  LPWSTR szFilePath = (wchar_t *)tstr_dname.c_str();

  IShellItem* pItem = nullptr;
  HRESULT hr = ::SHCreateItemFromParsingName(szFilePath, nullptr, IID_PPV_ARGS(&pItem));

  if (SUCCEEDED(hr)) {
    LPWSTR szName = nullptr;
    hr = pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &szName);
    if (SUCCEEDED(hr)) {
      SelectDirectory->SetFolder(pItem);
      ::CoTaskMemFree(szName);
    }
    pItem->Release();
  }

  string str_capt;
  if (capt != "") str_capt = capt;
  else str_capt = "Select Directory";
  tstring tstr_capt = widen(str_capt);

  SelectDirectory->SetOkButtonLabel(L"Select");
  SelectDirectory->SetTitle(tstr_capt.c_str());
  SelectDirectory->Show(window_handle());

  pItem = nullptr;
  hr = SelectDirectory->GetResult(&pItem);

  if (SUCCEEDED(hr)) {
    LPWSTR wstr_result;
    pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &wstr_result);
    pItem->Release();

    static string str_result;
    str_result = string_replace_all(shorten(wstr_result) + "\\", "\\\\", "\\");
    return (char *)str_result.c_str();
  }

  return (char *)"";
}

double get_color(double defcol) {
  CHOOSECOLORW cc;

  COLORREF DefColor = (int)defcol;
  static COLORREF CustColors[16];

  ZeroMemory(&cc, sizeof(cc));
  cc.lStructSize = sizeof(CHOOSECOLORW);
  cc.hwndOwner = window_handle();
  cc.rgbResult = DefColor;
  cc.lpCustColors = CustColors;
  cc.Flags = CC_RGBINIT | CC_ENABLEHOOK;
  cc.lpfnHook = CCHookProc;

  if (ChooseColorW(&cc) != 0)
    return (int)cc.rgbResult;

  return -1;
}

double get_color_ext(double defcol, char *title) {
  CHOOSECOLORW cc;

  COLORREF DefColor = (int)defcol;
  static COLORREF CustColors[16];

  str_cctitle = title;
  tstr_cctitle = widen(str_cctitle);

  ZeroMemory(&cc, sizeof(cc));
  cc.lStructSize = sizeof(CHOOSECOLORW);
  cc.hwndOwner = window_handle();
  cc.rgbResult = DefColor;
  cc.lpCustColors = CustColors;
  cc.Flags = CC_RGBINIT | CC_ENABLEHOOK;
  cc.lpfnHook = CCExtHookProc;

  if (ChooseColorW(&cc) != 0)
    return (int)cc.rgbResult;

  return -1;
}

char *message_get_caption() {
  static string str_caption;

  if (dialog_caption == L"") {
    wchar_t wstrWindowCaption[512];
    GetWindowTextW(window_handle(), wstrWindowCaption, 512);
    str_caption = shorten(wstrWindowCaption);
    return (char *)str_caption.c_str();
  }

  if (error_caption == L"") error_caption = L"Error";
  if (dialog_caption == widen(str_caption) && error_caption == L"Error")
    return (char *)"";

  str_caption = shorten(dialog_caption);
  return (char *)str_caption.c_str();
}

char *message_set_caption(char *str) {
  static string str_caption;
  string str_str = str;

  if (str_str != "") dialog_caption = widen(str_str);
  else dialog_caption = L"";

  if (str_str != "") error_caption = widen(str_str);
  else error_caption = L"Error";

  if (dialog_caption == L"" && error_caption == L"Error")
    return (char *)"";

  str_caption = shorten(dialog_caption);
  return (char *)str_caption.c_str();
}

char *widget_get_system() {
  return (char *)"Win32";
}

char *widget_set_system(char *sys) {
  return (char *)"Win32";
}
