#include <Windows.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "owopdater/utils/generic.h"


static bool g_debug_logging_enabled = true;

void SetDebugLogging(bool enabled) {
    g_debug_logging_enabled = enabled;
}

bool IsDebugLogging() {
    return g_debug_logging_enabled;
}

void DebugLog(const std::string& message) {
    if (!g_debug_logging_enabled) return;
    std::string out = "[OwOpdater] " + message + "\n";
    OutputDebugStringA(out.c_str());
}

void DebugLogPretty(const std::string& title, const std::string& message) {
    if (!g_debug_logging_enabled) return;
    std::ostringstream ss;
    ss << "[OwOpdater] " << title << "\n";
    ss << "----------------------------------------\n";
    ss << message << "\n";
    ss << "----------------------------------------\n";
    auto s = ss.str();
    OutputDebugStringA(s.c_str());
}