#include "owopdater/cli/cli_parser.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace owopdater {

CliArgs parse_cli_args(int argc, char** argv) {
    CliArgs out;
    if (argc <= 1) return out;

    bool anyFlag = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && argv[i][0] == '-') { anyFlag = true; break; }
    }

    if (!anyFlag) {
        if (argc > 1) out.link = std::string(argv[1]);
        if (argc > 2) out.absolute_dir = std::string(argv[2]);
        if (argc > 3) out.sha256 = std::string(argv[3]);
        if (argc > 4) {
            char* endptr = nullptr;
            long v = std::strtol(argv[4], &endptr, 10);
            if (endptr != argv[4]) out.pid = static_cast<int>(v);
        }
        if (argc > 5) out.launch = std::string(argv[5]);
        return out;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-link" || arg == "--link") {
            if (i + 1 < argc) out.link = argv[++i];
        } else if (arg == "-absolute" || arg == "-absolute-dir" || arg == "--absolute-dir" || arg == "-absolute_dir") {
            if (i + 1 < argc) out.absolute_dir = argv[++i];
        } else if (arg == "-sha-256" || arg == "--sha-256" || arg == "-sha256" || arg == "--sha256") {
            if (i + 1 < argc) out.sha256 = argv[++i];
        } else if (arg == "-launch" || arg == "--launch") {
            if (i + 1 < argc) out.launch = argv[++i];
        } else if (arg == "-pid" || arg == "--pid") {
            if (i + 1 < argc) {
                char* valStr = argv[++i];
                char* endptr = nullptr;
                long v = std::strtol(valStr, &endptr, 10);
                if (endptr != valStr) out.pid = static_cast<int>(v);
            }
        } else {
            if (!arg.empty() && arg[0] != '-') {
                if (!out.link.has_value()) out.link = arg;
                else if (!out.absolute_dir.has_value()) out.absolute_dir = arg;
                else if (!out.sha256.has_value()) out.sha256 = arg;
                else if (!out.pid.has_value()) {
                    char* endptr = nullptr;
                    long v = std::strtol(arg.c_str(), &endptr, 10);
                    if (endptr != arg.c_str()) out.pid = static_cast<int>(v);
                }
            }
        }
    }
    return out;
}

std::string cli_usage(const char* progName) {
    std::string s;
    s += std::string("Usage: ") + progName + " [options] <link> <absolute_dir> <sha256> <pid>\n";
    s += "Options:\n";
    s += "  -link <url>\n";
    s += "  -absolute-dir <path>\n";
    s += "  -sha-256 <hash>\n";
    s += "  -launch <procname>\n";
    s += "  -pid <pid>\n";
    s += "Examples:\n";
    s += "  " + std::string(progName) + " https://link.com C:\\\\path abcdef 1234\n";
    s += "  " + std::string(progName) + " -link https://link.com -absolute-dir C:\\\\path -sha-256 abcdef -pid 1234\n";
    return s;
}

} // namespace hi!
