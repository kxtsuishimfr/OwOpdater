#include "owopdater/core/updater/installer.h"
#include "owopdater/core/io/file_type.h"
#include "owopdater/core/updater/swapper.h"
#include "owopdater/core/updater/unzipper.h"
#include <Windows.h>
#include <tlhelp32.h>
#include <filesystem>

namespace owopdater { namespace core { namespace updater {

static bool enable_debug_privilege_noexcept(std::string& /*ohno*/) noexcept {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return false;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid)) { CloseHandle(token); return false; }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(token);
    return err == ERROR_SUCCESS;
}

static bool kill_process_tree(DWORD pid, std::string& error) noexcept {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (pe.th32ParentProcessID == pid) {
                    kill_process_tree(pe.th32ProcessID, error);
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }

    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!h) {
        DWORD code = GetLastError();
        if (code == ERROR_INVALID_PARAMETER || code == ERROR_NOT_FOUND) return true;
        enable_debug_privilege_noexcept(error);
        h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!h) {
            error = "failed open process";
            return false;
        }
    }

    BOOL terminated = FALSE;
    for (int attempt = 0; attempt < 10; ++attempt) {
        terminated = TerminateProcess(h, 1);
        if (terminated) {
            DWORD waitResult = WaitForSingleObject(h, 2000);
            if (waitResult != WAIT_TIMEOUT) break;
        }
        Sleep(200);
    }
    CloseHandle(h);
    if (!terminated) {
        error = "failed terminate process";
        return false;
    }
    return true;
}

static bool kill_pid(int pid, std::string& error) noexcept {
    if (pid <= 0) return true;
    enable_debug_privilege_noexcept(error);
    return kill_process_tree(static_cast<DWORD>(pid), error);
}

bool install(const std::string& sourceFile, const std::string& filename, const std::string& targetDir, int pid, std::string& error) noexcept {
    if (sourceFile.empty() || filename.empty() || targetDir.empty()) {
        error = "invalid path";
        return false;
    }
    auto type = core::io::detect(filename);
    if (type == core::io::FileType::Unknown) {
        error = "unknown file type";
        return false;
    }
    if (!kill_pid(pid, error)) return false;
    Sleep(2000);
    if (type == core::io::FileType::Zip) {
        std::filesystem::path tempExtract = std::filesystem::path(sourceFile);
        tempExtract.replace_extension(".unzipped");
        std::error_code ec;
        std::filesystem::remove_all(tempExtract, ec);
        std::filesystem::create_directories(tempExtract, ec);
        if (ec) {
            error = "extract dir failed";
            return false;
        }
        if (!unzip(sourceFile, tempExtract.string(), error)) return false;
        if (!swapper(tempExtract.string(), targetDir, error)) return false;
        return true;
    }
    if (!swapper(sourceFile, targetDir, error)) return false;
    return true;
}

}}}
