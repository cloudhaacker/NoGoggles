#!/bin/sh
# Cross-compiles driver_nogoggles.dll for 64-bit Windows from Linux using clang + lld.
# No Visual Studio or Windows SDK needed: import libraries are generated from .def files.
set -e
cd "$(dirname "$0")"
mkdir -p build
cat > build/kernel32.def <<'D'
LIBRARY kernel32.dll
EXPORTS
GetPrivateProfileStringA
LoadLibraryA
GetProcAddress
GetModuleHandleExA
GetModuleFileNameA
CreateThread
WaitForSingleObject
CloseHandle
Sleep
QueryPerformanceCounter
QueryPerformanceFrequency
CreateProcessA
CreateToolhelp32Snapshot
Process32First
Process32Next
GetPrivateProfileSectionNamesA
D
cat > build/user32.def <<'D'
LIBRARY user32.dll
EXPORTS
GetAsyncKeyState
GetCursorPos
SetCursorPos
GetSystemMetrics
RegisterClassExA
UnregisterClassA
CreateWindowExA
DestroyWindow
DefWindowProcA
PeekMessageA
DispatchMessageA
BeginPaint
EndPaint
InvalidateRect
SetLayeredWindowAttributes
SetWindowPos
ShowWindow
SetWindowRgn
FillRect
GetDC
ReleaseDC
D
cat > build/gdi32.def <<'D'
LIBRARY gdi32.dll
EXPORTS
GetStockObject
SelectObject
Ellipse
CreateEllipticRgn
CreateSolidBrush
DeleteObject
CreateFontA
SetBkMode
SetTextColor
TextOutA
GetTextExtentPoint32A
CreateRoundRectRgn
D
llvm-dlltool -m i386:x86-64 -d build/kernel32.def -l build/kernel32.lib
llvm-dlltool -m i386:x86-64 -d build/user32.def -l build/user32.lib
llvm-dlltool -m i386:x86-64 -d build/gdi32.def -l build/gdi32.lib

clang++ --target=x86_64-pc-windows-msvc -std=c++20 -O2 -ffreestanding \
  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-stack-protector -mno-stack-arg-probe \
  -Wall -Wno-unused-function -Wno-misleading-indentation \
  -Ishim -Isrc -c src/nogoggles.cpp -o build/nogoggles.obj

lld-link /dll /nodefaultlib /entry:_DllMainCRTStartup /machine:x64 /opt:ref \
  /out:build/driver_nogoggles.dll build/nogoggles.obj build/kernel32.lib build/user32.lib build/gdi32.lib
echo "Built build/driver_nogoggles.dll"
