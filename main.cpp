#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <fstream>
#include <regex>
#include "updater.h"
#include "splash.h"
#include "railworks_path.h"
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

static HINSTANCE g_inst{};
static HWND g_main{}, g_tsc{}, g_dll{}, g_loco{}, g_speed{}, g_log{};
static HMODULE g_railDll{};
static std::wstring g_railWorksPath;
static HFONT g_font{}, g_bold{}, g_title{}, g_small{};

using GetStringFn = const char* (__cdecl*)();
static GetStringFn pGetLocoName{};

std::wstring AnsiToWide(const char* s) {
    if (!s || !*s) return L"";
    int len = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (len <= 1) return L"";
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

void Log(const std::wstring& text) {
    if (!g_log) return;
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, len, len);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t p[32]{};
    swprintf_s(p, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    std::wstring line = std::wstring(p) + text + L"\r\n";
    SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
}

bool Exists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

std::vector<std::wstring> SteamRootsFromRegistry() {
    std::vector<std::wstring> roots;
    const wchar_t* subkeys[] = {
        L"Software\\Valve\\Steam",
        L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
        L"SOFTWARE\\Valve\\Steam"
    };

    HKEY hives[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_LOCAL_MACHINE };

    for (int i = 0; i < 3; ++i) {
        HKEY key{};
        if (RegOpenKeyExW(hives[i], subkeys[i], 0, KEY_READ, &key) == ERROR_SUCCESS) {
            wchar_t buf[1024]{};
            DWORD size = sizeof(buf);
            DWORD type = 0;
            if (RegQueryValueExW(key, L"SteamPath", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS ||
                RegQueryValueExW(key, L"InstallPath", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
                if (*buf) roots.emplace_back(buf);
            }
            RegCloseKey(key);
        }
    }
    return roots;
}

void AddLibrariesFromVdf(const std::wstring& steamRoot, std::vector<std::wstring>& roots) {
    std::wstring vdf = steamRoot + L"\\steamapps\\libraryfolders.vdf";
    std::wifstream in(vdf);
    if (!in) return;

    std::wstring line;
    std::wregex pathRe(LR"re("path"\s*"([^"]+)")re", std::regex_constants::icase);
    while (std::getline(in, line)) {
        std::wsmatch m;
        if (std::regex_search(line, m, pathRe) && m.size() > 1) {
            std::wstring p = m[1].str();
            size_t pos = 0;
            while ((pos = p.find(L"\\\\", pos)) != std::wstring::npos) {
                p.replace(pos, 2, L"\\");
                ++pos;
            }
            roots.push_back(p);
        }
    }
}

std::wstring FindRailWorks() {
    std::vector<std::wstring> roots = SteamRootsFromRegistry();

    // Common drive roots as fallback
    wchar_t drives[512]{};
    if (GetLogicalDriveStringsW(511, drives)) {
        for (wchar_t* p = drives; *p; p += wcslen(p) + 1) {
            std::wstring root = p;
            roots.push_back(root + L"SteamLibrary");
            roots.push_back(root + L"Steam");
            roots.push_back(root + L"Program Files (x86)\\Steam");
            roots.push_back(root + L"Program Files\\Steam");
        }
    }

    // Expand Steam libraryfolders.vdf
    std::vector<std::wstring> expanded = roots;
    for (const auto& root : roots) AddLibrariesFromVdf(root, expanded);

    for (const auto& root : expanded) {
        auto rw = RailWorksPath::Resolve(root);
        if (!rw.empty()) return rw;
    }
    return L"";
}

std::wstring BrowseForRailWorks() {
    BROWSEINFOW bi{};
    bi.hwndOwner = g_main;
    bi.lpszTitle = L"Vyber RailWorks, Steam knihovnu nebo RailWorks64.exe";
    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_BROWSEINCLUDEFILES | BIF_EDITBOX;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";

    wchar_t path[32768]{};
    std::wstring result;
    if (SHGetPathFromIDListEx(pidl, path, ARRAYSIZE(path), GPFIDL_DEFAULT)) result = path;
    CoTaskMemFree(pidl);
    if (result.empty()) {
        MessageBoxW(g_main, L"Vyber složku na disku nebo soubor RailWorks64.exe / RailWorks.exe. Zvolenou položku nelze převést na cestu.", L"Kvaltík TSC Hub", MB_ICONWARNING);
        return L"";
    }

    if (!result.empty()) {
        auto root = RailWorksPath::Resolve(result);
        if (!root.empty()) return root;
        const std::wstring message = L"V tomto výběru nebyla nalezena instalace Train Simulator Classic:\n\n" + result +
            L"\n\nOčekává se soubor RailWorks64.exe nebo RailWorks.exe přímo ve složce RailWorks."
            L"\nVyber tuto složku, přímo jeden z těchto EXE souborů, nebo Steam knihovnu "
            L"obsahující steamapps\\common\\RailWorks (lze vybrat i steamapps či common)."
            L"\n\nPokud EXE chybí, ověř instalaci hry ve Steamu. Dosavadní nastavení zůstalo zachováno.";
        MessageBoxW(g_main, message.c_str(), L"Kvaltík TSC Hub", MB_ICONWARNING);
    }
    return L"";
}

bool IsTscRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"RailWorks64.exe") == 0 ||
                _wcsicmp(pe.szExeFile, L"RailWorks.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

#include "hub_ui.h"
#include "live_modules.h"

void DisconnectRailDriver() {
    LiveModules::Reset();
    pGetLocoName = nullptr;
    if (g_railDll) {
        FreeLibrary(g_railDll);
        g_railDll = nullptr;
    }
}

bool ConnectRailDriver() {
    DisconnectRailDriver();
    if (g_railWorksPath.empty()) g_railWorksPath = FindRailWorks();
    if (g_railWorksPath.empty()) {
        SetWindowTextW(g_dll, L"RailDriver: nenalezen");
        return false;
    }

    auto path = g_railWorksPath + L"\\plugins\\RailDriver64.dll";
    if (!Exists(path)) {
        SetWindowTextW(g_dll, L"RailDriver: DLL chybí");
        return false;
    }

    g_railDll = LoadLibraryW(path.c_str());
    if (!g_railDll) {
        SetWindowTextW(g_dll, L"RailDriver: chyba načtení");
        return false;
    }

    pGetLocoName = reinterpret_cast<GetStringFn>(GetProcAddress(g_railDll, "GetLocoName"));
    if (!pGetLocoName || !LiveModules::Bind(g_railDll)) {
        DisconnectRailDriver();
        SetWindowTextW(g_dll,L"RailDriver: nekompatibilní DLL");
        return false;
    }
    SetWindowTextW(g_dll, L"RailDriver: připojeno");
    Log(L"RailDriver64.dll načtena.");
    return true;
}

void UpdateStatus() {
    SetWindowTextW(g_tsc, IsTscRunning() ? L"TSC: SPUŠTĚNO" : L"TSC: vypnuto");
    if (!g_railDll) ConnectRailDriver();

    LiveModules::Poll(IsTscRunning());
    SetWindowTextW(g_loco,(L"Lokomotiva: "+(LiveModules::loco.empty()?L"—":LiveModules::loco)).c_str());
    float speed=0; wchar_t speedText[64]{};
    if(Telemetry::SpeedKph(LiveModules::controllers,speed)) {
        swprintf_s(speedText,L"Rychlost: %.1f km/h",speed); SetWindowTextW(g_speed,speedText);
    } else SetWindowTextW(g_speed,L"Rychlost: —");
}

void OpenRailWorksFolder(const std::wstring& sub = L"") {
    if (g_railWorksPath.empty()) g_railWorksPath = FindRailWorks();
    if (g_railWorksPath.empty()) {
        MessageBoxW(g_main, L"RailWorks nebyl nalezen.", L"Kvaltík TSC Hub", MB_ICONWARNING);
        return;
    }
    std::wstring path = g_railWorksPath + sub;
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void LaunchTSC() {
    if (g_railWorksPath.empty()) g_railWorksPath = FindRailWorks();
    if (g_railWorksPath.empty()) {
        MessageBoxW(g_main, L"RailWorks nebyl nalezen.", L"Kvaltík TSC Hub", MB_ICONWARNING);
        return;
    }
    auto exe = g_railWorksPath + L"\\RailWorks64.exe";
    if (!RailWorksPath::IsFile(exe)) exe = g_railWorksPath + L"\\RailWorks.exe";
    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, g_railWorksPath.c_str(), SW_SHOWNORMAL);
    Log(L"Spouštím Train Simulator Classic.");
}

void ModuleInfo(const wchar_t* name, const wchar_t* desc) {
    std::wstring s = std::wstring(name) + L"\n\n" + desc +
        L"\n\nModul je připravený pro další vývoj přímo uvnitř Hubu.";
    MessageBoxW(g_main, s.c_str(), L"Kvaltík TSC Hub", MB_OK | MB_ICONINFORMATION);
}



void HandleModule(int id) {
    if (id==2015 || id==2014 || id==2001) { LiveModules::Open(id); return; }
    switch(id) {
        case 2001: ModuleInfo(L"NavTrain",L"Rychlost, stanice, profil tratě, jízdní řád a signalizace."); break;
        case 2002: ModuleInfo(L"VO79",L"Radiostanice VO79 napojená na TSC Connector."); break;
        case 2003: ModuleInfo(L"RailControl",L"Dispečerské schéma, provoz, návěstidla a mimořádnosti."); break;
        case 2004: ModuleInfo(L"Scenario Creator",L"Číslo vlaku + trať → tvorba scénáře."); break;
        case 2005: ModuleInfo(L"Jízdní řád",L"Kompletní jízdní řád podle čísla vlaku."); break;
        case 2006: ModuleInfo(L"Consist Manager",L"Správa lokomotiv, vozů a souprav."); break;
        case 2007: ModuleInfo(L"Scenario Doctor",L"Kontrola assetů a problémů scénáře."); break;
        case 2008: ModuleInfo(L"Kniha jízd",L"Historie jízd, kilometry a statistiky."); break;
        case 2009: ModuleInfo(L"Live Map",L"Živá poloha hráčova vlaku."); break;
        case 2010: ModuleInfo(L"Shader Manager",L"Performance, Real Neutral, Ultra Sunset a další."); break;
        case 2011: ModuleInfo(L"Route Manager",L"Správa tratí, verzí a závislostí."); break;
        case 2012: ModuleInfo(L"Rozkazovač",L"České vlakové rozkazy."); break;
        case 2013: ModuleInfo(L"Výpravčí",L"Odjezdové pískání a scénářové eventy."); break;
        case 2014: ModuleInfo(L"Driver Display",L"Externí panel pro druhý monitor."); break;
        case 2015: ModuleInfo(L"TSC Connector",L"RailDriver64.dll, controllery a diagnostika."); break;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
        case WM_DRAWITEM:
            if (wp) { HubUi::DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE; }
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC dc=reinterpret_cast<HDC>(wp);
            const bool log=reinterpret_cast<HWND>(lp)==g_log;
            SetTextColor(dc,log ? HubUi::Muted : HubUi::Text);
            SetBkColor(dc,log ? HubUi::Surface : HubUi::Background);
            return reinterpret_cast<LRESULT>(log ? HubUi::surfaceBrush : HubUi::backgroundBrush);
        }
        case WM_PAINT: HubUi::Paint(hwnd); return 0;
        case WM_SIZE: HubUi::Layout(hwnd); return 0;
        case WM_GETMINMAXINFO: {
            auto limits=reinterpret_cast<MINMAXINFO*>(lp);
            limits->ptMinTrackSize={1094,480};
            return 0;
        }
        case WM_VSCROLL: {
            SCROLLINFO info{sizeof(info),SIF_ALL}; GetScrollInfo(hwnd,SB_VERT,&info);
            int position=HubUi::scroll;
            switch(LOWORD(wp)) {
                case SB_LINEUP: position-=32; break;
                case SB_LINEDOWN: position+=32; break;
                case SB_PAGEUP: position-=info.nPage; break;
                case SB_PAGEDOWN: position+=info.nPage; break;
                case SB_THUMBTRACK: position=info.nTrackPos; break;
                case SB_THUMBPOSITION: position=info.nPos; break;
                case SB_TOP: position=0; break;
                case SB_BOTTOM: position=info.nMax; break;
            }
            HubUi::Scroll(hwnd,position); return 0;
        }
        case WM_MOUSEWHEEL:
            HubUi::Scroll(hwnd,HubUi::scroll-static_cast<short>(HIWORD(wp))*96/WHEEL_DELTA); return 0;
        case WM_CREATE:
            BuildUi(hwnd);
            g_railWorksPath = FindRailWorks();
            if (!g_railWorksPath.empty()) Log(L"RailWorks nalezen: " + g_railWorksPath);
            else Log(L"RailWorks zatím nebyl nalezen.");
            ConnectRailDriver();
            SetTimer(hwnd,1,1000,nullptr);
            UpdateStatus();
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id==1001) LaunchTSC();
            else if (id==1002) { DisconnectRailDriver(); if (!RailWorksPath::IsRoot(g_railWorksPath)) g_railWorksPath=FindRailWorks(); ConnectRailDriver(); UpdateStatus(); }
            else if (id==1003) {
                auto p = BrowseForRailWorks();
                if (!p.empty()) {
                    g_railWorksPath = p;
                    DisconnectRailDriver();
                    ConnectRailDriver();
                    Log(L"RailWorks ručně nastaven: " + g_railWorksPath);
                    UpdateStatus();
                }
            }
            else if (id==1004) {
                KvaltikUpdater::CheckAndUpdate(g_main, false);
            }
            else if (id>=2001 && id<=2015) HandleModule(id);
            else if (id==3001) OpenRailWorksFolder();
            else if (id==3002) OpenRailWorksFolder(L"\\Assets");
            else if (id==3003) OpenRailWorksFolder(L"\\Content");
            else if (id==3004) OpenRailWorksFolder(L"\\Content\\Routes");
            else if (id==3005) OpenRailWorksFolder(L"\\plugins");
            return 0;
        }

        case WM_TIMER:
            if (wp==1) UpdateStatus();
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd,1);
            DisconnectRailDriver();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int show) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_inst=h;
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    KvaltikSplash::Show(h);

    g_font=CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_bold=CreateFontW(18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_title=CreateFontW(34,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_small=CreateFontW(15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");

    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc);
    wc.lpfnWndProc=WndProc;
    wc.hInstance=h;
    wc.lpszClassName=L"KvaltikTSCHubMainWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wc.hbrBackground=HubUi::backgroundBrush;
    RegisterClassExW(&wc);

    RECT workArea{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&workArea,0);
    const int initialHeight=(std::min)(890,static_cast<int>(workArea.bottom-workArea.top));
    g_main=CreateWindowExW(0,wc.lpszClassName,L"Kvaltík TSC Hub",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
        CW_USEDEFAULT,CW_USEDEFAULT,1094,initialHeight,nullptr,nullptr,h,nullptr);

    if (!g_main) {
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }
    ShowWindow(g_main,show);
    UpdateWindow(g_main);

    MSG m{};
    while(GetMessageW(&m,nullptr,0,0)>0) {
        if (LiveModules::connector && IsDialogMessageW(LiveModules::connector,&m)) continue;
        if (LiveModules::display && IsDialogMessageW(LiveModules::display,&m)) continue;
        if (LiveModules::navigation && IsDialogMessageW(LiveModules::navigation,&m)) continue;
        if (IsDialogMessageW(g_main,&m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (SUCCEEDED(comResult)) CoUninitialize();
    DeleteObject(g_font); DeleteObject(g_bold); DeleteObject(g_title); DeleteObject(g_small);
    HubUi::Cleanup();
    return static_cast<int>(m.wParam);
}
