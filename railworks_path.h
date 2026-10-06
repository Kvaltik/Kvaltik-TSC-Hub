#pragma once
#include <filesystem>
#include <string>

namespace RailWorksPath {
namespace fs = std::filesystem;

inline bool IsFile(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

inline bool IsRoot(const fs::path& path) {
    return IsFile(path / L"RailWorks64.exe") || IsFile(path / L"RailWorks.exe");
}

// Search only the selected location and the known Steam layout, never unrelated drives.
inline std::wstring Resolve(const std::wstring& selection) {
    if (selection.empty()) return {};
    fs::path base(selection);
    std::error_code ec;
    base = fs::absolute(base, ec);
    if (ec) return {};
    base = base.lexically_normal();
    if (IsFile(base)) {
        const auto name = base.filename().wstring();
        if (_wcsicmp(name.c_str(), L"RailWorks64.exe") != 0 &&
            _wcsicmp(name.c_str(), L"RailWorks.exe") != 0) return {};
        base = base.parent_path();
    }
    const fs::path candidates[] = {
        base, base / L"RailWorks", base / L"common" / L"RailWorks",
        base / L"steamapps" / L"common" / L"RailWorks"
    };
    for (const auto& candidate : candidates) {
        if (IsRoot(candidate)) return candidate.wstring();
    }
    return {};
}
} // namespace RailWorksPath
