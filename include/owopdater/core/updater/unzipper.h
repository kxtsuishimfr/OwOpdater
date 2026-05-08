#pragma once
#include <string>

namespace owopdater { namespace core { namespace updater {

bool unzip(const std::string& zipPath, const std::string& outDir, std::string& error) noexcept;

}}}
