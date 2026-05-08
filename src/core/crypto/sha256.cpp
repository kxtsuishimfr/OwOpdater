#include "owopdater/core/crypto/sha256.h"
#include <Windows.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")

namespace owopdater { namespace core { namespace crypto {

static std::string to_hex(const unsigned char* data, size_t len) noexcept {
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return ss.str();
}

static std::string to_lower(const std::string& s) noexcept {
    std::string result = s;
    for (char& c : result) {
        c = (char)std::tolower((unsigned char)c);
    }
    return result;
}

std::string sha256_file(const std::string& filepath, std::string& error) noexcept {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        error = "crypto context failed";
        return "";
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        error = "hash creation failed";
        return "";
    }
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        error = "file open failed";
        return "";
    }
    const size_t buffer_size = 65536;
    unsigned char buffer[buffer_size];
    while (file.read((char*)buffer, buffer_size) || file.gcount() > 0) {
        if (!CryptHashData(hHash, buffer, (DWORD)file.gcount(), 0)) {
            file.close();
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            error = "hash data failed";
            return "";
        }
    }
    file.close();
    DWORD hash_len = 32;
    unsigned char hash_value[32];
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash_value, &hash_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        error = "hash param failed";
        return "";
    }
    std::string result = to_hex(hash_value, hash_len);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return result;
}

bool verify_sha256(const std::string& filepath, const std::string& expected_hash, std::string& error) noexcept {
    if (expected_hash.empty()) return true;

    
    std::string exp;
    exp.reserve(expected_hash.size());
    for (char c : expected_hash) {
        if (!std::isspace((unsigned char)c)) exp.push_back(c);
    }
    if (exp.size() != 64) {
        error = "invalid sha256 format";
        return false;
    }
    for (char c : exp) {
        if (!std::isxdigit((unsigned char)c)) {
            error = "invalid sha256 format";
            return false;
        }
    }

    std::string computed = sha256_file(filepath, error);
    if (computed.empty()) return false;
    if (to_lower(computed) != to_lower(exp)) {
        error = "sha256 mismatch";
        return false;
    }
    return true;
}

}}} // no namespace wow
