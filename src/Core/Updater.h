#pragma once
#include "Storage.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <regex>
#include <array>
#include <vector>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"bcrypt.lib")
#ifndef HUB_VERSION
#define HUB_VERSION "2.0.0"
#endif
namespace Core::Updates {
inline std::array<int,3> Version(const std::string& text) {
    std::smatch m;
    if(!std::regex_match(text,m,std::regex(R"(v?([0-9]{1,6})\.([0-9]{1,6})\.([0-9]{1,6}))"))) throw std::runtime_error("Invalid release version");
    return {std::stoi(m[1]),std::stoi(m[2]),std::stoi(m[3])};
}
struct Internet { HINTERNET h; ~Internet(){if(h)WinHttpCloseHandle(h);} operator HINTERNET()const{return h;} };
inline std::vector<unsigned char> Get(const wchar_t* host,const std::wstring& path,size_t limit) {
    Internet session{WinHttpOpen(L"KvaltikTSCHub/2.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};
    if(!session.h)throw std::runtime_error("Network initialization failed");
    WinHttpSetTimeouts(session,10000,10000,15000,15000);
    Internet connection{WinHttpConnect(session,host,INTERNET_DEFAULT_HTTPS_PORT,0)};
    Internet request{connection.h?WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE):nullptr};
    if(!request.h)throw std::runtime_error("Network request failed");
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request,L"Accept: application/vnd.github+json\r\n",-1,nullptr,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("Network unavailable");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)||status!=200)throw std::runtime_error("Release unavailable (HTTP "+std::to_string(status)+")");
    std::vector<unsigned char> result; unsigned char buffer[16384]; DWORD count=0;
    do { if(!WinHttpReadData(request,buffer,sizeof(buffer),&count))throw std::runtime_error("Download interrupted"); if(result.size()+count>limit)throw std::runtime_error("Download too large");result.insert(result.end(),buffer,buffer+count); }while(count);
    return result;
}
inline std::string Latest() {
    auto bytes=Get(L"api.github.com",L"/repos/Kvaltik/Kvaltik-TSC-Hub/releases/latest",1024*1024);
    std::string json(bytes.begin(),bytes.end());std::smatch m;
    if(!std::regex_search(json,m,std::regex(R"tag("tag_name"\s*:\s*"(v?[0-9]+\.[0-9]+\.[0-9]+)")tag")))throw std::runtime_error("Invalid release metadata");
    Version(m[1]);return m[1];
}
inline std::string Hash(const std::vector<unsigned char>& data) {
    unsigned char digest[32]{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(data.data()),static_cast<ULONG>(data.size()),digest,32)<0)throw std::runtime_error("SHA256 failed");
    std::string out;const char* hex="0123456789abcdef";for(auto c:digest){out+=hex[c>>4];out+=hex[c&15];}return out;
}
inline fs::path Executable(){wchar_t path[32768]{};DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)throw std::runtime_error("Executable path unavailable");return path;}
inline std::wstring Quote(const std::wstring& s){if(s.find(L'"')!=std::wstring::npos)throw std::runtime_error("Invalid path");return L"\""+s+L"\"";}
inline bool Start(const fs::path& exe,std::wstring arguments) {
    std::wstring command=Quote(exe.wstring())+L" "+arguments;STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,exe.parent_path().c_str(),&startup,&process))return false;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
inline void Stage(const std::string& tag) {
    Version(tag);auto target=Executable();auto staging=fs::path(target.wstring()+L".new");
    const auto base=L"/Kvaltik/Kvaltik-TSC-Hub/releases/download/"+Wide(tag)+L"/KvaltikTSCHub.exe";
    auto checksum=Get(L"github.com",base+L".sha256",4096);auto bytes=Get(L"github.com",base,100*1024*1024);
    std::string expected(checksum.begin(),checksum.end());
    if(expected.size()<64||Hash(bytes)!=expected.substr(0,64)||bytes.size()<64||bytes[0]!='M'||bytes[1]!='Z')throw std::runtime_error("Release checksum mismatch");
    {std::ofstream out(staging,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.flush();if(!out)throw std::runtime_error("Install directory is not writable");}
    auto helper=DataDirectory()/(L"updater-"+std::to_wstring(GetCurrentProcessId())+L".exe");
    fs::copy_file(target,helper,fs::copy_options::overwrite_existing);
    if(!Start(helper,L"--apply-update "+std::to_wstring(GetCurrentProcessId())+L" "+Quote(target.wstring())))throw std::runtime_error("Cannot start updater");
}
inline int Apply(DWORD pid,const fs::path& target) {
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
    if(parent){DWORD result=WaitForSingleObject(parent,30000);CloseHandle(parent);if(result!=WAIT_OBJECT_0)return 2;}
    auto staging=fs::path(target.wstring()+L".new"),backup=fs::path(target.wstring()+L".bak");
    if(!ReplaceFileW(target.c_str(),staging.c_str(),backup.c_str(),0,nullptr,nullptr)){Write(DataDirectory()/L"update-result.txt",L"Aktualizace selhala. Původní EXE nebylo nahrazeno. Chyba "+std::to_wstring(GetLastError()));return 3;}
    if(!Start(target,L"")){ReplaceFileW(target.c_str(),backup.c_str(),nullptr,0,nullptr,nullptr);Write(DataDirectory()/L"update-result.txt",L"Novou verzi nelze spustit. Obnovena záloha.");return 4;}
    Write(DataDirectory()/L"update-result.txt",L"Aktualizace byla nainstalována. Předchozí EXE je v souboru .bak.");return 0;
}
}
