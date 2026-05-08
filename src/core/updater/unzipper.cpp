#include "owopdater/core/updater/unzipper.h"
#include <Windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <shlguid.h>
#include <OleAuto.h>
#include <filesystem>
#include <system_error>
#include <fstream>

namespace owopdater { namespace core { namespace updater {

static std::wstring to_wstring(const std::string& s) noexcept {
    if (s.empty()) return std::wstring();
    int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len == 0) return std::wstring();
    std::wstring result;
    result.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], len);
    return result;
}

static bool is_rar(const std::string& filepath) noexcept {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    unsigned char sig[7];
    file.read((char*)sig, 7);
    if (file.gcount() < 4) return false;
    return sig[0] == 0x52 && sig[1] == 0x61 && sig[2] == 0x72 && sig[3] == 0x21;
}

static bool open_shell_folder(IShellDispatch* shell, const std::wstring& path, Folder** outFolder) noexcept {
    VARIANT arg;
    VariantInit(&arg);
    arg.vt = VT_BSTR;
    arg.bstrVal = SysAllocString(path.c_str());
    HRESULT hr = shell->NameSpace(arg, outFolder);
    VariantClear(&arg);
    return SUCCEEDED(hr) && outFolder && *outFolder;
}

bool unzip(const std::string& zipPath, const std::string& outDir, std::string& error) noexcept {
    std::error_code ec;
    std::filesystem::path zipFile(zipPath);
    std::filesystem::path targetDir(outDir);
    if (zipPath.empty() || outDir.empty()) {
        error = "invalid path";
        return false;
    }
    if (!std::filesystem::exists(zipFile, ec)) {
        error = "zip missing";
        return false;
    }
    if (is_rar(zipPath)) {
        error = "rar extraction not supported";
        return false;
    }
    std::filesystem::create_directories(targetDir, ec);
    if (ec) {
        error = "target create failed";
        return false;
    }
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool doUninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        error = "com init failed";
        return false;
    }
    IShellDispatch* shell = nullptr;
    hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell));
    if (FAILED(hr) || !shell) {
        if (doUninit) CoUninitialize();
        error = "shell instance failed";
        return false;
    }
    Folder* zipFolder = nullptr;
    Folder* destFolder = nullptr;
    bool ok = false;
    if (open_shell_folder(shell, zipFile.wstring(), &zipFolder) && open_shell_folder(shell, targetDir.wstring(), &destFolder)) {
        FolderItems* items = nullptr;
        if (SUCCEEDED(zipFolder->Items(&items)) && items) {
            VARIANT arg;
            VariantInit(&arg);
            arg.vt = VT_DISPATCH;
            arg.pdispVal = items;
            VARIANT flags;
            VariantInit(&flags);
            flags.vt = VT_I4;
            flags.lVal = 0x104;
            hr = destFolder->CopyHere(arg, flags);
            VariantClear(&flags);
            VariantClear(&arg);
            if (SUCCEEDED(hr)) ok = true;
            items->Release();
        }
    }
    if (zipFolder) zipFolder->Release();
    if (destFolder) destFolder->Release();
    shell->Release();
    if (doUninit) CoUninitialize();
    if (!ok) error = "unzip failed";
    return ok;
}

}}}
