#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace KvaltikUpdater {

static const wchar_t* CURRENT_VERSION = L"0.2.0";

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
    return out;
}

inline bool HttpGet(const wchar_t* host, const wchar_t* path, std::vector<unsigned char>& data) {
    HINTERNET session = WinHttpOpen(L"KvaltikTSC-Hub/0.2",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers = L"Accept: application/vnd.github+json\r\n"
                             L"X-GitHub-Api-Version: 2022-11-28\r\n";
    WinHttpAddRequestHeaders(request, headers, -1, WINHTTP_ADDREQ_FLAG_ADD);

    bool ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);

    if (ok) {
        DWORD status = 0, statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        ok = (status >= 200 && status < 300);
    }

    if (ok) {
        while (true) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
            size_t old = data.size();
            data.resize(old + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, data.data() + old, available, &read)) {
                ok = false;
                break;
            }
            data.resize(old + read);
            if (read == 0) break;
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

inline bool GetLatestVersion(std::wstring& version) {
    std::vector<unsigned char> bytes;
    if (!HttpGet(L"api.github.com", L"/repos/Kvaltik/Kvaltik-TSC-Hub/releases/latest", bytes))
        return false;

    std::string json(bytes.begin(), bytes.end());
    std::string key = "\"tag_name\":\"";
    size_t p = json.find(key);
    if (p == std::string::npos) return false;
    p += key.size();
    size_t e = json.find('"', p);
    if (e == std::string::npos) return false;

    std::string tag = json.substr(p, e - p);
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
        tag.erase(tag.begin());

    version = Utf8ToWide(tag);
    return !version.empty();
}

inline std::vector<int> ParseVersion(const std::wstring& v) {
    std::vector<int> out;
    std::wstringstream ss(v);
    std::wstring part;
    while (std::getline(ss, part, L'.')) {
        try { out.push_back(std::stoi(part)); }
        catch (...) { out.push_back(0); }
    }
    while (out.size() < 3) out.push_back(0);
    return out;
}

inline bool IsNewer(const std::wstring& latest) {
    auto a = ParseVersion(latest);
    auto b = ParseVersion(CURRENT_VERSION);
    size_t count = a.size() > b.size() ? a.size() : b.size();
    for (size_t i = 0; i < count; ++i) {
        int av = i < a.size() ? a[i] : 0;
        int bv = i < b.size() ? b[i] : 0;
        if (av > bv) return true;
        if (av < bv) return false;
    }
    return false;
}

inline bool DownloadLatestExe(const std::wstring& targetPath) {
    std::vector<unsigned char> bytes;
    if (!HttpGet(L"github.com",
        L"/Kvaltik/Kvaltik-TSC-Hub/releases/latest/download/KvaltikTSCHub.exe",
        bytes)) return false;

    std::ofstream out(targetPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    return out.good();
}

inline bool ScheduleReplaceAndRestart(const std::wstring& newExePath) {
    wchar_t current[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, current, MAX_PATH)) return false;

    std::wstring batPath = std::wstring(current) + L".update.bat";
    std::wofstream bat(batPath);
    if (!bat) return false;

    bat << L"@echo off\n";
    bat << L"chcp 65001 >nul\n";
    bat << L"timeout /t 2 /nobreak >nul\n";
    bat << L":retry\n";
    bat << L"move /Y \"" << newExePath << L"\" \"" << current << L"\" >nul 2>&1\n";
    bat << L"if errorlevel 1 (timeout /t 1 /nobreak >nul & goto retry)\n";
    bat << L"start \"\" \"" << current << L"\"\n";
    bat << L"del \"%~f0\"\n";
    bat.close();

    ShellExecuteW(nullptr, L"open", batPath.c_str(), nullptr, nullptr, SW_HIDE);
    return true;
}

inline bool CheckAndUpdate(HWND owner, bool silentIfCurrent = false) {
    std::wstring latest;
    if (!GetLatestVersion(latest)) {
        if (!silentIfCurrent)
            MessageBoxW(owner, L"Nepodařilo se zkontrolovat aktualizace.", L"Kvaltík TSC Hub", MB_ICONWARNING);
        return false;
    }

    if (!IsNewer(latest)) {
        if (!silentIfCurrent) {
            std::wstring msg = L"Máš aktuální verzi " + std::wstring(CURRENT_VERSION) + L".";
            MessageBoxW(owner, msg.c_str(), L"Aktualizace", MB_ICONINFORMATION);
        }
        return false;
    }

    std::wstring msg = L"Je dostupná nová verze " + latest +
        L".\n\nChceš ji stáhnout a nainstalovat?";
    if (MessageBoxW(owner, msg.c_str(), L"Nová aktualizace", MB_YESNO | MB_ICONINFORMATION) != IDYES)
        return false;

    wchar_t current[MAX_PATH]{};
    GetModuleFileNameW(nullptr, current, MAX_PATH);
    std::wstring newExe = std::wstring(current) + L".new";

    if (!DownloadLatestExe(newExe)) {
        MessageBoxW(owner, L"Stažení nové verze se nepodařilo.", L"Aktualizace", MB_ICONERROR);
        return false;
    }

    if (!ScheduleReplaceAndRestart(newExe)) {
        MessageBoxW(owner, L"Nepodařilo se připravit instalaci aktualizace.", L"Aktualizace", MB_ICONERROR);
        return false;
    }

    MessageBoxW(owner, L"Aktualizace byla stažena. Hub se nyní restartuje.", L"Aktualizace", MB_ICONINFORMATION);
    PostMessageW(owner, WM_CLOSE, 0, 0);
    return true;
}

} // namespace KvaltikUpdater
