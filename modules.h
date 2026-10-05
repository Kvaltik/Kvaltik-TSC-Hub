#pragma once
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace HubModules {
namespace fs = std::filesystem;

extern HWND g_main;
extern HINSTANCE g_inst;
extern HFONT g_font, g_bold, g_small;
extern std::wstring g_railWorksPath;
extern HMODULE g_railDll;

using GetStringFn = const char* (__cdecl*)();
using GetControllerValueFn = float (__cdecl*)(int,int);
using GetCurrentControllerValueFn = float (__cdecl*)(int);
using SetControllerValueFn = void (__cdecl*)(int,float);

extern GetStringFn pGetLocoName;
extern GetStringFn pGetControllerList;
extern GetControllerValueFn pGetControllerValue;
extern GetCurrentControllerValueFn pGetCurrentControllerValue;
extern SetControllerValueFn pSetControllerValue;

inline std::wstring WidenAnsi(const char* s) {
    if (!s || !*s) return L"";
    int n = MultiByteToWideChar(CP_ACP,0,s,-1,nullptr,0);
    if (n<=1) return L"";
    std::wstring out(n-1,L'\0');
    MultiByteToWideChar(CP_ACP,0,s,-1,out.data(),n);
    return out;
}

inline HWND MkLabel(HWND p,const wchar_t* t,int x,int y,int w,int h,HFONT f) {
    HWND c=CreateWindowExW(0,L"STATIC",t,WS_CHILD|WS_VISIBLE,x,y,w,h,p,nullptr,g_inst,nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)f,TRUE); return c;
}
inline HWND MkButton(HWND p,const wchar_t* t,int id,int x,int y,int w,int h) {
    HWND c=CreateWindowExW(0,L"BUTTON",t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,y,w,h,p,(HMENU)(INT_PTR)id,g_inst,nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)g_bold,TRUE); return c;
}
inline HWND MkEdit(HWND p,const wchar_t* t,int id,int x,int y,int w,int h,DWORD extra=0) {
    HWND c=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",t,WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|extra,x,y,w,h,p,(HMENU)(INT_PTR)id,g_inst,nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)g_font,TRUE); return c;
}
inline std::wstring GetText(HWND h) {
    int n=GetWindowTextLengthW(h); std::wstring s(n,L'\0'); if(n) GetWindowTextW(h,s.data(),n+1); return s;
}
inline std::wstring ModuleTitle(int id) {
    switch(id){
        case 2001:return L"NavTrain"; case 2002:return L"VO79"; case 2003:return L"RailControl";
        case 2004:return L"Scenario Creator"; case 2005:return L"Jízdní řád"; case 2006:return L"Consist Manager";
        case 2007:return L"Scenario Doctor"; case 2008:return L"Kniha jízd"; case 2009:return L"Live Map";
        case 2010:return L"Shader Manager"; case 2011:return L"Route Manager"; case 2012:return L"Rozkazovač";
        case 2013:return L"Výpravčí"; case 2014:return L"Driver Display"; case 2015:return L"TSC Connector";
    } return L"Modul";
}
struct State { int id{}; HWND list{}, value{}, status{}, e1{}, e2{}, e3{}; };

