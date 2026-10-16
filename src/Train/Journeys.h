#pragma once
#include "../Core/Storage.h"
namespace Train {
inline std::wstring Timestamp(){SYSTEMTIME t{};GetLocalTime(&t);wchar_t b[40]{};swprintf_s(b,L"%04d-%02d-%02d %02d:%02d:%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;}
inline std::wstring Csv(const std::wstring& s){std::wstring out=L"\"";for(auto c:s){out+=c;if(c==L'"')out+=c;}return out+L"\"";}
class Journeys {
    Core::fs::path dir;
public:
    explicit Journeys(Core::fs::path directory):dir(std::move(directory)){}
    bool Active()const{return !Core::Read(dir/L"active-journey.txt").empty();}
    void Begin(const std::wstring& label,const std::wstring& loco){if(Active())throw std::runtime_error("Journey already active");if(label.empty())throw std::runtime_error("Enter journey name");Core::Write(dir/L"active-journey.txt",Csv(Timestamp())+L";"+Csv(label)+L";"+Csv(loco));}
    void End(){auto active=Core::Read(dir/L"active-journey.txt");if(active.empty())throw std::runtime_error("No active journey");auto history=Core::Read(dir/L"journeys.csv");if(history.empty())history=L"start;train;loco;end\r\n";auto row=active+L";"+Csv(Timestamp())+L"\r\n";if(history.find(active+L";")==std::wstring::npos)Core::Write(dir/L"journeys.csv",history+row);Core::Write(dir/L"active-journey.txt",L"");}
    std::wstring History()const{return Core::Read(dir/L"journeys.csv");}
};
}
