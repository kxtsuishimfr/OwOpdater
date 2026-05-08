#pragma once
#include <string>

namespace owopdater { namespace client {

int run_update(const std::string& link, const std::string& absolute_dir, int pid, const std::string& sha256 = "", const std::string& launch = "");

}} // namespace owopdater::client