inline std::vector<std::string> ControllerNames() {
    std::vector<std::string> out;
    if(!pGetControllerList) return out;
    const char* raw=pGetControllerList(); if(!raw) return out;
    std::string s(raw); size_t pos=0;
    while(true){
        size_t e=s.find("::",pos); auto part=s.substr(pos,e==std::string::npos?std::string::npos:e-pos);
        if(!part.empty()) out.push_back(part);
        if(e==std::string::npos) break; pos=e+2;
    }
    return out;
}
inline void FillControllers(State* st) {
    if(!st || !st->list) return;
    ListView_DeleteAllItems(st->list);
    auto names=ControllerNames();
    for(int i=0;i<(int)names.size();++i){
        LVITEMW it{}; it.mask=LVIF_TEXT; it.iItem=i;
        std::wstring id=std::to_wstring(i); it.pszText=id.data(); ListView_InsertItem(st->list,&it);
        auto name=WidenAnsi(names[i].c_str());
        ListView_SetItemText(st->list,i,1,name.data());
        float v=pGetCurrentControllerValue?pGetCurrentControllerValue(i):(pGetControllerValue?pGetControllerValue(i,0):0);
        wchar_t b[64]; swprintf_s(b,L"%.3f",v); ListView_SetItemText(st->list,i,2,b);
    }
    if(st->status) {
        std::wstring s=L"Controllerů: "+std::to_wstring(names.size());
        SetWindowTextW(st->status,s.c_str());
    }
}
inline HWND MakeList(HWND hwnd) {
    HWND l=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,
        20,105,740,390,hwnd,(HMENU)5100,g_inst,nullptr);
    ListView_SetExtendedListViewStyle(l,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH;
    c.cx=60; c.pszText=(LPWSTR)L"ID"; ListView_InsertColumn(l,0,&c);
    c.cx=470; c.pszText=(LPWSTR)L"Controller"; ListView_InsertColumn(l,1,&c);
    c.cx=160; c.pszText=(LPWSTR)L"Hodnota"; ListView_InsertColumn(l,2,&c);
    SendMessageW(l,WM_SETFONT,(WPARAM)g_font,TRUE); return l;
}
inline void OpenPath(const std::wstring& p) {
    if(!p.empty()) ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
}
inline std::wstring AppDataFile(const wchar_t* name) {
    wchar_t base[MAX_PATH]{}; GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
    fs::path dir=fs::path(base)/L"KvaltikTSCHub"; std::error_code ec; fs::create_directories(dir,ec);
    return (dir/name).wstring();
}
inline void AppendJourney(const std::wstring& train,const std::wstring& route,const std::wstring& note) {
    auto p=AppDataFile(L"journeys.csv"); bool fresh=!fs::exists(p);
    std::wofstream out(p,std::ios::app); if(!out) return;
    if(fresh) out<<L"date;train;route;loco;note\n";
    SYSTEMTIME st{}; GetLocalTime(&st);
    std::wstring loco=pGetLocoName?WidenAnsi(pGetLocoName()):L"";
    out<<st.wYear<<L"-"<<std::setw(2)<<std::setfill(L'0')<<st.wMonth<<L"-"<<std::setw(2)<<st.wDay<<L";"
       <<train<<L";"<<route<<L";"<<loco<<L";"<<note<<L"\n";
}
inline void BuildModule(HWND hwnd,State* st) {
    auto title=ModuleTitle(st->id);
    MkLabel(hwnd,title.c_str(),20,18,560,38,g_bold);
    std::wstring loco=L"Lokomotiva: "+(pGetLocoName?WidenAnsi(pGetLocoName()):L"—");
    st->status=MkLabel(hwnd,loco.c_str(),20,60,740,28,g_font);

    if(st->id==2015 || st->id==2001 || st->id==2014) {
        st->list=MakeList(hwnd); FillControllers(st);
        MkButton(hwnd,L"Obnovit controllery",5201,20,515,180,34);
        st->value=MkEdit(hwnd,L"0",5202,215,515,100,34);
        if(st->id==2015) MkButton(hwnd,L"Nastavit vybraný",5203,330,515,170,34);
        return;
    }
    if(st->id==2008) {
        MkLabel(hwnd,L"Číslo vlaku",20,115,150,26,g_font); st->e1=MkEdit(hwnd,L"",5301,180,110,220,32);
        MkLabel(hwnd,L"Trať",20,160,150,26,g_font); st->e2=MkEdit(hwnd,L"",5302,180,155,420,32);
        MkLabel(hwnd,L"Poznámka",20,205,150,26,g_font); st->e3=MkEdit(hwnd,L"",5303,180,200,420,32);
        MkButton(hwnd,L"Uložit jízdu",5304,180,250,160,36);
        MkButton(hwnd,L"Otevřít CSV",5305,350,250,160,36);
        return;
    }
    if(st->id==2011 || st->id==2007) {
        st->list=MakeList(hwnd);
        ListView_DeleteColumn(st->list,2); ListView_DeleteColumn(st->list,1);
        LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.cx=650; c.pszText=(LPWSTR)(st->id==2011?L"Route GUID":L"Nalezená položka");
        ListView_SetColumn(st->list,0,&c);
        if(!g_railWorksPath.empty()){
            fs::path root=fs::path(g_railWorksPath)/L"Content"/L"Routes";
            int i=0; std::error_code ec;
            if(fs::exists(root,ec)) for(auto& d:fs::directory_iterator(root,ec)) if(d.is_directory()){
                std::wstring n=d.path().filename().wstring();
                LVITEMW it{};it.mask=LVIF_TEXT;it.iItem=i++;it.pszText=n.data();ListView_InsertItem(st->list,&it);
            }
            SetWindowTextW(st->status,(L"Trasy nalezeny v: "+root.wstring()).c_str());
        }
        MkButton(hwnd,L"Otevřít Routes",5401,20,515,170,34);
        return;
    }
    if(st->id==2006) {
        MkLabel(hwnd,L"Správa souprav používá Train Simulator ConsistTemplates.",20,120,650,30,g_font);
        MkButton(hwnd,L"Otevřít ConsistTemplates",5501,20,175,230,38);
        MkButton(hwnd,L"Otevřít Assets",5502,265,175,170,38);
        return;
    }
    if(st->id==2010) {
        MkLabel(hwnd,L"Správa ReShade / shader presetů pro TSC.",20,120,650,30,g_font);
        MkButton(hwnd,L"Otevřít RailWorks",5601,20,175,180,38);
        MkButton(hwnd,L"Otevřít preset složku",5602,215,175,210,38);
        return;
    }
    if(st->id==2004) {
        MkLabel(hwnd,L"Číslo vlaku",20,115,150,26,g_font); st->e1=MkEdit(hwnd,L"",5701,180,110,220,32);
        MkLabel(hwnd,L"Trať / Route GUID",20,160,150,26,g_font); st->e2=MkEdit(hwnd,L"",5702,180,155,420,32);
        MkButton(hwnd,L"Vytvořit pracovní návrh",5703,180,215,230,38);
        MkLabel(hwnd,L"Generátor vytvoří bezpečný návrh mimo herní data; přímý zápis scénáře přidáme po ověření formátu trasy.",20,280,700,55,g_small);
        return;
    }
    if(st->id==2002) {
        MkLabel(hwnd,L"Kanál",20,120,90,26,g_font); st->e1=MkEdit(hwnd,L"00",5801,115,115,100,32);
        MkLabel(hwnd,L"Volací znak",240,120,110,26,g_font); st->e2=MkEdit(hwnd,L"",5802,355,115,180,32);
        MkButton(hwnd,L"PTT",5803,20,175,120,70);
        MkButton(hwnd,L"DATA",5804,155,175,120,70);
        MkButton(hwnd,L"SYS",5805,290,175,120,70);
        MkLabel(hwnd,L"VO79 panel je připravený na mapování funkcí přes TSC Connector.",20,275,700,30,g_small);
        return;
    }
    if(st->id==2003 || st->id==2009) {
        MkLabel(hwnd,st->id==2003?L"RailControl: dispečerský přehled":L"Live Map: poloha a provoz",20,120,650,30,g_font);
        MkLabel(hwnd,L"Čeká na poziční datový zdroj z TSC. Connector už je společný základ pro napojení.",20,165,700,55,g_small);
        MkButton(hwnd,L"Otevřít Routes",5901,20,235,160,36);
        return;
    }
    if(st->id==2005) {
        MkLabel(hwnd,L"Číslo vlaku",20,120,150,26,g_font); st->e1=MkEdit(hwnd,L"",6001,180,115,220,32);
        MkButton(hwnd,L"Připravit hledání",6002,180,165,190,36);
        MkLabel(hwnd,L"Online zdroj jízdních řádů bude napojen jako samostatný provider.",20,225,700,40,g_small);
        return;
    }
    if(st->id==2012) {
        MkLabel(hwnd,L"Text rozkazu",20,115,150,26,g_font); st->e1=MkEdit(hwnd,L"",6101,20,150,700,120,ES_MULTILINE|ES_AUTOVSCROLL);
        MkButton(hwnd,L"Uložit rozkaz",6102,20,290,170,36);
        return;
    }
    if(st->id==2013) {
        MkLabel(hwnd,L"Český výpravčí / odjezdové eventy",20,120,600,30,g_font);
        MkButton(hwnd,L"Test zvukového signálu",6201,20,175,220,40);
        MkLabel(hwnd,L"Napojení na ScenarioScript eventy zůstává oddělené od Hubu, aby Hub neupravoval scénář bez potvrzení.",20,240,700,55,g_small);
        return;
    }
}
inline void SaveScenarioDraft(State* st) {
    fs::path dir=fs::path(AppDataFile(L"scenario-drafts")); std::error_code ec; fs::create_directories(dir,ec);
    SYSTEMTIME tm{};GetLocalTime(&tm); wchar_t n[80];swprintf_s(n,L"draft_%04d%02d%02d_%02d%02d.txt",tm.wYear,tm.wMonth,tm.wDay,tm.wHour,tm.wMinute);
    std::wofstream out(dir/n); if(out){out<<L"Train="<<GetText(st->e1)<<L"\nRoute="<<GetText(st->e2)<<L"\n";OpenPath((dir/n).wstring());}
}
inline LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    State* st=(State*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_CREATE){
        auto cs=(CREATESTRUCTW*)lp; st=new State(); st->id=(int)(INT_PTR)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)st); BuildModule(hwnd,st); SetTimer(hwnd,1,400,nullptr); return 0;
    }
    if(!st) return DefWindowProcW(hwnd,msg,wp,lp);
    if(msg==WM_TIMER && st->list && (st->id==2015||st->id==2001||st->id==2014)){ FillControllers(st); return 0; }
    if(msg==WM_COMMAND){
        int id=LOWORD(wp);
        if(id==5201) FillControllers(st);
        else if(id==5203 && pSetControllerValue && st->list){
            int row=ListView_GetNextItem(st->list,-1,LVNI_SELECTED);
            if(row>=0){float v=(float)_wtof(GetText(st->value).c_str());pSetControllerValue(row,v);}
        }
        else if(id==5304){AppendJourney(GetText(st->e1),GetText(st->e2),GetText(st->e3));MessageBoxW(hwnd,L"Jízda uložena.",L"Kniha jízd",MB_OK);}
        else if(id==5305) OpenPath(AppDataFile(L"journeys.csv"));
        else if(id==5401||id==5901) OpenPath((fs::path(g_railWorksPath)/L"Content"/L"Routes").wstring());
        else if(id==5501) OpenPath((fs::path(g_railWorksPath)/L"Content"/L"ConsistTemplates").wstring());
        else if(id==5502) OpenPath((fs::path(g_railWorksPath)/L"Assets").wstring());
        else if(id==5601) OpenPath(g_railWorksPath);
        else if(id==5602){fs::path p=fs::path(g_railWorksPath)/L"KvaltikPresets";std::error_code ec;fs::create_directories(p,ec);OpenPath(p.wstring());}
        else if(id==5703) SaveScenarioDraft(st);
        else if(id==5803) SetWindowTextW(st->status,L"VO79: PTT aktivní (lokální režim)");
        else if(id==5804) SetWindowTextW(st->status,L"VO79: DATA");
        else if(id==5805) SetWindowTextW(st->status,L"VO79: SYS");
        else if(id==6002) MessageBoxW(hwnd,L"Provider jízdních řádů bude připojen v další datové vrstvě.",L"Jízdní řád",MB_OK);
        else if(id==6102){std::wofstream out(AppDataFile(L"rozkaz.txt"));out<<GetText(st->e1);MessageBoxW(hwnd,L"Rozkaz uložen.",L"Rozkazovač",MB_OK);}
        else if(id==6201) MessageBeep(MB_ICONINFORMATION);
        return 0;
    }
    if(msg==WM_DESTROY){KillTimer(hwnd,1);delete st;return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
inline void Register() {
    static bool done=false;if(done)return;done=true;
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=Proc;wc.hInstance=g_inst;wc.lpszClassName=L"KvaltikTSCModuleWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassExW(&wc);
}
inline void Open(int id) {
    Register();
    std::wstring title=L"Kvaltík TSC Hub — "+ModuleTitle(id);
    CreateWindowExW(0,L"KvaltikTSCModuleWindow",title.c_str(),WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,810,640,g_main,nullptr,g_inst,(LPVOID)(INT_PTR)id);
}
} // namespace HubModules
