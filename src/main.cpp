#include <iostream>

#include "CodeToInject.h"
#include "CodeInjector.h"

int main(int argc, const char* argv[])
{
    if(3 != argc)
    {
        std::cout << "Usage: " << argv[0] << " exe_path time_scale" << std::endl;
        return 0;
    }
    HANDLE h = startProcess(argv[1]);
    if( !h )
    {
        return 1;
    }

    char* data = new char[sizeToInject()];
    writeData(data, atof(argv[2]));
    char* c = (char*)injectData(h, data, sizeToInject());
    delete[] data;
    if( !c )
    {
        return 1;
    }
    if( !runRemoteThread(h, c + thrdFuncOffset(), c) )
    {
        return 1;
    }
    return 0;
}

