#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <ctime>
#include <fstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

ID2D1Factory* g_pD2DFactory = nullptr;
IDWriteFactory* g_pDWriteFactory = nullptr;
IDWriteTextFormat* g_pTextFormat = nullptr;
IDWriteTextFormat* g_pSubTextFormat = nullptr;

const UINT_PTR TIMER_UPDATE = 1;
const int UPDATE_INTERVAL = 1000; // Update setiap 1 detik

const std::wstring APP_VERSION = L"v0.1.0-pre";
const std::wstring APP_AUTHOR = L"amaruki";

struct Config {
    bool showOnStartup = true;
    int startupDelay = 3000;
    int fontSize = 80;
    int subFontSize = 40;
    std::wstring timeFormat = L"%H:%M:%S";
    std::wstring dateFormat = L"%A, %d %B %Y";
    bool showDate = true;
} g_config;

struct ClockWindow {
    HWND hwnd;
    ID2D1HwndRenderTarget* pRenderTarget = nullptr;
    ID2D1SolidColorBrush* pTextBrush = nullptr;
    ID2D1SolidColorBrush* pShadowBrush = nullptr;
    std::wstring lastTimeText;
    std::wstring lastDateText;
};

std::vector<ClockWindow> g_clocks;
NOTIFYICONDATAW g_nid = {};
bool g_isStartup = false;
bool g_enabled = true;
HWND g_workerW = nullptr;

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) { (*ppT)->Release(); *ppT = nullptr; }
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateDeviceResources(ClockWindow&);
void DiscardDeviceResources(ClockWindow&);
void Render(ClockWindow&);
void UpdateTimeAll();
void LoadConfig();
void SaveConfig();
void CreateTrayIcon(HWND);
void ShowContextMenu(HWND);
BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT, LPARAM);
HWND GetWorkerW();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    g_isStartup = (argc > 1 && wcsstr(argv[1], L"/startup") != nullptr);
    if (argv) LocalFree(argv);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadConfig();

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory))) {
        return 0;
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                    reinterpret_cast<IUnknown**>(&g_pDWriteFactory)))) {
        return 0;
    }

    g_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, (float)g_config.fontSize, L"", &g_pTextFormat);
    if (g_pTextFormat) {
        g_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    g_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, (float)g_config.subFontSize, L"", &g_pSubTextFormat);
    if (g_pSubTextFormat) {
        g_pSubTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_pSubTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DesktopClockOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    // Trigger WorkerW creation
    HWND progman = FindWindowW(L"Progman", nullptr);
    SendMessageTimeoutW(progman, 0x052C, 0xD, 0, SMTO_NORMAL, 1000, nullptr);
    SendMessageTimeoutW(progman, 0x052C, 0xD, 1, SMTO_NORMAL, 1000, nullptr);
    Sleep(200);
    
    g_workerW = GetWorkerW();

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)hInstance);

    if (g_clocks.empty()) {
        MessageBoxW(nullptr, L"Gagal membuat clock overlay.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    CreateTrayIcon(g_clocks[0].hwnd);
    SetTimer(g_clocks[0].hwnd, TIMER_UPDATE, UPDATE_INTERVAL, nullptr);

    if (g_isStartup && g_config.showOnStartup) {
        Sleep(g_config.startupDelay);
    }

    // Initial update
    UpdateTimeAll();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    KillTimer(g_clocks[0].hwnd, TIMER_UPDATE);
    
    for (auto& clock : g_clocks) {
        DiscardDeviceResources(clock);
        DestroyWindow(clock.hwnd);
    }
    
    SafeRelease(&g_pSubTextFormat);
    SafeRelease(&g_pTextFormat);
    SafeRelease(&g_pDWriteFactory);
    SafeRelease(&g_pD2DFactory);
    CoUninitialize();
    return 0;
}

HWND GetWorkerW()
{
    HWND workerW = nullptr;
    HWND h = nullptr;
    
    while ((h = FindWindowExW(nullptr, h, L"WorkerW", nullptr))) {
        HWND shellView = FindWindowExW(h, nullptr, L"SHELLDLL_DefView", nullptr);
        if (shellView) {
            workerW = FindWindowExW(nullptr, h, L"WorkerW", nullptr);
            break;
        }
    }
    
    return workerW;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
{
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMonitor, &mi);

    HINSTANCE hInstance = (HINSTANCE)lParam;

    int width = mi.rcMonitor.right - mi.rcMonitor.left;
    int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    int clockWidth = width / 2;
    int clockHeight = 300;
    int x = mi.rcMonitor.left + (width - clockWidth) / 2;
    int y = mi.rcMonitor.top + (height - clockHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"DesktopClockOverlay", L"Desktop Clock",
        WS_POPUP | WS_VISIBLE,
        x, y, clockWidth, clockHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return TRUE;

    // Set transparency
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // Attach to desktop
    if (g_workerW && IsWindow(g_workerW)) {
        SetParent(hwnd, g_workerW);
    }

    g_clocks.push_back({ hwnd });
    
    // Force initial paint
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ClockWindow* clock = nullptr;
    for (auto& c : g_clocks) {
        if (c.hwnd == hwnd) { 
            clock = &c; 
            break; 
        }
    }

    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            
            if (clock && g_enabled) {
                Render(*clock);
            }
            
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_UPDATE && g_enabled) {
            UpdateTimeAll();
        }
        return 0;

    case WM_USER + 1:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001) {
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == 1002) {
            g_enabled = !g_enabled;
            SaveConfig();
            for (auto& c : g_clocks) {
                ShowWindow(c.hwnd, g_enabled ? SW_SHOW : SW_HIDE);
                if (g_enabled) {
                    InvalidateRect(c.hwnd, nullptr, TRUE);
                }
            }
        }
        return 0;

    case WM_DISPLAYCHANGE:
        if (clock) {
            DiscardDeviceResources(*clock);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void CreateDeviceResources(ClockWindow& clock)
{
    if (clock.pRenderTarget) return;

    RECT rc;
    GetClientRect(clock.hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = g_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(clock.hwnd, size),
        &clock.pRenderTarget);

    if (SUCCEEDED(hr) && clock.pRenderTarget) {
        clock.pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &clock.pTextBrush);
        clock.pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.8f), &clock.pShadowBrush);
    }
}

