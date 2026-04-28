#pragma once
#include <string>
#include <vector>

std::string tempHousePath();

// Launches a process with the given argument list (argv[0] is the executable).
// Returns true on success.
bool launchProcess(const std::vector<std::string>& args);
