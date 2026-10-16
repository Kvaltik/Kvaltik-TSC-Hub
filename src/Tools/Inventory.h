#pragma once
#include "../Core/Storage.h"
#include <sstream>
namespace Tools {
inline std::wstring Inventory(const Core::fs::path& root,const Core::fs::path& sub) {
    if(root.empty())return L"Nejprve vyber instalaci v Nastavení.";
    auto path=root/sub;std::error_code ec;
    if(!Core::fs::is_directory(path,ec))return L"Složka není dostupná: "+path.wstring();
    std::wstring result=path.wstring()+L"\r\n\r\n";size_t count=0;
    Core::fs::directory_iterator it(path,ec),end;
    while(!ec && it!=end && count<2000){result+=it->path().filename().wstring()+L"\r\n";++count;it.increment(ec);}
    if(ec)result+=L"\r\nČást složky nelze přečíst: "+Core::Wide(ec.message());
    if(count==2000)result+=L"\r\nVýpis omezen na 2000 položek.";
    if(count==0)result+=L"Složka je prázdná.";
    return result;
}
}
