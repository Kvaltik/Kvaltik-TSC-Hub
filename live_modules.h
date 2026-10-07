#pragma once
#include "telemetry.h"
#include "nav_route.h"
#include <commdlg.h>
#pragma comment(lib,"comdlg32.lib")

namespace LiveModules {
using ValuesFn=float (__cdecl*)(int,int);
static ValuesFn values=nullptr;
static GetStringFn names=nullptr;
static std::vector<Telemetry::Controller> controllers;
static std::wstring loco, status=L"Čeká na připojení";
static HWND connector{}, display{}, navigation{};
static bool positionAvailable=false;
static double latitude=0,longitude=0;
struct WindowState { int id; HWND status{},list{},speed{},loco{}; std::string signature; HWND destination{},distance{},previous{},next{}; std::vector<Navigation::Stop> stops; size_t stopIndex=0; };
inline std::wstring Number(float value) {
    if(!Telemetry::Valid(value)) return L"—";
    wchar_t text[48]{}; swprintf_s(text,L"%.3f",value); return text;
}
inline void Reset() { names=nullptr; values=nullptr; controllers.clear(); loco.clear(); positionAvailable=false; status=L"Odpojeno"; }
inline bool Bind(HMODULE dll) {
    names=reinterpret_cast<GetStringFn>(GetProcAddress(dll,"GetControllerList"));
    values=reinterpret_cast<ValuesFn>(GetProcAddress(dll,"GetControllerValue"));
    return names && values;
}
inline void Poll(bool running) {
    controllers.clear(); loco.clear(); positionAvailable=false;
    if(!running) {status=L"TSC neběží. Spusť hru a načti scénář."; return;}
    if(!g_railDll) {status=L"RailDriver není připojen. Zkontroluj cestu ke hře a plugins\\RailDriver64.dll."; return;}
    if(!names || !values || !pGetLocoName) {status=L"DLL neposkytuje požadované funkce telemetrie.";return;}
    loco=AnsiToWide(pGetLocoName());
    if(loco.empty()) {status=L"Čeká na lokomotivu. Načti scénář a přepni se do kabiny.";return;}
    const char* raw=names();
    auto list=Telemetry::SplitNames(raw?raw:"");
    for(size_t i=0;i<list.size();++i) {
        // Empty names still occupy an ID in the RailDriver list.
        controllers.push_back({static_cast<int>(i),list[i],values(static_cast<int>(i),0),values(static_cast<int>(i),1),values(static_cast<int>(i),2)});
    }
    latitude=values(400,0);longitude=values(401,0);
    positionAvailable=!controllers.empty() && Telemetry::Valid(static_cast<float>(latitude)) && Telemetry::Valid(static_cast<float>(longitude)) && std::abs(latitude)<=90 && std::abs(longitude)<=180 && (latitude!=0 || longitude!=0);
    status=controllers.empty()?L"Lokomotiva nevrátila žádné controllery.":L"Živá data · "+std::to_wstring(controllers.size())+L" controllerů · pouze čtení";
}
inline HWND Label(HWND parent,const wchar_t* text,int x,int y,int w,int h,HFONT font) {
    HWND child=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|(wcschr(text,L'\n')?0:SS_ENDELLIPSIS),x,y,w,h,parent,nullptr,g_inst,nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return child;
}
inline void Refresh(WindowState* s) {
    SetWindowTextW(s->status,status.c_str()); SetWindowTextW(s->loco,loco.empty()?L"Lokomotiva: —":loco.c_str());
    if(s->speed) {
        float speed=0; wchar_t text[64]{};
        if(Telemetry::SpeedKph(controllers,speed)) swprintf_s(text,L"%.1f km/h",speed);
        else wcscpy_s(text,L"— km/h");
        SetWindowTextW(s->speed,text);
    }
    if(s->destination) {
        std::wstring destination=L"Načti seznam zastávek ve formátu CSV.";
        std::wstring distance=L"Poloha vlaku není dostupná.";
        if(!s->stops.empty()) {
            const auto& stop=s->stops[s->stopIndex];
            destination=std::to_wstring(s->stopIndex+1)+L" / "+std::to_wstring(s->stops.size())+L" · "+KvaltikUpdater::Utf8ToWide(stop.name);
            if(positionAvailable) {
                wchar_t text[128]{};swprintf_s(text,L"%.2f km vzdušnou čarou",Navigation::DistanceKm(latitude,longitude,stop));distance=text;
            }
        } else distance=L"Soubor UTF-8: name;latitude;longitude";
        SetWindowTextW(s->destination,destination.c_str());SetWindowTextW(s->distance,distance.c_str());
        EnableWindow(s->previous,!s->stops.empty() && s->stopIndex>0);
        EnableWindow(s->next,!s->stops.empty() && s->stopIndex+1<s->stops.size());
    }
    if(!s->list) return;
    std::string signature; HWND destination{},distance{},previous{},next{}; std::vector<Navigation::Stop> stops; size_t stopIndex=0;
    for(const auto& c:controllers) signature+=std::to_string(c.name.size())+":"+c.name;
    const bool rebuild=signature!=s->signature;
    SendMessageW(s->list,WM_SETREDRAW,FALSE,0);
    if(rebuild) { ListView_DeleteAllItems(s->list); s->signature=signature; }
    for(int i=0;i<static_cast<int>(controllers.size());++i) {
        const auto& c=controllers[i];
        if(rebuild) {
            auto id=std::to_wstring(c.id); LVITEMW item{}; item.mask=LVIF_TEXT;item.iItem=i;item.pszText=id.data();
            ListView_InsertItem(s->list,&item);
            auto name=c.name.empty()?L"(bez názvu)":AnsiToWide(c.name.c_str());
            ListView_SetItemText(s->list,i,1,name.data());
        }
        auto value=Number(c.value),minimum=Number(c.minimum),maximum=Number(c.maximum);
        ListView_SetItemText(s->list,i,2,value.data());
        ListView_SetItemText(s->list,i,3,minimum.data());
        ListView_SetItemText(s->list,i,4,maximum.data());
    }
    SendMessageW(s->list,WM_SETREDRAW,TRUE,0); InvalidateRect(s->list,nullptr,FALSE);
}
inline HWND Button(HWND window,const wchar_t* title,int id,int x,int y,int width) {
    HWND result=CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,width,40,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),g_inst,nullptr);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(g_bold),TRUE);return result;
}
inline void LoadRoute(HWND window,WindowState* s) {
    wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;
    dialog.lpstrFile=path;dialog.nMaxFile=ARRAYSIZE(path);dialog.lpstrFilter=L"Seznam zastávek (*.csv)\0*.csv\0\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog)) return;
    try {
        std::ifstream input{std::filesystem::path(path)}; if(!input)throw std::runtime_error("Cannot open route");
        auto stops=Navigation::Parse(input);s->stops=std::move(stops);s->stopIndex=0;Refresh(s);
    } catch(const std::exception&) {
        MessageBoxW(window,L"Soubor nelze načíst. Očekává se UTF-8 CSV s hlavičkou name;latitude;longitude a desetinnou tečkou. Každá zastávka musí mít název a platné souřadnice. Původní trasa zůstala zachována.",L"NavTrain",MB_ICONWARNING);
    }
}
inline LRESULT CALLBACK Proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    auto s=reinterpret_cast<WindowState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE) {
        auto cs=reinterpret_cast<CREATESTRUCTW*>(lp); s=new WindowState{};s->id=static_cast<int>(reinterpret_cast<INT_PTR>(cs->lpCreateParams));
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
        Label(window,s->id==2015?L"TSC Connector":(s->id==2001?L"NavTrain":L"Driver Display"),24,20,740,44,g_title);
        s->status=Label(window,L"",24,78,740,44,g_font);
        s->loco=Label(window,L"",24,126,740,28,g_small);
        if(s->id==2015) {
            s->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL,24,172,740,370,window,nullptr,g_inst,nullptr);
            ListView_SetExtendedListViewStyle(s->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(s->list,HubUi::Surface);ListView_SetTextBkColor(s->list,HubUi::Surface);ListView_SetTextColor(s->list,HubUi::Text);
            SendMessageW(s->list,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
            const wchar_t* titles[]={L"ID",L"Controller",L"Hodnota",L"Minimum",L"Maximum"};
            const int widths[]={55,335,110,110,110};
            for(int i=0;i<5;++i) {LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.cx=widths[i];c.pszText=const_cast<LPWSTR>(titles[i]);ListView_InsertColumn(s->list,i,&c);}
        } else if(s->id==2001) {
            s->destination=Label(window,L"",24,182,740,44,g_bold);
            s->distance=Label(window,L"",24,238,740,40,g_title);
            Button(window,L"Načíst zastávky",7001,24,304,180);
            s->previous=Button(window,L"Předchozí",7002,220,304,160);
            s->next=Button(window,L"Další zastávka",7003,396,304,180);
            Label(window,L"Zastávku přepínáš ručně. Vzdálenost je vzdušná, ne po kolejích.\nPoloha závisí na datech poskytovaných hrou.\nTato verze neurčuje návěsti ani rychlostní omezení.",24,370,740,100,g_font);
        } else {
            s->speed=Label(window,L"— km/h",24,182,740,64,g_title);
            Label(window,L"Rychlost z SpeedometerKPH nebo SpeedometerMPH.\nPokud lokomotiva údaj neposkytuje, zobrazí se pomlčka.\nOkno můžeš přesunout na druhý monitor.",24,278,740,100,g_font);
        }
        Refresh(s);SetTimer(window,1,1000,nullptr);return 0;
    }
    if(msg==WM_DRAWITEM) {HubUi::DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;}
    if(msg==WM_COMMAND && s && s->id==2001) {
        if(LOWORD(wp)==7001)LoadRoute(window,s);
        else if(LOWORD(wp)==7002 && s->stopIndex>0)--s->stopIndex;
        else if(LOWORD(wp)==7003 && s->stopIndex+1<s->stops.size())++s->stopIndex;
        Refresh(s);return 0;
    }
    if(msg==WM_CTLCOLORSTATIC) {auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,HubUi::Text);SetBkColor(dc,HubUi::Background);return reinterpret_cast<LRESULT>(HubUi::backgroundBrush);}
    if(msg==WM_GETMINMAXINFO) {reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={820,540};return 0;}
    if(msg==WM_SIZE && s) {
        RECT r{};GetClientRect(window,&r);int width=(std::max)(100,static_cast<int>(r.right)-48);
        MoveWindow(s->status,24,78,width,44,TRUE);MoveWindow(s->loco,24,126,width,28,TRUE);
        if(s->list) MoveWindow(s->list,24,172,width,(std::max)(80,static_cast<int>(r.bottom)-196),TRUE);
        return 0;
    }
    if(msg==WM_TIMER && s) {Refresh(s);return 0;}
    if(msg==WM_DESTROY && s) {KillTimer(window,1);if(s->id==2015)connector=nullptr;else if(s->id==2001)navigation=nullptr;else display=nullptr;delete s;SetWindowLongPtrW(window,GWLP_USERDATA,0);return 0;}
    return DefWindowProcW(window,msg,wp,lp);
}
inline void Open(int id) {
    HWND& existing=id==2015?connector:(id==2001?navigation:display);
    if(existing) {ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);return;}
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=Proc;wc.hInstance=g_inst;wc.lpszClassName=L"KvaltikLiveModule";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=HubUi::backgroundBrush;
    RegisterClassExW(&wc);
    existing=CreateWindowExW(0,wc.lpszClassName,id==2015?L"TSC Connector":(id==2001?L"NavTrain":L"Driver Display"),WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,820,610,g_main,nullptr,g_inst,reinterpret_cast<LPVOID>(static_cast<INT_PTR>(id)));
}
}
