#include <iostream>
#include "owopdater/cli/cli_parser.h"
#include "owopdater/client/updater.h"

namespace owopdater {

int run_cli(int argc, char** argv) {
    auto args = parse_cli_args(argc, argv);
    if (!args.valid()) {
        std::cout << cli_usage(argc > 0 ? argv[0] : "owopdater");
        return 1;
    }

    return client::run_update(*args.link, *args.absolute_dir, *args.pid, *args.sha256, args.launch.has_value() ? *args.launch : std::string());
}

} // namespace femboys
