#ifndef CODEINJECTOR_H
#define CODEINJECTOR_H

#include <string>
#include <Windows.h>

HANDLE startProcess( const std::string& cmdLine );

void* injectData( HANDLE hProcess, void* dataToWrite, size_t dataSize );

bool runRemoteThread( HANDLE hProcess, void* addr, void* param );
#endif // CODEINJECTOR_H
