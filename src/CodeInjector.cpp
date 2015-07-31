#include "CodeInjector.h"

#include <iostream>

HANDLE startProcess( const std::string& cmdLine )
{
    std::string dir;
    size_t pos = cmdLine.rfind('\\');
    if(std::string::npos == pos)
    {
        pos = cmdLine.rfind('/');
    }

    const char* pDir = nullptr;
    if(std::string::npos != pos)
    {
        dir = cmdLine.substr(0, pos);
        pDir = dir.c_str();
    }

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset( &si, 0, sizeof(si));
    si.cb = sizeof(si);
    if( !CreateProcessA(cmdLine.c_str(),
                        nullptr,
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        pDir,
                        &si,
                        &pi) )
    {
        std::cout << "CreateProcessA failed with " << GetLastError() << std::endl;
        return nullptr;
    }
    return pi.hProcess;
}

void* injectData( HANDLE hProcess, void* dataToWrite, size_t dataSize )
{
    void* data = VirtualAllocEx(hProcess, nullptr, dataSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(!data)
    {
        std::cout << "VirtualAllocEx failed with " << GetLastError() << std::endl;
        return nullptr;
    }
    if(!WriteProcessMemory(hProcess, data, dataToWrite, dataSize, nullptr))
    {
        std::cout << "WriteProcessMemory failed with " << GetLastError() << std::endl;
        return nullptr;
    }
    return data;
}

bool runRemoteThread( HANDLE hProcess, void* addr, void* param )
{
    FlushInstructionCache(hProcess, nullptr, 0);
    if( !CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)addr, param, 0, nullptr) )
    {
        std::cout << "CreateRemoteThread failed with " << GetLastError() << std::endl;
        return false;
    }
    return true;
}
