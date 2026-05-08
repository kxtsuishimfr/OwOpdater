#pragma once

#include <Windows.h>
#include <string>

void SetDebugLogging(bool enabled);
bool IsDebugLogging();
void DebugLog(const std::string& message);
void DebugLogPretty(const std::string& title, const std::string& message);
