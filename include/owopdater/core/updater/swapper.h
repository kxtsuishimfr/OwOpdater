#pragma once
#include <string>

namespace owopdater { namespace core { namespace updater {

bool swapper(const std::string& source, const std::string& targetDir, std::string& error) noexcept;

}}}
