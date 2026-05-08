#include "owopdater/client/updater.h"
#include "owopdater/core/updater/installer.h"
#include "owopdater/core/crypto/sha256.h"
#include "owopdater/utils/generic.h"
#include "owopdater/utils/http.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <Windows.h>

namespace owopdater { namespace client {

static std::wstring Utf8ToWstring(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len == 0) return std::wstring();
    std::wstring w;
    w.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

static std::filesystem::path make_temp_path(const std::string& filename)
{
    auto temp = std::filesystem::temp_directory_path();
    return temp / filename;
}

int run_update(const std::string& link, const std::string& absolute_dir, int pid, const std::string& sha256, const std::string& launchProc)
{
    DebugLog("updater starting for link=" + link + " pid=" + std::to_string(pid));
    if (link.empty()) return 2;
    if (absolute_dir.empty()) return 3;

    HttpResponse resp = HttpGetFull(link);
    if (resp.status_code != 200) {
        std::cerr << "download failed: " << resp.reason << std::endl;
        DebugLog("download failed: " + resp.reason);
        return 4;
    }

    if (pid > 0) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (h) {
            TerminateProcess(h, 0);
            WaitForSingleObject(h, 5000);
            CloseHandle(h);
        }
    }

    std::string filename = "downloaded.bin";
    size_t pos = link.find_last_of("/\\");
    if (pos != std::string::npos && pos + 1 < link.size()) filename = link.substr(pos + 1);
    std::filesystem::path tempPath = make_temp_path(filename);
    std::filesystem::create_directories(tempPath.parent_path());

    std::ofstream of(tempPath, std::ios::binary);
    of.write(resp.body.data(), static_cast<std::streamsize>(resp.body.size()));
    of.close();

    DebugLog("download finished writing temp file " + tempPath.string());
    if (!std::filesystem::exists(tempPath)) {
        std::cerr << "download write failed" << std::endl;
        DebugLog("temp file missing after write");
        return 5;
    }

    if (!sha256.empty()) {
        DebugLog("verifying sha256");
        std::string error;
        if (!core::crypto::verify_sha256(tempPath.string(), sha256, error)) {
            std::cerr << "sha256 verification failed: " << error << std::endl;
            DebugLog("sha256 verification failed: " + error);
            return 9;
        }
        DebugLog("sha256 verified successfully");
    }

    DebugLog("calling install with file " + tempPath.string());
    std::string error;
    if (!core::updater::install(tempPath.string(), filename, absolute_dir, pid, error)) {
        std::cerr << error << std::endl;
        DebugLog("install failed: " + error);
        return 6;
    }
    DebugLog("install succeeded");

    if (!launchProc.empty()) {
        try {
            std::filesystem::path exePath = std::filesystem::path(absolute_dir) / launchProc;
            if (std::filesystem::exists(exePath)) {
                std::wstring exeW = Utf8ToWstring(exePath.string());
                std::wstring cwdW = Utf8ToWstring(absolute_dir);
                STARTUPINFOW si;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi;
                ZeroMemory(&pi, sizeof(pi));
                BOOL ok = CreateProcessW(exeW.c_str(), nullptr, nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, cwdW.c_str(), &si, &pi);
                if (ok) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    DebugLog("launched " + exePath.string());
                } else {
                    DWORD err = GetLastError();
                    DebugLog("launch failed: " + std::to_string(err));
                }
            } else {
                DebugLog("launch target missing: " + exePath.string());
            }
        } catch (...) {
            DebugLog("launch exception");
        }
    }

    return 0;
}

}} // namespace owopdater::whyrureadingthis?
