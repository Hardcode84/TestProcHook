#include "CodeToInject.h"

#include <Windows.h>
#include <Winternl.h>

#if !defined(WIN32) || !defined(_M_IX86)
#error supported only x86
#endif

#define INIT_SHARED_DATA_PTR(p) asm volatile("mov %%eax, %0" : "=r" (p));

#define JUMP_PATCH(pShared, pDest, pSrc) \
{ \
    DWORD OldProt; \
    const int patchedSize = 10; \
    pShared->vp_f(pSrc, patchedSize, PAGE_EXECUTE_READWRITE, &OldProt); \
    *(char*)pSrc = (char)0xB8; \
    *(DWORD*)((DWORD)pSrc + 1) = (DWORD)pShared; \
    *(char*)((DWORD)pSrc + 5) = (char)0xE9; \
    *(DWORD*)((DWORD)pSrc + 6) = (DWORD)pDest - (DWORD)pSrc - patchedSize; \
    pShared->vp_f(pSrc, patchedSize, OldProt, &OldProt); \
}

#define mydiv(num,den) \
{ \
    uint64_t quot = 0, qbit = 1; \
 \
    while((int64_t)den >= 0) \
    { \
        den <<= 1; \
        qbit <<= 1; \
    } \
 \
    while( qbit ) \
    { \
        if( den <= uint64_t(num) ) \
        { \
            num -= den; \
            quot += qbit; \
        } \
        den >>= 1; \
        qbit >>= 1; \
    } \
    num = quot; \
}


typedef HMODULE (WINAPI *LoadLibrary_t)(LPCTSTR lpFileName);
typedef FARPROC (WINAPI *GetProcAddress_t)( HMODULE hModule, LPCSTR lpProcName);

typedef NTSTATUS (NTAPI *NtQueryPerformanceCounter_t)(PLARGE_INTEGER PerformanceCounter, PLARGE_INTEGER PerformanceFrequency);
typedef ULONG (NTAPI*NtGetTickCount_t)();
typedef NTSTATUS (NTAPI *NtDelayExecution_t)(BOOLEAN Alertable, IN PLARGE_INTEGER DelayInterval);
typedef BOOL (WINAPI *VirtualProtect_t)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
typedef BOOL (WINAPI *FlushInstructionCache_t)(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T dwSize);
typedef HANDLE (WINAPI *GetCurrentProcess_t)(void);


#pragma pack(push,1)

const unsigned DefaultScale = 100;
struct SharedDara
{
    LoadLibrary_t ll_f;
    GetProcAddress_t gpa_f;
    VirtualProtect_t vp_f;
    FlushInstructionCache_t fic_f;
    GetCurrentProcess_t gcp_f;
    NtQueryPerformanceCounter_t qpc_f;
    NtGetTickCount_t gtc_f;
    NtDelayExecution_t slp_f;

    ULONG gtc_start;
    LARGE_INTEGER qpc_start;
    unsigned scale;
};

#pragma pack(pop)

const size_t PageSize = 4 * 1024;
const size_t FuncSize = 3*1024;

