#pragma once
#include <string>

namespace owopdater { namespace core { namespace crypto {

std::string sha256_file(const std::string& filepath, std::string& error) noexcept;
bool verify_sha256(const std::string& filepath, const std::string& expected_hash, std::string& error) noexcept;

}}}
