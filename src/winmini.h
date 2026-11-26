// The handful of Windows functions NoGoggles uses, declared by hand so the
// driver can be built without the Windows SDK.
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef void* HANDLE;
typedef void* HMODULE;
typedef int BOOL;
struct POINT { long x, y; };
union LARGE_INTEGER { struct { unsigned long lo; long hi; } parts; long long QuadPart; };

typedef void* HWND;
typedef void* HDC;
typedef void* HGDIOBJ;
typedef void* HRGN;
struct SIZE { long cx, cy; };
struct PROCESSENTRY32 { unsigned long dwSize, cntUsage, th32ProcessID; uintptr_t th32DefaultHeapID; unsigned long th32ModuleID, cntThreads, th32ParentProcessID; long pcPriClassBase; unsigned long dwFlags; char szExeFile[260]; };
#define TH32CS_SNAPPROCESS 2
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
struct RECT { long left, top, right, bottom; };
struct MSG { HWND hwnd; unsigned int message; uintptr_t wParam; intptr_t lParam; unsigned long time; POINT pt; unsigned long lPrivate; };
struct PAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; unsigned char rgbReserved[32]; };
typedef intptr_t(__stdcall* WNDPROC)(HWND, unsigned int, uintptr_t, intptr_t);
struct WNDCLASSEXA { unsigned int cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HMODULE hInstance; void* hIcon; void* hCursor; void* hbrBackground; const char* lpszMenuName; const char* lpszClassName; void* hIconSm; };
struct STARTUPINFOA { unsigned long cb; char* lpReserved; char* lpDesktop; char* lpTitle; unsigned long dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags; unsigned short wShowWindow, cbReserved2; unsigned char* lpReserved2; HANDLE hStdInput, hStdOutput, hStdError; };
struct PROCESS_INFORMATION { HANDLE hProcess, hThread; unsigned long dwProcessId, dwThreadId; };

#define WS_POPUP 0x80000000u
#define WS_EX_TOPMOST 0x8
#define WS_EX_TRANSPARENT 0x20
#define WS_EX_TOOLWINDOW 0x80
#define WS_EX_LAYERED 0x80000
#define WS_EX_NOACTIVATE 0x08000000
#define WM_PAINT 0x000F
#define WM_NCHITTEST 0x0084
#define HTTRANSPARENT (-1)
#define PM_REMOVE 1
#define LWA_ALPHA 2
#define SW_HIDE 0
#define SWP_NOACTIVATE 0x10
#define SWP_SHOWWINDOW 0x40
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define WHITE_BRUSH 0
#define BLACK_BRUSH 4
#define NULL_PEN 8
#define CREATE_NO_WINDOW 0x08000000

#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_VM_READ 0x0010
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define GW_OWNER 4
#define GWL_EXSTYLE (-20)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define SWP_NOSIZE 0x1
#define SWP_NOMOVE 0x2
#define KEYEVENTF_KEYUP 0x2
typedef BOOL(__stdcall* WNDENUMPROC)(HWND, intptr_t);

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x2
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x4