void DiscardDeviceResources(ClockWindow& clock)
{
    SafeRelease(&clock.pTextBrush);
    SafeRelease(&clock.pShadowBrush);
    SafeRelease(&clock.pRenderTarget);
}

void Render(ClockWindow& clock)
{
    CreateDeviceResources(clock);
    if (!clock.pRenderTarget) return;

    clock.pRenderTarget->BeginDraw();
    
    // Clear with transparent black (key color)
    clock.pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    D2D1_SIZE_F size = clock.pRenderTarget->GetSize();

    // Draw time
    if (!clock.lastTimeText.empty() && clock.pTextBrush && g_pTextFormat) {
        float timeHeight = size.height * 0.5f;
        
        // Shadow
        if (clock.pShadowBrush) {
            D2D1_RECT_F shadowRect = D2D1::RectF(4, 4, size.width + 4, timeHeight + 4);
            clock.pRenderTarget->DrawText(
                clock.lastTimeText.c_str(), 
                (UINT32)clock.lastTimeText.length(),
                g_pTextFormat, 
                shadowRect, 
                clock.pShadowBrush);
        }
        
        // Text
        D2D1_RECT_F textRect = D2D1::RectF(0, 0, size.width, timeHeight);
        clock.pRenderTarget->DrawText(
            clock.lastTimeText.c_str(), 
            (UINT32)clock.lastTimeText.length(),
            g_pTextFormat, 
            textRect, 
            clock.pTextBrush);
    }

    // Draw date
    if (!clock.lastDateText.empty() && g_config.showDate && g_pSubTextFormat && clock.pTextBrush) {
        float dateTop = size.height * 0.5f;
        
        // Shadow
        if (clock.pShadowBrush) {
            D2D1_RECT_F shadowRect = D2D1::RectF(3, dateTop + 3, size.width + 3, size.height + 3);
            clock.pRenderTarget->DrawText(
                clock.lastDateText.c_str(), 
                (UINT32)clock.lastDateText.length(),
                g_pSubTextFormat, 
                shadowRect, 
                clock.pShadowBrush);
        }
        
        // Text
        D2D1_RECT_F textRect = D2D1::RectF(0, dateTop, size.width, size.height);
        clock.pRenderTarget->DrawText(
            clock.lastDateText.c_str(), 
            (UINT32)clock.lastDateText.length(),
            g_pSubTextFormat, 
            textRect, 
            clock.pTextBrush);
    }

    HRESULT hr = clock.pRenderTarget->EndDraw();
    
    // Recreate if device lost
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources(clock);
    }
}

void UpdateTimeAll()
{
    time_t now = time(nullptr);
    struct tm tstruct;
    localtime_s(&tstruct, &now);

    wchar_t timeBuf[100], dateBuf[200];
    wcsftime(timeBuf, 100, g_config.timeFormat.c_str(), &tstruct);
    wcsftime(dateBuf, 200, g_config.dateFormat.c_str(), &tstruct);

    std::wstring newTime = timeBuf;
    std::wstring newDate = dateBuf;

    for (auto& clock : g_clocks) {
        if (clock.lastTimeText != newTime || clock.lastDateText != newDate) {
            clock.lastTimeText = newTime;
            clock.lastDateText = newDate;
            InvalidateRect(clock.hwnd, nullptr, FALSE);
        }
    }
}

void LoadConfig()
{
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    wcscat_s(path, L"\\DesktopClock.ini");

    std::wifstream file(path);
    if (file.is_open()) {
        std::wstring line;
        while (std::getline(file, line)) {
            if (line.find(L"ShowOnStartup=") == 0) {
                g_enabled = (line.substr(14) == L"1");
            } else if (line.find(L"StartupDelay=") == 0) {
                g_config.startupDelay = std::stoi(line.substr(13));
            } else if (line.find(L"FontSize=") == 0) {
                g_config.fontSize = std::stoi(line.substr(9));
            } else if (line.find(L"SubFontSize=") == 0) {
                g_config.subFontSize = std::stoi(line.substr(12));
            }
        }
        file.close();
    }
}

void SaveConfig()
{
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    wcscat_s(path, L"\\DesktopClock.ini");

    std::wofstream file(path);
    if (file.is_open()) {
        file << L"ShowOnStartup=" << (g_enabled ? 1 : 0) << L"\n";
        file << L"StartupDelay=" << g_config.startupDelay << L"\n";
        file << L"FontSize=" << g_config.fontSize << L"\n";
        file << L"SubFontSize=" << g_config.subFontSize << L"\n";
        file.close();
    }
}

void CreateTrayIcon(HWND hwnd)
{
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_USER + 1;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    swprintf_s(g_nid.szTip, L"Desktop Clock %s by %s", APP_VERSION.c_str(), APP_AUTHOR.c_str());
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (g_enabled ? MF_CHECKED : 0), 1002, L"Aktifkan Clock");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 1001, L"Keluar");
    
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
    PostMessage(hwnd, WM_NULL, 0, 0);
}