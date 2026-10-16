#define NOMINMAX
#include "../src/Core/Storage.h"
#include "../src/Core/Updater.h"
#include "../src/Train/Journeys.h"
#include <cassert>
#include <iostream>
int main(){
    const std::wstring text=L"Příliš žluťoučký kůň · Žďár nad Sázavou 🚆";
    assert(Core::Wide(Core::Utf8(text))==text);
    bool rejected=false;try{Core::Wide(std::string("\xff"));}catch(...){rejected=true;}assert(rejected);
    assert(Core::ValidProfile(L"Kvaltík"));assert(!Core::ValidProfile(L"   "));assert(!Core::ValidProfile(L"a\nb"));
    assert(Core::Updates::Version("v2.10.0")>Core::Updates::Version("2.9.9"));
    for(const auto* invalid:{"2.0.0-beta","garbage","2.0","v2.0.0/evil","2.0.0suffix"}){rejected=false;try{Core::Updates::Version(invalid);}catch(...){rejected=true;}assert(rejected);}
    assert(Core::Updates::Hash({'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    auto directory=Core::fs::temp_directory_path()/(L"hub2-tests-"+std::to_wstring(GetCurrentProcessId()));Core::fs::create_directories(directory);
    auto path=directory/L"čeština.txt";Core::Write(path,text);assert(Core::Read(path)==text);Core::Write(path,L"nová hodnota");assert(Core::Read(path)==L"nová hodnota");
    Train::Journeys journal(directory);assert(!journal.Active());journal.Begin(L"R 123; \"Praha\"",L"Žabotlam");assert(journal.Active());
    Train::Journeys restarted(directory);assert(restarted.Active());restarted.End();assert(!restarted.Active());assert(restarted.History().find(L"\"R 123; \"\"Praha\"\"\"")!=std::wstring::npos);
    rejected=false;try{restarted.End();}catch(...){rejected=true;}assert(rejected);
    Core::fs::remove_all(directory);
    std::cout<<"Hub 2 core tests passed\n";
}
