#include "owopdater/core/updater/swapper.h"
#include <filesystem>
#include <system_error>
#include <string>
#include <algorithm>
#include <Windows.h>

namespace owopdater { namespace core { namespace updater {

static bool clear_dir(const std::filesystem::path& dir, std::string& error) noexcept {
    std::error_code ec;

    std::filesystem::path currentExe;
    wchar_t modbuf[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, modbuf, MAX_PATH) != 0) {
        currentExe = std::filesystem::path(modbuf);
    }

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        return s;
    };

    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            error = "dir iter error";
            return false;
        }

        if (!currentExe.empty()) {
            std::error_code eqec;
            if (std::filesystem::exists(entry.path(), eqec) && std::filesystem::exists(currentExe, eqec)) {
                std::error_code eqec2;
                bool same = std::filesystem::equivalent(entry.path(), currentExe, eqec2);
                if (!eqec2 && same) continue;
            }
        }

        std::string stem = entry.path().stem().string();
        std::string ext = entry.path().extension().string();
        if (!stem.empty()) {
            std::string sstem = lower(stem);
            std::string sext = lower(ext);
            if (sstem == "owopdater" && sext == ".exe") {
                continue;
            }
        }

        for (int attempt = 0; attempt < 10; ++attempt) {
            std::filesystem::remove_all(entry.path(), ec);
            if (!ec) break;
            if (attempt < 9) Sleep(1000);
        }
        if (ec) {
            error = "failed clear " + entry.path().string();
            return false;
        }
    }
    return true;
}

static bool copy_tree(const std::filesystem::path& source, const std::filesystem::path& target, std::string& error) noexcept {
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(source, ec)) {
        std::filesystem::path rel = std::filesystem::relative(entry.path(), source, ec);
        if (ec) {
            error = "failed relative path";
            return false;
        }
        std::filesystem::path dest = target / rel;
        if (entry.is_directory(ec)) {
            std::filesystem::create_directories(dest, ec);
            if (ec) {
                error = "failed create dir " + dest.string();
                return false;
            }
            continue;
        }
        std::filesystem::create_directories(dest.parent_path(), ec);
        if (ec) {
            error = "failed create parent " + dest.parent_path().string();
            return false;
        }
        std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            error = "failed copy " + entry.path().string();
            return false;
        }
    }
    return true;
}

bool swapper(const std::string& source, const std::string& targetDir, std::string& error) noexcept {
    std::error_code ec;
    std::filesystem::path sourcePath(source);
    std::filesystem::path targetPath(targetDir);
    if (source.empty() || targetDir.empty()) {
        error = "invalid path";
        return false;
    }
    if (!std::filesystem::exists(sourcePath, ec)) {
        error = "source missing";
        return false;
    }
    if (ec) {
        error = "source access error";
        return false;
    }
    std::filesystem::create_directories(targetPath, ec);
    if (ec) {
        error = "target create failed";
        return false;
    }
    if (!clear_dir(targetPath, error)) return false;
    if (std::filesystem::is_directory(sourcePath, ec)) {
        if (ec) {
            error = "source type error";
            return false;
        }
        return copy_tree(sourcePath, targetPath, error);
    }
    std::filesystem::path dest = targetPath / sourcePath.filename();
    std::filesystem::copy_file(sourcePath, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "copy file failed";
        return false;
    }
    return true;
}

}}}
