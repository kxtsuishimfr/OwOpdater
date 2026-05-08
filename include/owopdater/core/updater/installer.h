#pragma once
#include <string>

namespace owopdater { namespace core { namespace updater {

bool install(const std::string& sourceFile, const std::string& filename, const std::string& targetDir, int pid, std::string& error) noexcept;

}}}
