#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
namespace Core {
namespace fs = std::filesystem;
inline std::string Utf8(const std::wstring& value) {
    if(value.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(!n) throw std::runtime_error("Invalid Unicode");
    std::string out(n,0); WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),n,nullptr,nullptr); return out;
}
inline std::wstring Wide(const std::string& value) {
    if(value.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring out(n,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),n); return out;
}
inline fs::path DataDirectory() {
    PWSTR path=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path))) throw std::runtime_error("AppData unavailable");
    fs::path dir=fs::path(path)/L"KvaltikTSCHub"/L"v2"; CoTaskMemFree(path); fs::create_directories(dir); return dir;
}
inline std::wstring Read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) { if(!fs::exists(path)) return {}; throw std::runtime_error("Cannot read file"); }
    std::string bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.size()>4*1024*1024) throw std::runtime_error("File too large");
    return Wide(bytes);
}
inline void Write(const fs::path& path,const std::wstring& value) {
    const auto temp=fs::path(path.wstring()+L".tmp"); const auto bytes=Utf8(value);
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out.write(bytes.data(),bytes.size()); out.flush(); if(!out) throw std::runtime_error("Cannot save file"); }
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace file");
}
struct Settings {
    fs::path directory=DataDirectory();
    std::wstring railworks,profile;
    bool remember=false;
    void Load() { railworks=Read(directory/L"railworks.txt"); profile=Read(directory/L"profile.txt"); remember=!profile.empty(); }
    void Save() const { Write(directory/L"railworks.txt",railworks); Write(directory/L"profile.txt",remember?profile:L""); }
};
// Local Windows profile only; never represented as server authentication.
inline bool ValidProfile(const std::wstring& name) {
    return !name.empty() && name.size()<=64 && name.find_first_not_of(L" \t")!=std::wstring::npos && name.find_first_of(L"\r\n") == std::wstring::npos;
}
}
