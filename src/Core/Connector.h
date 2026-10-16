#pragma once
#include "Storage.h"
#include "../../telemetry.h"
#include <tlhelp32.h>
namespace Core {
struct Snapshot {
    bool running=false;
    std::wstring status=L"Nepřipojeno",loco;
    std::vector<Telemetry::Controller> controllers;
};
class Connector {
    HMODULE dll=nullptr;
    using TextFn=const char* (__cdecl*)();
    using ValueFn=float (__cdecl*)(int,int);
    TextFn loco=nullptr,names=nullptr; ValueFn values=nullptr;
public:
    Snapshot snapshot;
    Connector()=default;
    Connector(const Connector&)=delete;
    Connector& operator=(const Connector&)=delete;
    ~Connector(){ Disconnect(); }
    void Disconnect(){ if(dll) FreeLibrary(dll); dll=nullptr;loco=nullptr;names=nullptr;values=nullptr;snapshot={}; }
    bool Connect(const fs::path& root) {
        Disconnect();
        if(root.empty()){snapshot.status=L"Vyber instalaci TSC v Nastavení.";return false;}
        auto path=root/L"plugins"/L"RailDriver64.dll";
        dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dll){snapshot.status=L"RailDriver64.dll nelze načíst. Ověř 64bitovou instalaci a složku plugins. Chyba "+std::to_wstring(GetLastError());return false;}
        loco=reinterpret_cast<TextFn>(GetProcAddress(dll,"GetLocoName"));
        names=reinterpret_cast<TextFn>(GetProcAddress(dll,"GetControllerList"));
        values=reinterpret_cast<ValueFn>(GetProcAddress(dll,"GetControllerValue"));
        if(!loco||!names||!values){Disconnect();snapshot.status=L"RailDriver DLL nemá požadované exporty.";return false;}
        snapshot.status=L"DLL načtena. Čeká na scénář.";return true;
    }
    void Poll() {
        snapshot.controllers.clear();snapshot.loco.clear();snapshot.running=false;
        HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(h!=INVALID_HANDLE_VALUE){PROCESSENTRY32W p{};p.dwSize=sizeof(p);if(Process32FirstW(h,&p))do{if(!_wcsicmp(p.szExeFile,L"RailWorks64.exe"))snapshot.running=true;}while(Process32NextW(h,&p));CloseHandle(h);}
        if(!dll)return;
        if(!snapshot.running){snapshot.status=L"TSC x64 neběží. Spusť hru a načti scénář.";return;}
        const char* name=loco();
        if(name) { try{snapshot.loco=Wide(name);}catch(...){snapshot.loco=L"(název není UTF-8)";} }
        if(snapshot.loco.empty()){snapshot.status=L"Čeká na lokomotivu v načteném scénáři.";return;}
        const char* raw=names();auto list=Telemetry::SplitNames(raw?raw:"");
        for(size_t i=0;i<list.size() && i<10000;++i)snapshot.controllers.push_back({static_cast<int>(i),list[i],values(static_cast<int>(i),0),values(static_cast<int>(i),1),values(static_cast<int>(i),2)});
        snapshot.status=snapshot.controllers.empty()?L"Lokomotiva neposkytuje controllery.":L"Živá telemetrie · pouze čtení";
    }
};
}
