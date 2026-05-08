#include "owopdater/core/updater/installer.h"
#include "owopdater/core/io/file_type.h"
#include "owopdater/core/updater/swapper.h"
#include "owopdater/core/updater/unzipper.h"
#include <Windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include "owopdater/utils/generic.h"
#include <algorithm>
#include <vector>

namespace owopdater { namespace core { namespace updater {

static bool enable_debug_privilege_noexcept(std::string&) noexcept {
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
    DWORD currentPid = GetCurrentProcessId();
    DWORD parentOfCurrent = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (pe.th32ProcessID == currentPid) parentOfCurrent = pe.th32ParentProcessID;
                if (pe.th32ParentProcessID == pid) {
                    if (pe.th32ProcessID == currentPid) {
                        DebugLog(std::string("kill_process_tree: skipping current process child ") + std::to_string(pe.th32ProcessID));
                    } else {
                        kill_process_tree(pe.th32ProcessID, error);
                    }
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }

    if (pid == currentPid) {
        DebugLog(std::string("kill_process_tree: skip terminating self pid=") + std::to_string(pid));
        return true;
    }

    
    if (parentOfCurrent != 0 && pid == parentOfCurrent) {
        HANDLE hp = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!hp) {
            DWORD code = GetLastError();
            if (code == ERROR_INVALID_PARAMETER || code == ERROR_NOT_FOUND) return true;
            error = std::string("failed open process ") + std::to_string(code);
            return false;
        }
        DWORD wait = WaitForSingleObject(hp, 5000);
        CloseHandle(hp);
        if (wait == WAIT_OBJECT_0) return true;
        error = "failed terminate process 5";
        return false;
    }
    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!h) {
        DWORD code = GetLastError();
        DebugLog(std::string("kill_process_tree: OpenProcess failed pid=") + std::to_string(pid) + " code=" + std::to_string(code));
        if (code == ERROR_INVALID_PARAMETER || code == ERROR_NOT_FOUND) return true;
        enable_debug_privilege_noexcept(error);
        h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!h) {
            DWORD code2 = GetLastError();
            error = std::string("failed open process ") + std::to_string(code2);
            DebugLog(std::string("kill_process_tree: open failed after enable_debug pid=") + std::to_string(pid) + " code=" + std::to_string(code2));
            return false;
        }
    }

    BOOL terminated = FALSE;
    DWORD lastErr = 0;
    for (int attempt = 0; attempt < 10; ++attempt) {
        terminated = TerminateProcess(h, 1);
        if (terminated) {
            DWORD waitResult = WaitForSingleObject(h, 2000);
            if (waitResult != WAIT_TIMEOUT) break;
        } else {
            lastErr = GetLastError();
            DebugLog(std::string("kill_process_tree: TerminateProcess failed pid=") + std::to_string(pid) + " attempt=" + std::to_string(attempt) + " err=" + std::to_string(lastErr));
        }
        Sleep(200);
    }
    CloseHandle(h);
    if (!terminated) {
        error = std::string("failed terminate process ") + std::to_string(lastErr);
        return false;
    }
    return true;
}

static bool kill_pid(int pid, std::string& error) noexcept {
    if (pid <= 0) return true;
    enable_debug_privilege_noexcept(error);
    return kill_process_tree(static_cast<DWORD>(pid), error);
}

static bool wait_for_stable_dir(const std::filesystem::path& dir, int maxSeconds, std::string& error) noexcept {
    if (!std::filesystem::exists(dir)) return true;
    try {
        int stableCount = -1;
        int stableIterations = 0;
        int iterations = (maxSeconds * 1000) / 250;
        for (int i = 0; i < iterations; ++i) {
            std::error_code ec;
            size_t count = 0;
            for (auto& e : std::filesystem::recursive_directory_iterator(dir, ec)) {
                if (ec) break;
                ++count;
            }
            if (ec) {
                error = "dir iterate error";
                return false;
            }
            if ((int)count == stableCount) {
                ++stableIterations;
                if (stableIterations >= 4) return true; 
            } else {
                stableCount = (int)count;
                stableIterations = 0;
            }
            Sleep(250);
        }
        return false;
    } catch (...) {
        error = "exception during wait";
        return false;
    }
}

bool install(const std::string& sourceFile, const std::string& filename, const std::string& targetDir, int pid, std::string& error) noexcept {
    try {
    DebugLog(std::string("install: sourceFile=") + sourceFile + " filename=" + filename + " targetDir=" + targetDir + " pid=" + std::to_string(pid));
    if (sourceFile.empty() || filename.empty() || targetDir.empty()) {
        error = "invalid path";
        return false;
    }
    auto type = core::io::detect(filename);
    if (type == core::io::FileType::Unknown) {
        error = "unknown file type";
        return false;
    }
    DebugLog(std::string("install: attempting to kill pid ") + std::to_string(pid));
    if (!kill_pid(pid, error)) {
        DebugLog(std::string("install: kill_pid failed: ") + error);
        return false;
    }
    DebugLog("install: kill_pid ok");
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
        DebugLog(std::string("install: starting unzip ") + sourceFile + " -> " + tempExtract.string());
        if (!unzip(sourceFile, tempExtract.string(), error)) {
            DebugLog(std::string("install: unzip failed: ") + error);
            return false;
        }

        DebugLog("unzip returned waiting for extraction");
        std::string waitErr;
        if (!wait_for_stable_dir(tempExtract, 10, waitErr)) {
            DebugLog("extraction did not stabilize: " + waitErr);
        } else {
            DebugLog("extraction good");
        }

        {
            std::vector<std::filesystem::path> toRemove;
            std::error_code ec2;
            for (auto& e : std::filesystem::recursive_directory_iterator(tempExtract, ec2)) {
                if (ec2) break;
                if (!e.is_regular_file(ec2)) continue;
                std::string stem = e.path().stem().string();
                std::string ext = e.path().extension().string();
                std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (stem == "owopdater" && ext == ".exe") {
                    toRemove.push_back(e.path());
                }
            }
            for (auto& p : toRemove) {
                std::error_code remec;
                for (int attempt = 0; attempt < 5; ++attempt) {
                    std::filesystem::remove(p, remec);
                    if (!remec) break;
                    Sleep(200);
                }
                if (remec) DebugLog("failed removing extracted owopdater: " + p.string());
                else DebugLog("removed extracted owopdater: " + p.string());
            }
        }

        DebugLog(std::string("install: calling swapper with source=") + tempExtract.string() + " target=" + targetDir);
        if (!swapper(tempExtract.string(), targetDir, error)) {
            DebugLog(std::string("install: swapper failed: ") + error);
            return false;
        }
        return true;
    }
    DebugLog(std::string("install: single-file source; calling swapper source=") + sourceFile + " target=" + targetDir);
    if (!swapper(sourceFile, targetDir, error)) {
        DebugLog(std::string("install: swapper failed: ") + error);
        return false;
    }
    DebugLog("install: completed successfully");
    return true;
    } catch (const std::exception& ex) {
        error = std::string("install exception: ") + ex.what();
        DebugLog(std::string("install exception: ") + ex.what());
        return false;
    } catch (...) {
        error = "install exception";
        DebugLog("install exception");
        return false;
    }
}

}}}