DWORD WINAPI thrdFunc(void* param)
{
    SharedDara* pShared = (SharedDara*)(param);

    char ntdllStr[] = {'n','t','d','l','l','.','d','l','l','\0'};
    char kernStr[] = {'k','e','r','n','e','l','3','2','.','d','l','l','\0'};
    char winmmStr[] = {'W','i','n','m','m','.','d','l','l','\0'}; //for timeGetTime

    HMODULE hNtdll = pShared->ll_f(ntdllStr);
    HMODULE hKern  = pShared->ll_f(kernStr);
    HMODULE hWinmm = pShared->ll_f(winmmStr);

    char vpStr[] = {'V','i','r','t','u','a','l','P','r','o','t','e','c','t','\0'};
    pShared->vp_f =  reinterpret_cast<decltype(pShared->vp_f)>(pShared->gpa_f(hKern, vpStr));
    char ficStr[] = {'F','l','u','s','h','I','n','s','t','r','u','c','t','i','o','n','C','a','c','h','e','\0'};
    pShared->fic_f = reinterpret_cast<decltype(pShared->fic_f)>(pShared->gpa_f(hKern, ficStr));
    char gcpStr[] = {'G','e','t','C','u','r','r','e','n','t','P','r','o','c','e','s','s','\0'};
    pShared->gcp_f = reinterpret_cast<decltype(pShared->gcp_f)>(pShared->gpa_f(hKern, gcpStr));
    char qpcStr[] = {'N','t','Q','u','e','r','y','P','e','r','f','o','r','m','a','n','c','e','C','o','u','n','t','e','r','\0'};
    pShared->qpc_f = reinterpret_cast<decltype(pShared->qpc_f)>(pShared->gpa_f(hNtdll, qpcStr));
    char gtcStr[] = {'N','t','G','e','t','T','i','c','k','C','o','u','n','t','\0'};
    pShared->gtc_f = reinterpret_cast<decltype(pShared->gtc_f)>(pShared->gpa_f(hNtdll, gtcStr));
    char clpStr[] = {'N','t','D','e','l','a','y','E','x','e','c','u','t','i','o','n','\0'};
    pShared->slp_f = reinterpret_cast<decltype(pShared->slp_f)>(pShared->gpa_f(hNtdll, clpStr));

    char srcGtcStr[] = {'G','e','t','T','i','c','k','C','o','u','n','t','\0'};
    void* srcGtc = reinterpret_cast<void*>(pShared->gpa_f(hKern, srcGtcStr));
    char srcQpcStr[] = {'Q','u','e','r','y','P','e','r','f','o','r','m','a','n','c','e','C','o','u','n','t','e','r','\0'};
    void* srcQpc = reinterpret_cast<void*>(pShared->gpa_f(hKern, srcQpcStr));
    char srcTgtStr[] = {'t','i','m','e','G','e','t','T','i','m','e','\0'};
    void* srcTgt = reinterpret_cast<void*>(pShared->gpa_f(hWinmm, srcTgtStr));
    char srcSlpStr[] = {'S','l','e','e','p','E','x','\0'};
    void* srcSlp = reinterpret_cast<void*>(pShared->gpa_f(hKern, srcSlpStr));

    void* myGtc = ((char*)pShared) + sizeof(SharedDara) + FuncSize;
    void* myQpc = ((char*)pShared) + sizeof(SharedDara) + FuncSize * 2;
    void* mySlp = ((char*)pShared) + sizeof(SharedDara) + FuncSize * 3;

    pShared->gtc_start = pShared->gtc_f();
    pShared->qpc_f(&pShared->qpc_start, nullptr);

    JUMP_PATCH(pShared, myGtc, srcGtc);
    JUMP_PATCH(pShared, myQpc, srcQpc);
    JUMP_PATCH(pShared, myGtc, srcTgt);
    JUMP_PATCH(pShared, mySlp, srcSlp);
    pShared->fic_f(pShared->gcp_f(), nullptr, 0);
    return 0;
}

DWORD WINAPI myGetTickCount()
{
    SharedDara* pShared;
    INIT_SHARED_DATA_PTR(pShared);
    ULONG newTicks = pShared->gtc_f();
    return pShared->gtc_start + ((newTicks - pShared->gtc_start) * pShared->scale) / DefaultScale;
}

BOOL WINAPI myQueryPerformanceCounter(PLARGE_INTEGER PerformanceCounter)
{
    SharedDara* pShared;
    INIT_SHARED_DATA_PTR(pShared);
    pShared->qpc_f(PerformanceCounter, nullptr);
    PerformanceCounter->QuadPart = ((PerformanceCounter->QuadPart - pShared->qpc_start.QuadPart) * LONGLONG(pShared->scale));
    ULONGLONG den = DefaultScale;
    mydiv(PerformanceCounter->QuadPart, den);
    PerformanceCounter->QuadPart += pShared->qpc_start.QuadPart;
    return TRUE;
}

DWORD WINAPI mySleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
    SharedDara* pShared;
    INIT_SHARED_DATA_PTR(pShared);

    if(0 != dwMilliseconds)
    {
        dwMilliseconds = (dwMilliseconds * DefaultScale) / pShared->scale;
        dwMilliseconds = dwMilliseconds ? dwMilliseconds : 1;
    }
    LARGE_INTEGER li;
    li.QuadPart = dwMilliseconds * 10000; // to 100 ns
    return (pShared->slp_f(bAlertable, &li) ? WAIT_IO_COMPLETION : 0);
}

void* funcPtrs[] = {reinterpret_cast<void*>(thrdFunc),
                    reinterpret_cast<void*>(myGetTickCount),
                    reinterpret_cast<void*>(myQueryPerformanceCounter),
                    reinterpret_cast<void*>(mySleepEx)};

size_t sizeToInject()
{
    return sizeof(SharedDara) + FuncSize * (sizeof(funcPtrs)/sizeof(funcPtrs[0]));
}

ptrdiff_t thrdFuncOffset()
{
    return sizeof(SharedDara);
}

void writeData(void* ptr, float scale)
{
    SharedDara dat;
    memset(&dat, 0, sizeof(dat));
    dat.ll_f = LoadLibraryA;
    dat.gpa_f = GetProcAddress;
    dat.scale = scale * DefaultScale;
    char* temPtr = reinterpret_cast<char*>(ptr);
    memcpy(temPtr, &dat, sizeof(dat));
    temPtr += sizeof(dat);
    for(size_t i = 0; i < (sizeof(funcPtrs)/sizeof(funcPtrs[0])); ++i)
    {
        memcpy(temPtr, funcPtrs[i], FuncSize);
        temPtr += FuncSize;
    }
}