extern "C" {
// kernel32
__declspec(dllimport) unsigned long __stdcall GetPrivateProfileStringA(const char*, const char*, const char*, char*, unsigned long, const char*);
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HMODULE, const char*);
__declspec(dllimport) BOOL __stdcall GetModuleHandleExA(unsigned long, const char*, HMODULE*);
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(HMODULE, char*, unsigned long);
__declspec(dllimport) HANDLE __stdcall CreateThread(void*, size_t, unsigned long(__stdcall*)(void*), void*, unsigned long, unsigned long*);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(HANDLE, unsigned long);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) void __stdcall Sleep(unsigned long);
__declspec(dllimport) BOOL __stdcall QueryPerformanceCounter(LARGE_INTEGER*);
__declspec(dllimport) BOOL __stdcall QueryPerformanceFrequency(LARGE_INTEGER*);
__declspec(dllimport) BOOL __stdcall CreateProcessA(const char*, char*, void*, void*, BOOL, unsigned long, void*, const char*, STARTUPINFOA*, PROCESS_INFORMATION*);
__declspec(dllimport) HANDLE __stdcall CreateToolhelp32Snapshot(unsigned long, unsigned long);
__declspec(dllimport) BOOL __stdcall Process32First(HANDLE, PROCESSENTRY32*);
__declspec(dllimport) BOOL __stdcall Process32Next(HANDLE, PROCESSENTRY32*);
__declspec(dllimport) unsigned long __stdcall GetPrivateProfileSectionNamesA(char*, unsigned long, const char*);
__declspec(dllimport) HANDLE __stdcall OpenProcess(unsigned long, BOOL, unsigned long);
__declspec(dllimport) BOOL __stdcall QueryFullProcessImageNameA(HANDLE, unsigned long, char*, unsigned long*);
__declspec(dllimport) BOOL __stdcall K32EnumProcessModules(HANDLE, HMODULE*, unsigned long, unsigned long*);
__declspec(dllimport) unsigned long __stdcall K32GetModuleBaseNameA(HANDLE, HMODULE, char*, unsigned long);
__declspec(dllimport) unsigned long __stdcall GetCurrentProcessId();
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
// user32
__declspec(dllimport) BOOL __stdcall EnumWindows(WNDENUMPROC, intptr_t);
__declspec(dllimport) BOOL __stdcall IsWindowVisible(HWND);
__declspec(dllimport) HWND __stdcall GetWindow(HWND, unsigned int);
__declspec(dllimport) intptr_t __stdcall GetWindowLongPtrA(HWND, int);
__declspec(dllimport) unsigned long __stdcall GetWindowThreadProcessId(HWND, unsigned long*);
__declspec(dllimport) BOOL __stdcall GetWindowRect(HWND, RECT*);
__declspec(dllimport) HWND __stdcall GetForegroundWindow();
__declspec(dllimport) BOOL __stdcall SetForegroundWindow(HWND);
__declspec(dllimport) BOOL __stdcall AttachThreadInput(unsigned long, unsigned long, BOOL);
__declspec(dllimport) void __stdcall keybd_event(unsigned char, unsigned char, unsigned long, uintptr_t);
__declspec(dllimport) int __stdcall FillRect(HDC, const RECT*, HGDIOBJ);
__declspec(dllimport) HDC __stdcall GetDC(HWND);
__declspec(dllimport) int __stdcall ReleaseDC(HWND, HDC);
__declspec(dllimport) unsigned short __stdcall RegisterClassExA(const WNDCLASSEXA*);
__declspec(dllimport) BOOL __stdcall UnregisterClassA(const char*, HMODULE);
__declspec(dllimport) HWND __stdcall CreateWindowExA(unsigned long, const char*, const char*, unsigned long, int, int, int, int, HWND, void*, HMODULE, void*);
__declspec(dllimport) BOOL __stdcall DestroyWindow(HWND);
__declspec(dllimport) intptr_t __stdcall DefWindowProcA(HWND, unsigned int, uintptr_t, intptr_t);
__declspec(dllimport) BOOL __stdcall PeekMessageA(MSG*, HWND, unsigned int, unsigned int, unsigned int);
__declspec(dllimport) intptr_t __stdcall DispatchMessageA(const MSG*);
__declspec(dllimport) HDC __stdcall BeginPaint(HWND, PAINTSTRUCT*);
__declspec(dllimport) BOOL __stdcall EndPaint(HWND, const PAINTSTRUCT*);
__declspec(dllimport) BOOL __stdcall InvalidateRect(HWND, const RECT*, BOOL);
__declspec(dllimport) BOOL __stdcall SetLayeredWindowAttributes(HWND, unsigned long, unsigned char, unsigned long);
__declspec(dllimport) BOOL __stdcall SetWindowPos(HWND, HWND, int, int, int, int, unsigned int);
__declspec(dllimport) BOOL __stdcall ShowWindow(HWND, int);
__declspec(dllimport) int __stdcall SetWindowRgn(HWND, HRGN, BOOL);
// gdi32
__declspec(dllimport) HGDIOBJ __stdcall CreateSolidBrush(unsigned long);
__declspec(dllimport) BOOL __stdcall DeleteObject(HGDIOBJ);
__declspec(dllimport) HGDIOBJ __stdcall CreateFontA(int, int, int, int, int, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, const char*);
__declspec(dllimport) int __stdcall SetBkMode(HDC, int);
__declspec(dllimport) unsigned long __stdcall SetTextColor(HDC, unsigned long);
__declspec(dllimport) BOOL __stdcall TextOutA(HDC, int, int, const char*, int);
__declspec(dllimport) BOOL __stdcall GetTextExtentPoint32A(HDC, const char*, int, SIZE*);
__declspec(dllimport) HRGN __stdcall CreateRoundRectRgn(int, int, int, int, int, int);
__declspec(dllimport) HGDIOBJ __stdcall GetStockObject(int);
__declspec(dllimport) HGDIOBJ __stdcall SelectObject(HDC, HGDIOBJ);
__declspec(dllimport) BOOL __stdcall Ellipse(HDC, int, int, int, int);
__declspec(dllimport) HRGN __stdcall CreateEllipticRgn(int, int, int, int);
__declspec(dllimport) short __stdcall GetAsyncKeyState(int);
__declspec(dllimport) BOOL __stdcall GetCursorPos(POINT*);
__declspec(dllimport) BOOL __stdcall SetCursorPos(int, int);
__declspec(dllimport) int __stdcall GetSystemMetrics(int);
}
