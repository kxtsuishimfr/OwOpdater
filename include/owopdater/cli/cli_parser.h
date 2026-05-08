#pragma once
#include <string>
#include <optional>

namespace owopdater {

struct CliArgs {
    std::optional<std::string> link;
    std::optional<std::string> absolute_dir;
    std::optional<std::string> sha256;
    std::optional<int> pid;
    std::optional<std::string> launch;

    bool valid() const { return link.has_value() && absolute_dir.has_value() && sha256.has_value() && pid.has_value(); }
};

CliArgs parse_cli_args(int argc, char** argv);
std::string cli_usage(const char* progName = "owopdater");
int run_cli(int argc, char** argv);

} // namespace owopdater
