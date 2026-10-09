#ifndef BL_WINMINI_H
#define BL_WINMINI_H
/* Minimal Windows x64 ABI declarations. Only documented OS APIs are used.
   This keeps the reproducible cross-build independent of proprietary SDK files. */
#include <stdint.h>
#include <stddef.h>
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define API __declspec(dllimport)
typedef void *HANDLE;typedef HANDLE HWND;typedef HANDLE HINSTANCE;typedef HANDLE HMODULE;
typedef HANDLE HICON;typedef HANDLE HCURSOR;typedef HANDLE HBRUSH;typedef HANDLE HFONT;typedef HANDLE HGDIOBJ;
typedef HANDLE HDC;typedef HANDLE HBITMAP;typedef HANDLE HMENU;typedef HANDLE HTREEITEM;typedef HANDLE HIMAGELIST;
typedef HANDLE HGLOBAL;typedef uint16_t WCHAR;typedef const WCHAR *LPCWSTR;typedef WCHAR *LPWSTR;
typedef uint32_t DWORD;typedef int32_t LONG;typedef uint16_t WORD;typedef uint8_t BYTE;typedef int BOOL;
typedef uint32_t UINT;typedef uintptr_t UINT_PTR;typedef uintptr_t WPARAM;typedef intptr_t LPARAM;typedef intptr_t LRESULT;typedef intptr_t LONG_PTR;
typedef uint32_t COLORREF;typedef uint16_t ATOM;
typedef union {struct {DWORD LowPart;LONG HighPart;};int64_t QuadPart;} LARGE_INTEGER;
typedef struct {DWORD dwLowDateTime,dwHighDateTime;} FILETIME;
typedef struct {LONG x,y;} POINT;typedef struct {LONG cx,cy;} SIZE;
typedef struct {LONG left,top,right,bottom;} RECT;
typedef LRESULT (CALLBACK *WNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef struct {UINT cbSize,style;WNDPROC lpfnWndProc;int cbClsExtra,cbWndExtra;HINSTANCE hInstance;HICON hIcon;HCURSOR hCursor;HBRUSH hbrBackground;LPCWSTR lpszMenuName,lpszClassName;HICON hIconSm;} WNDCLASSEXW;
typedef struct {HWND hwnd;UINT message;WPARAM wParam;LPARAM lParam;DWORD time;POINT pt;DWORD lPrivate;} MSG;
typedef struct {HDC hdc;BOOL fErase;RECT rcPaint;BOOL fRestore,fIncUpdate;BYTE rgbReserved[32];} PAINTSTRUCT;
typedef struct {DWORD dwSize,dwICC;} INITCOMMONCONTROLSEX;
typedef struct {HWND hwndFrom;UINT_PTR idFrom;UINT code;} NMHDR;
typedef struct {UINT mask;HTREEITEM hItem;UINT state,stateMask;LPWSTR pszText;int cchTextMax,iImage,iSelectedImage,cChildren;LPARAM lParam;} TVITEMW;
typedef struct {HTREEITEM hParent,hInsertAfter;TVITEMW item;} TVINSERTSTRUCTW;
typedef struct {NMHDR hdr;UINT action;TVITEMW itemOld,itemNew;POINT ptDrag;} NMTREEVIEWW;
typedef struct {UINT mask;int iItem,iSubItem;UINT state,stateMask;LPWSTR pszText;int cchTextMax,iImage;LPARAM lParam;int iIndent,iGroupId;UINT cColumns;UINT *puColumns;int *piColFmt;int iGroup;} LVITEMW;
typedef struct {NMHDR hdr;LVITEMW item;} NMLVDISPINFOW;
typedef struct {NMHDR hdr;int iItem,iSubItem;UINT uNewState,uOldState,uChanged;POINT ptAction;LPARAM lParam;} NMLISTVIEW;
typedef struct {UINT mask;int fmt,cx;LPWSTR pszText;int cchTextMax,iSubItem,iImage,iOrder,cxMin,cxDefault,cxIdeal;} LVCOLUMNW;
typedef struct {NMHDR hdr;DWORD dwDrawStage;HDC hdc;RECT rc;UINT_PTR dwItemSpec;UINT uItemState;LPARAM lItemlParam;} NMCUSTOMDRAW;
typedef struct {NMCUSTOMDRAW nmcd;COLORREF clrText,clrTextBk;int iSubItem;} NMLVCUSTOMDRAW;
typedef struct {UINT mask;DWORD dwState,dwStateMask;LPWSTR pszText;int cchTextMax,iImage;LPARAM lParam;} TCITEMW;
typedef struct {UINT CtlType,CtlID,itemID,itemAction,itemState;HWND hwndItem;HDC hDC;RECT rcItem;UINT_PTR itemData;} DRAWITEMSTRUCT;
typedef struct {LONG bmType,bmWidth,bmHeight,bmWidthBytes;WORD bmPlanes,bmBitsPixel;void *bmBits;} BITMAP;
typedef struct {DWORD lStructSize;HWND hwndOwner;HINSTANCE hInstance;LPCWSTR lpstrFilter;LPWSTR lpstrCustomFilter;DWORD nMaxCustFilter,nFilterIndex;LPWSTR lpstrFile;DWORD nMaxFile;LPWSTR lpstrFileTitle;DWORD nMaxFileTitle;LPCWSTR lpstrInitialDir,lpstrTitle;DWORD Flags;WORD nFileOffset,nFileExtension;LPCWSTR lpstrDefExt;LPARAM lCustData;void *lpfnHook;LPCWSTR lpTemplateName;void *pvReserved;DWORD dwReserved,FlagsEx;} OPENFILENAMEW;
typedef struct {POINT ptReserved,ptMaxSize,ptMaxPosition,ptMinTrackSize,ptMaxTrackSize;} MINMAXINFO;
typedef struct {void *lpCreateParams;HINSTANCE hInstance;HMENU hMenu;HWND hwndParent;int cy,cx,y,x;LONG style;LPCWSTR lpszName,lpszClass;DWORD dwExStyle;} CREATESTRUCTW;
#define NULL_HANDLE ((HANDLE)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAKEINTRESOURCEW(i) ((LPWSTR)(uintptr_t)(WORD)(i))
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r))|((WORD)(BYTE)(g)<<8)|((DWORD)(BYTE)(b)<<16)))
#define LOWORD(v) ((WORD)((uintptr_t)(v)&0xffff))
#define HIWORD(v) ((WORD)(((uintptr_t)(v)>>16)&0xffff))
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_BEGIN 0
#define FILE_END 2
#define WS_OVERLAPPEDWINDOW 0x00cf0000u
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define WS_TABSTOP 0x00010000u
#define WS_BORDER 0x00800000u
#define WS_VSCROLL 0x00200000u
#define WS_HSCROLL 0x00100000u
#define WS_CLIPCHILDREN 0x02000000u
#define WS_CLIPSIBLINGS 0x04000000u
#define WS_EX_CLIENTEDGE 0x00000200u
#define WS_EX_CONTROLPARENT 0x00010000u
#define ES_MULTILINE 4
#define ES_AUTOVSCROLL 0x40
#define ES_AUTOHSCROLL 0x80
#define ES_READONLY 0x800
#define ES_NUMBER 0x2000
#define BS_OWNERDRAW 0x0b
#define CBS_DROPDOWNLIST 3
#define CBS_HASSTRINGS 0x200
#define TVS_HASBUTTONS 1
#define TVS_HASLINES 2
#define TVS_LINESATROOT 4
#define TVS_SHOWSELALWAYS 0x20
#define LVS_REPORT 1
#define LVS_SINGLESEL 4
#define LVS_SHOWSELALWAYS 8
#define LVS_OWNERDATA 0x1000
#define LVS_EX_GRIDLINES 1
#define LVS_EX_FULLROWSELECT 0x20
#define LVS_EX_DOUBLEBUFFER 0x10000
#define WM_CREATE 1
#define WM_DESTROY 2
#define WM_SIZE 5
#define WM_SETFOCUS 7
#define WM_CLOSE 0x10
#define WM_QUIT 0x12
#define WM_PAINT 0x0f
#define WM_ERASEBKGND 0x14
#define WM_GETMINMAXINFO 0x24
#define WM_DRAWITEM 0x2b
#define WM_SETFONT 0x30
#define WM_GETFONT 0x31
#define WM_NOTIFY 0x4e
#define WM_COMMAND 0x111
#define WM_TIMER 0x113
#define WM_KEYDOWN 0x100
#define WM_SYSKEYDOWN 0x104
#define WM_CTLCOLOREDIT 0x133
#define WM_CTLCOLORLISTBOX 0x134
#define WM_CTLCOLORSTATIC 0x138
#define WM_DROPFILES 0x233
#define WM_DPICHANGED 0x2e0
#define WM_APP 0x8000
#define EN_CHANGE 0x300
#define CBN_SELCHANGE 1
#define SW_SHOW 5
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SWP_NOZORDER 4
#define SWP_NOACTIVATE 0x10
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_ICONINFORMATION 0x40
#define MB_ICONWARNING 0x30
#define MB_YESNO 4
#define MB_DEFBUTTON2 0x100
#define IDYES 6
#define GWLP_USERDATA (-21)
#define COLOR_WINDOW 5
#define COLOR_BTNFACE 15
#define TRANSPARENT 1
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_WORDBREAK 0x10
#define DT_SINGLELINE 0x20
#define DT_NOPREFIX 0x800
#define DT_END_ELLIPSIS 0x8000
#define DEFAULT_GUI_FONT 17
#define IDC_ARROW 32512
#define IMAGE_BITMAP 0
#define IMAGE_ICON 1
#define LR_DEFAULTCOLOR 0
#define LR_DEFAULTSIZE 0x40
#define LR_CREATEDIBSECTION 0x2000
#define SRCCOPY 0x00cc0020
#define HALFTONE 4
#define DI_NORMAL 3
#define PS_SOLID 0
#define OFN_OVERWRITEPROMPT 2
#define OFN_HIDEREADONLY 4
#define OFN_NOCHANGEDIR 8
#define OFN_PATHMUSTEXIST 0x800
#define OFN_FILEMUSTEXIST 0x1000
#define OFN_EXPLORER 0x80000
#define CF_UNICODETEXT 13
#define GMEM_MOVEABLE 2
#define VK_CONTROL 0x11
#define VK_SHIFT 0x10
#define VK_ESCAPE 0x1b
#define VK_RETURN 0x0d
#define VK_F5 0x74
#define VK_LEFT 0x25
#define VK_RIGHT 0x27
#define VK_MENU 0x12
#define TV_FIRST 0x1100
#define TVM_INSERTITEMW (TV_FIRST+50)
#define TVM_DELETEITEM (TV_FIRST+1)
#define TVM_EXPAND (TV_FIRST+2)
#define TVM_SELECTITEM (TV_FIRST+11)
#define TVM_SETIMAGELIST (TV_FIRST+9)
#define TVM_SETBKCOLOR (TV_FIRST+29)
#define TVM_SETTEXTCOLOR (TV_FIRST+30)
#define TVM_SETITEMHEIGHT (TV_FIRST+27)
#define TVIF_TEXT 1
#define TVIF_IMAGE 2
#define TVIF_PARAM 4
#define TVIF_SELECTEDIMAGE 0x20
#define TVI_ROOT ((HTREEITEM)(intptr_t)-65536)
#define TVI_LAST ((HTREEITEM)(intptr_t)-65534)
#define TVE_EXPAND 2
#define TVGN_CARET 9
#define TVN_SELCHANGEDW ((UINT)-451)
#define LVM_FIRST 0x1000
#define LVM_SETITEMCOUNT (LVM_FIRST+47)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST+54)
#define LVM_INSERTCOLUMNW (LVM_FIRST+97)
#define LVM_DELETECOLUMN (LVM_FIRST+28)
#define LVM_SETIMAGELIST (LVM_FIRST+3)
#define LVM_SETBKCOLOR (LVM_FIRST+1)
#define LVM_SETTEXTCOLOR (LVM_FIRST+36)
#define LVM_SETTEXTBKCOLOR (LVM_FIRST+38)
#define LVM_GETNEXTITEM (LVM_FIRST+12)
#define LVM_SETITEMSTATE (LVM_FIRST+43)
#define LVM_ENSUREVISIBLE (LVM_FIRST+19)
#define LVCF_FMT 1
#define LVCF_WIDTH 2
#define LVCF_TEXT 4
#define LVCF_SUBITEM 8
#define LVIF_TEXT 1
#define LVIS_SELECTED 2
#define LVIS_FOCUSED 1
#define LVNI_SELECTED 2
#define LVSICF_NOSCROLL 2
#define LVN_GETDISPINFOW ((UINT)-177)
#define LVN_ITEMCHANGED ((UINT)-101)
#define NM_DBLCLK ((UINT)-3)
#define NM_CUSTOMDRAW ((UINT)-12)
#define CDDS_PREPAINT 1
#define CDDS_ITEMPREPAINT 0x10001
#define CDRF_DODEFAULT 0
#define CDRF_NOTIFYITEMDRAW 0x20
#define CDRF_NEWFONT 2
#define TCM_FIRST 0x1300
#define TCM_INSERTITEMW (TCM_FIRST+62)
#define TCM_GETCURSEL (TCM_FIRST+11)
#define TCM_SETCURSEL (TCM_FIRST+12)
#define TCIF_TEXT 1
#define TCN_SELCHANGE ((UINT)-551)
#define CB_ADDSTRING 0x143
#define CB_RESETCONTENT 0x14b
#define CB_SETCURSEL 0x14e
#define CB_GETCURSEL 0x147
#define CB_SETDROPPEDWIDTH 0x160
#define EM_SETSEL 0xb1
#define EM_SETLIMITTEXT 0xc5
#define EM_SETCUEBANNER 0x1501
#define PBM_SETRANGE32 (0x400+6)
#define PBM_SETPOS (0x400+2)
#define ODS_SELECTED 1
#define ODS_GRAYED 2
#define ODS_DISABLED 4
#define ODS_FOCUS 0x10
#define ILC_COLOR32 0x20
#define ILC_MASK 1
API void WINAPI ExitProcess(UINT);
API HMODULE WINAPI GetModuleHandleW(LPCWSTR);
API LPWSTR WINAPI GetCommandLineW(void);
API DWORD WINAPI GetLastError(void);
API HANDLE WINAPI CreateFileW(LPCWSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
API BOOL WINAPI CloseHandle(HANDLE);
API BOOL WINAPI ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
API BOOL WINAPI WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
API BOOL WINAPI SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD);
API BOOL WINAPI GetFileSizeEx(HANDLE,LARGE_INTEGER*);
API BOOL WINAPI GetFileTime(HANDLE,FILETIME*,FILETIME*,FILETIME*);
API BOOL WINAPI CreateDirectoryW(LPCWSTR,void*);
API BOOL WINAPI DeleteFileW(LPCWSTR);
API BOOL WINAPI MoveFileExW(LPCWSTR,LPCWSTR,DWORD);
API uint64_t WINAPI GetTickCount64(void);
API BOOL WINAPI FlushFileBuffers(HANDLE);
API int WINAPI MultiByteToWideChar(UINT,DWORD,const char*,int,LPWSTR,int);
API int WINAPI WideCharToMultiByte(UINT,DWORD,LPCWSTR,int,char*,int,const char*,BOOL*);
API DWORD WINAPI GetFullPathNameW(LPCWSTR,DWORD,LPWSTR,LPWSTR*);
API DWORD WINAPI GetEnvironmentVariableW(LPCWSTR,LPWSTR,DWORD);
API DWORD WINAPI GetModuleFileNameW(HMODULE,LPWSTR,DWORD);
API DWORD WINAPI GetTempPathW(DWORD,LPWSTR);
API DWORD WINAPI GetCurrentProcessId(void);
API HANDLE WINAPI CreateThread(void*,size_t,DWORD (WINAPI*)(void*),void*,DWORD,DWORD*);
API void WINAPI Sleep(DWORD);
API HGLOBAL WINAPI GlobalAlloc(UINT,size_t);
API void *WINAPI GlobalLock(HGLOBAL);
API BOOL WINAPI GlobalUnlock(HGLOBAL);
API HGLOBAL WINAPI GlobalFree(HGLOBAL);
API HANDLE WINAPI LocalFree(HANDLE);
API ATOM WINAPI RegisterClassExW(const WNDCLASSEXW*);
API HWND WINAPI CreateWindowExW(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,void*);
API LRESULT WINAPI DefWindowProcW(HWND,UINT,WPARAM,LPARAM);
API BOOL WINAPI DestroyWindow(HWND);
API BOOL WINAPI ShowWindow(HWND,int);
API BOOL WINAPI UpdateWindow(HWND);
API BOOL WINAPI GetMessageW(MSG*,HWND,UINT,UINT);
API BOOL WINAPI TranslateMessage(const MSG*);
API LRESULT WINAPI DispatchMessageW(const MSG*);
API void WINAPI PostQuitMessage(int);
API LRESULT WINAPI SendMessageW(HWND,UINT,WPARAM,LPARAM);
API BOOL WINAPI PostMessageW(HWND,UINT,WPARAM,LPARAM);
API BOOL WINAPI GetClientRect(HWND,RECT*);
API BOOL WINAPI GetWindowRect(HWND,RECT*);
API BOOL WINAPI MoveWindow(HWND,int,int,int,int,BOOL);
API BOOL WINAPI SetWindowPos(HWND,HWND,int,int,int,int,UINT);
API BOOL WINAPI SetWindowTextW(HWND,LPCWSTR);
API int WINAPI GetWindowTextW(HWND,LPWSTR,int);
API int WINAPI GetWindowTextLengthW(HWND);
API BOOL WINAPI EnableWindow(HWND,BOOL);
API BOOL WINAPI IsWindow(HWND);
API BOOL WINAPI IsDialogMessageW(HWND,MSG*);
API HWND WINAPI SetFocus(HWND);
API HWND WINAPI GetFocus(void);
API short WINAPI GetKeyState(int);
API HCURSOR WINAPI LoadCursorW(HINSTANCE,LPCWSTR);
API HICON WINAPI LoadIconW(HINSTANCE,LPCWSTR);
API HANDLE WINAPI LoadImageW(HINSTANCE,LPCWSTR,UINT,int,int,UINT);
API BOOL WINAPI DrawIconEx(HDC,int,int,HICON,int,int,UINT,HBRUSH,UINT);
API int WINAPI FillRect(HDC,const RECT*,HBRUSH);
API int WINAPI FrameRect(HDC,const RECT*,HBRUSH);
API int WINAPI DrawTextW(HDC,LPCWSTR,int,RECT*,UINT);
API UINT_PTR WINAPI SetTimer(HWND,UINT_PTR,UINT,void*);
API BOOL WINAPI KillTimer(HWND,UINT_PTR);
API BOOL WINAPI InvalidateRect(HWND,const RECT*,BOOL);
API HDC WINAPI BeginPaint(HWND,PAINTSTRUCT*);
API BOOL WINAPI EndPaint(HWND,const PAINTSTRUCT*);
API int WINAPI MessageBoxW(HWND,LPCWSTR,LPCWSTR,UINT);
API int WINAPI GetSystemMetrics(int);
API UINT WINAPI GetDpiForWindow(HWND);
API BOOL WINAPI SetProcessDpiAwarenessContext(HANDLE);
API LONG_PTR WINAPI GetWindowLongPtrW(HWND,int);
API LONG_PTR WINAPI SetWindowLongPtrW(HWND,int,LONG_PTR);
API HBRUSH WINAPI GetSysColorBrush(int);
API COLORREF WINAPI GetSysColor(int);
API BOOL WINAPI DestroyIcon(HICON);
API BOOL WINAPI OpenClipboard(HWND);
API BOOL WINAPI EmptyClipboard(void);
API HANDLE WINAPI SetClipboardData(UINT,HANDLE);
API BOOL WINAPI CloseClipboard(void);
API BOOL WINAPI SetForegroundWindow(HWND);
API BOOL WINAPI DrawFocusRect(HDC,const RECT*);
API HFONT WINAPI CreateFontW(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCWSTR);
API HBRUSH WINAPI CreateSolidBrush(COLORREF);
API BOOL WINAPI DeleteObject(HGDIOBJ);
API HGDIOBJ WINAPI SelectObject(HDC,HGDIOBJ);
API int WINAPI SetBkMode(HDC,int);
API COLORREF WINAPI SetTextColor(HDC,COLORREF);
API HGDIOBJ WINAPI GetStockObject(int);
API HDC WINAPI CreateCompatibleDC(HDC);
API BOOL WINAPI DeleteDC(HDC);
API int WINAPI GetObjectW(HGDIOBJ,int,void*);
API BOOL WINAPI StretchBlt(HDC,int,int,int,int,HDC,int,int,int,int,DWORD);
API int WINAPI SetStretchBltMode(HDC,int);
API HGDIOBJ WINAPI CreatePen(int,int,COLORREF);
API BOOL WINAPI MoveToEx(HDC,int,int,POINT*);
API BOOL WINAPI LineTo(HDC,int,int);
API BOOL WINAPI RoundRect(HDC,int,int,int,int,int,int);
API BOOL WINAPI InitCommonControlsEx(const INITCOMMONCONTROLSEX*);
API HIMAGELIST WINAPI ImageList_Create(int,int,UINT,int,int);
API int WINAPI ImageList_ReplaceIcon(HIMAGELIST,int,HICON);
#define ImageList_AddIcon(a,b) ImageList_ReplaceIcon(a,-1,b)
API BOOL WINAPI ImageList_Destroy(HIMAGELIST);
API BOOL WINAPI GetOpenFileNameW(OPENFILENAMEW*);
API BOOL WINAPI GetSaveFileNameW(OPENFILENAMEW*);
API void WINAPI DragAcceptFiles(HWND,BOOL);
API UINT WINAPI DragQueryFileW(HANDLE,UINT,LPWSTR,UINT);
API void WINAPI DragFinish(HANDLE);
API HINSTANCE WINAPI ShellExecuteW(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,int);
API LPWSTR *WINAPI CommandLineToArgvW(LPCWSTR,int*);
API LONG WINAPI SetWindowTheme(HWND,LPCWSTR,LPCWSTR);

