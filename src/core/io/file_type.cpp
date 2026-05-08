#include "owopdater/core/io/file_type.h"
#include <algorithm>
#include <cctype>

namespace owopdater { namespace core { namespace io {

static std::string to_lower(std::string s) noexcept {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

FileType detect(const std::string& filename) noexcept {
    if (filename.empty()) return FileType::Unknown;
    size_t pos = filename.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? filename : filename.substr(pos + 1);
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return FileType::Unknown;
    std::string ext = to_lower(name.substr(dot + 1));
    if (ext == "zip" || ext == "rar") return FileType::Zip;
    if (ext == "exe" || ext == "dll" || ext == "msi") return FileType::Executable;
    return FileType::Unknown;
}

bool is_zip(const std::string& filename) noexcept { return detect(filename) == FileType::Zip; }
bool is_exe(const std::string& filename) noexcept { return detect(filename) == FileType::Executable; }

}}} // :>
