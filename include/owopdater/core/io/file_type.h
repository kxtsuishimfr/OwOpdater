#pragma once
#include <string>

namespace owopdater { namespace core { namespace io {

enum class FileType { Unknown = 0, Zip, Executable };

FileType detect(const std::string& filename) noexcept;
bool is_zip(const std::string& filename) noexcept;
bool is_exe(const std::string& filename) noexcept;

}}}
