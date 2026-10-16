#pragma once
#include "Storage.h"
#include "../../railworks_path.h"
#include <regex>
#include <sstream>
namespace Core {
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
    std::wistringstream in(Core::Read(vdf));
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


}
