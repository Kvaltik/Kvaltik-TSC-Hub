#include "../railworks_path.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
int main() {
    const auto fixture = fs::temp_directory_path() / (L"tsc-path-tests-" + std::to_wstring(GetCurrentProcessId()));
    if (!fs::create_directory(fixture)) return 1;
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
        if (!ok) ++failures;
    };
    try {
        const auto library = fixture / L"Steam knihovna žluťoučká";
        const auto root = library / L"steamapps" / L"common" / L"RailWorks";
        fs::create_directories(root);
        std::ofstream(root / L"RailWorks64.exe").put('x');
        auto resolves = [&](const fs::path& path) {
            const auto result = RailWorksPath::Resolve(path.wstring());
            return !result.empty() && fs::equivalent(fs::path(result), root);
        };
        check(resolves(root), "RailWorks root");
        check(resolves(library), "Steam library with spaces and Unicode");
        check(resolves(root.parent_path()), "common folder");
        check(resolves(root.parent_path().parent_path()), "steamapps folder");
        check(resolves(root / L"RailWorks64.exe"), "64-bit executable");
        check(resolves(root / L"RAILWORKS64.EXE"), "case-insensitive executable");
        check(resolves(fs::path(root.wstring() + L"\\")), "trailing separator");
        check(resolves(root / L".." / L"RailWorks"), "dot segments");
        const auto previousDirectory = fs::current_path();
        fs::current_path(fixture);
        check(resolves(root.lexically_relative(fixture)), "relative path");
        fs::current_path(previousDirectory);
        check(RailWorksPath::Resolve(L"").empty(), "empty selection");
        check(RailWorksPath::Resolve((fixture / L"missing").wstring()).empty(), "missing path");
        check(RailWorksPath::Resolve(fixture.wstring()).empty(), "no unrelated recursive search");
        std::ofstream(root / L"other.exe").put('x');
        check(RailWorksPath::Resolve((root / L"other.exe").wstring()).empty(), "unrelated file rejected");
        fs::remove(root / L"RailWorks64.exe");
        fs::create_directory(root / L"RailWorks64.exe");
        check(RailWorksPath::Resolve(root.wstring()).empty(), "directory named exe rejected");
        std::ofstream(root / L"RailWorks.exe").put('x');
        check(resolves(library), "32-bit-only installation");
        check(resolves(root / L"RailWorks.exe"), "32-bit executable");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        ++failures;
    }
    fs::remove_all(fixture);
    return failures ? 1 : 0;
}
