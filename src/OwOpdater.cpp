#include "owopdater/cli/cli_parser.h"
#include "owopdater/utils/generic.h"
#include <string>

int main(int argc, char** argv) {
	DebugLog("OwOpdater starting");
	int result = owopdater::run_cli(argc, argv);
	DebugLog(std::string("OwOpdater returning ") + std::to_string(result));
	return result;
}