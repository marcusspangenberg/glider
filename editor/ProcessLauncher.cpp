#include "ProcessLauncher.h"
#include <cstdlib>

std::string tempHousePath()
{
#ifdef _WIN32
    const char* tmp = getenv("TEMP");
    return std::string(tmp ? tmp : "C:\\Temp") + "\\glider_test_house";
#else
    return "/tmp/glider_test_house";
#endif
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

bool launchProcess(const std::vector<std::string>& args)
{
    std::string cmdLine;
    for (const auto& arg : args)
    {
        if (!cmdLine.empty())
        {
            cmdLine += ' ';
        }
        if (arg.find(' ') != std::string::npos)
        {
            cmdLine += '"' + arg + '"';
        }
        else
        {
            cmdLine += arg;
        }
    }
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

#else
#include <sys/types.h>
#include <unistd.h>

bool launchProcess(const std::vector<std::string>& args)
{
    std::vector<char*> argv;
    for (auto& arg : const_cast<std::vector<std::string>&>(args))
    {
        argv.emplace_back(arg.data());
    }
    argv.emplace_back(nullptr);

    const pid_t pid = fork();
    if (pid == 0)
    {
        execv(args[0].c_str(), argv.data());
        _exit(1);
    }
    return pid > 0;
}
#endif
