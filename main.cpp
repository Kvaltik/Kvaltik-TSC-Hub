#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>
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
    std::wstring out(len - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, out.data(), len);
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

std::wstring FindRailWorks() {
    wchar_t drives[512]{};
    if (!GetLogicalDriveStringsW(511, drives)) return L"";

    for (wchar_t* p = drives; *p; p += wcslen(p) + 1) {
        std::wstring root = p;
        std::vector<std::wstring> paths = {
            root + L"SteamLibrary\\steamapps\\common\\RailWorks",
            root + L"Steam\\steamapps\\common\\RailWorks",
            root + L"Program Files (x86)\\Steam\\steamapps\\common\\RailWorks",
            root + L"Program Files\\Steam\\steamapps\\common\\RailWorks"
        };
        for (const auto& x : paths)
            if (Exists(x + L"\\RailWorks64.exe")) return x;
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

void DisconnectRailDriver() {
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
    SetWindowTextW(g_dll, L"RailDriver: připojeno");
    Log(L"RailDriver64.dll načtena.");
    return true;
}

void UpdateStatus() {
    SetWindowTextW(g_tsc, IsTscRunning() ? L"TSC: SPUŠTĚNO" : L"TSC: vypnuto");
    if (!g_railDll) ConnectRailDriver();

    if (g_railDll && pGetLocoName) {
        const char* raw = pGetLocoName();
        auto name = AnsiToWide(raw);
        SetWindowTextW(g_loco, (L"Lokomotiva: " + (name.empty() ? L"—" : name)).c_str());
    } else {
        SetWindowTextW(g_loco, L"Lokomotiva: —");
    }

    SetWindowTextW(g_speed, L"Rychlost: —");
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
    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, g_railWorksPath.c_str(), SW_SHOWNORMAL);
    Log(L"Spouštím Train Simulator Classic.");
}

void ModuleInfo(const wchar_t* name, const wchar_t* desc) {
    std::wstring s = std::wstring(name) + L"\n\n" + desc +
        L"\n\nModul je připravený pro další vývoj přímo uvnitř Hubu.";
    MessageBoxW(g_main, s.c_str(), L"Kvaltík TSC Hub", MB_OK | MB_ICONINFORMATION);
}

void Font(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); }

HWND Label(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT f, DWORD style=SS_LEFT) {
    HWND c = CreateWindowExW(0, L"STATIC", text, WS_CHILD|WS_VISIBLE|style,
        x,y,w,h,parent,nullptr,g_inst,nullptr);
    Font(c,f);
    return c;
}

HWND Btn(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"BUTTON", text, WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),g_inst,nullptr);
    Font(c,g_bold);
    return c;
}

void BuildUi(HWND hwnd) {
    Label(hwnd,L"KVALTÍK TSC HUB",24,18,430,45,g_title);
    Label(hwnd,L"Train Simulator Classic — všechno na jednom místě",26,62,520,25,g_small);
    Btn(hwnd,L"SPUSTIT TSC",1001,760,24,180,42);
    Btn(hwnd,L"OBNOVIT",1002,950,24,120,42);

    g_tsc   = Label(hwnd,L"TSC: —",25,105,200,30,g_bold);
    g_dll   = Label(hwnd,L"RailDriver: —",235,105,250,30,g_bold);
    g_loco  = Label(hwnd,L"Lokomotiva: —",495,105,350,30,g_bold);
    g_speed = Label(hwnd,L"Rychlost: —",855,105,200,30,g_bold);

    Label(hwnd,L"MODULY",25,150,180,30,g_bold);

    struct Card { const wchar_t* title; const wchar_t* sub; int id; };
    std::vector<Card> cards = {
        {L"NavTrain",L"asistent strojvedoucího",2001},
        {L"VO79",L"radiostanice",2002},
        {L"RailControl",L"dispečerský panel",2003},
        {L"Scenario Creator",L"generátor scénářů",2004},
        {L"Jízdní řád",L"trasa a časy",2005},
        {L"Consist Manager",L"soupravy",2006},
        {L"Scenario Doctor",L"kontrola scénářů",2007},
        {L"Kniha jízd",L"historie a statistiky",2008},
        {L"Live Map",L"živá poloha",2009},
        {L"Shader Manager",L"ReShade profily",2010},
        {L"Route Manager",L"tratě a závislosti",2011},
        {L"Rozkazovač",L"české rozkazy",2012},
        {L"Výpravčí",L"pískání a eventy",2013},
        {L"Driver Display",L"druhý monitor",2014},
        {L"TSC Connector",L"RailDriver diagnostika",2015}
    };

    const int cardW=205, cardH=88, gapX=14, gapY=14, cols=5;
    for (int i=0;i<(int)cards.size();++i) {
        int x=25+(i%cols)*(cardW+gapX);
        int y=190+(i/cols)*(cardH+gapY);
        Btn(hwnd,cards[i].title,cards[i].id,x,y,cardW,50);
        Label(hwnd,cards[i].sub,x+3,y+54,cardW-6,26,g_small,SS_CENTER);
    }

    Label(hwnd,L"RYCHLÉ NÁSTROJE",25,510,220,30,g_bold);
    Btn(hwnd,L"RailWorks",3001,25,545,150,36);
    Btn(hwnd,L"Assets",3002,185,545,150,36);
    Btn(hwnd,L"Content",3003,345,545,150,36);
    Btn(hwnd,L"Routes",3004,505,545,150,36);
    Btn(hwnd,L"Plugins",3005,665,545,150,36);

    Label(hwnd,L"LOG",25,600,100,25,g_bold);
    g_log = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,
        25,628,1045,145,hwnd,nullptr,g_inst,nullptr);
    Font(g_log,g_small);
}

void HandleModule(int id) {
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
            else if (id==1002) { DisconnectRailDriver(); g_railWorksPath=FindRailWorks(); ConnectRailDriver(); UpdateStatus(); }
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
    g_inst=h;
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

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
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&wc);

    g_main=CreateWindowExW(0,wc.lpszClassName,L"Kvaltík TSC Hub",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,1120,830,nullptr,nullptr,h,nullptr);

    if (!g_main) return 1;
    ShowWindow(g_main,show);
    UpdateWindow(g_main);

    MSG m{};
    while(GetMessageW(&m,nullptr,0,0)>0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return static_cast<int>(m.wParam);
}