/* Compile-time x64 ABI guards for the structures crossing the Windows boundary. */
_Static_assert(sizeof(WCHAR)==2,"Windows x64 ABI: WCHAR");
_Static_assert(sizeof(DWORD)==4,"Windows x64 ABI: DWORD");
_Static_assert(sizeof(HANDLE)==8,"Windows x64 ABI: HANDLE");
_Static_assert(sizeof(WNDCLASSEXW)==80,"Windows x64 ABI: WNDCLASSEXW");
_Static_assert(sizeof(MSG)==48,"Windows x64 ABI: MSG");
_Static_assert(sizeof(PAINTSTRUCT)==72,"Windows x64 ABI: PAINTSTRUCT");
_Static_assert(sizeof(OPENFILENAMEW)==152,"Windows x64 ABI: OPENFILENAMEW");
_Static_assert(sizeof(NMHDR)==24,"Windows x64 ABI: NMHDR");
_Static_assert(sizeof(TVITEMW)==56,"Windows x64 ABI: TVITEMW");
_Static_assert(sizeof(NMTREEVIEWW)==152,"Windows x64 ABI: NMTREEVIEWW");
_Static_assert(sizeof(LVITEMW)==88,"Windows x64 ABI: LVITEMW");
_Static_assert(sizeof(NMLVDISPINFOW)==112,"Windows x64 ABI: NMLVDISPINFOW");
_Static_assert(sizeof(NMLISTVIEW)==64,"Windows x64 ABI: NMLISTVIEW");
_Static_assert(sizeof(LVCOLUMNW)==56,"Windows x64 ABI: LVCOLUMNW");
_Static_assert(sizeof(NMCUSTOMDRAW)==80,"Windows x64 ABI: NMCUSTOMDRAW");
_Static_assert(sizeof(TCITEMW)==40,"Windows x64 ABI: TCITEMW");
_Static_assert(sizeof(DRAWITEMSTRUCT)==64,"Windows x64 ABI: DRAWITEMSTRUCT");
_Static_assert(sizeof(BITMAP)==32,"Windows x64 ABI: BITMAP");
_Static_assert(sizeof(CREATESTRUCTW)==80,"Windows x64 ABI: CREATESTRUCTW");
#endif
