/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/11/2026 by @author Tsukini

File Name:
##  @file WinWindows.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/windows/WinWindows.hpp"
#include <QGuiApplication>
#include <type_traits>
#include <windows.h>
#include <QScreen>

//----------------------------------------------------------------//
/* DEFINE */

#define XMOUSE_PW_RENDERFULLCONTENT 0x00000002 // PrintWindow: the DirectComposition content too (Windows 8.1+)
#define XMOUSE_DWMWA_CLOAKED 14 // DwmGetWindowAttribute: hidden by the shell (other virtual desktop, suspended app)

//----------------------------------------------------------------//
/* TOOLS */

// Physical pixels of Windows -> the logical pixels of the desktop of xmouse (Qt)
_cold static double ratio(void)
{
    QScreen* screen = QGuiApplication::primaryScreen();

    return screen ? std::max(1.0, screen->devicePixelRatio()) : 1.0;
}

_cold static bool cloaked(HWND window)
{
    using DwmGetWindowAttributeFn = std::add_pointer_t<HRESULT(HWND, DWORD, PVOID, DWORD)>; // x86_64: one calling convention
    static DwmGetWindowAttributeFn get = []() -> DwmGetWindowAttributeFn {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmGetWindowAttributeFn>(reinterpret_cast<void*>(GetProcAddress(dwm, "DwmGetWindowAttribute"))) : nullptr;
    }();
    DWORD value = 0;

    return get && get(window, XMOUSE_DWMWA_CLOAKED, &value, sizeof(value)) == S_OK && value != 0;
}

_cold static QString program_of(const DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    QString out;

    if (!process) return "";
    if (QueryFullProcessImageNameW(process, 0, path, &size)) out = QString::fromWCharArray(path, static_cast<int>(size)).section('\\', -1);
    CloseHandle(process);
    return out;
}

_cold static std::optional<xmouse::platform::WindowInfo> describe(HWND window)
{
    xmouse::platform::WindowInfo info;
    const int length = GetWindowTextLengthW(window);
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    RECT rect = {};
    DWORD pid = 0;
    const double r = ratio();

    if (!IsWindow(window) || !IsWindowVisible(window) || length <= 0) return std::nullopt;
    if (GetWindow(window, GW_OWNER) != nullptr || (GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) || cloaked(window)) return std::nullopt;
    GetWindowTextW(window, title.data(), length + 1);
    GetWindowRect(window, &rect);
    GetWindowThreadProcessId(window, &pid);
    info.id = reinterpret_cast<quint64>(window);
    info.title = QString::fromWCharArray(title.c_str(), length);
    info.pid = static_cast<qint64>(pid);
    info.app = program_of(pid);
    info.rect = QRect(static_cast<int>(rect.left / r), static_cast<int>(rect.top / r), static_cast<int>((rect.right - rect.left) / r), static_cast<int>((rect.bottom - rect.top) / r));
    return info;
}

_cold static BOOL CALLBACK collect(HWND window, LPARAM data)
{
    QList<xmouse::platform::WindowInfo>* out = reinterpret_cast<QList<xmouse::platform::WindowInfo>*>(data);
    const std::optional<xmouse::platform::WindowInfo> info = IsIconic(window) ? std::nullopt : describe(window);

    if (info) out->append(*info);
    return TRUE;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::WinWindows::WinWindows(void)
{
    /* Nothing */
}

xmouse::platform::WinWindows::~WinWindows()
{
    /* Nothing */
}

QList<xmouse::platform::WindowInfo> xmouse::platform::WinWindows::list(void)
{
    QList<WindowInfo> out;

    EnumWindows(&collect, reinterpret_cast<LPARAM>(&out));
    return out;
}

std::optional<xmouse::platform::WindowInfo> xmouse::platform::WinWindows::info(const quint64 id)
{
    return describe(reinterpret_cast<HWND>(id));
}

QImage xmouse::platform::WinWindows::capture(const quint64 id)
{
    HWND window = reinterpret_cast<HWND>(id);
    RECT rect = {};
    HDC screen = nullptr;
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ old = nullptr;
    BITMAPINFO bmi = {};
    QImage out;
    int w = 0;
    int h = 0;

    if (!IsWindow(window) || IsIconic(window) || !GetWindowRect(window, &rect)) return {};
    w = rect.right - rect.left;
    h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return {};
    screen = GetDC(nullptr);
    memory = CreateCompatibleDC(screen);
    bitmap = CreateCompatibleBitmap(screen, w, h);
    old = SelectObject(memory, bitmap);
    if (PrintWindow(window, memory, XMOUSE_PW_RENDERFULLCONTENT)) {
        out = QImage(w, h, QImage::Format_RGB32);
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        if (GetDIBits(memory, bitmap, 0, static_cast<UINT>(h), out.bits(), &bmi, DIB_RGB_COLORS) != h) out = QImage();
    }
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return out;
}

void xmouse::platform::WinWindows::activate(const quint64 id)
{
    HWND window = reinterpret_cast<HWND>(id);

    if (!IsWindow(window)) return;
    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    SetForegroundWindow(window); // may be refused by the focus lock: the injected click focuses it anyway
}
