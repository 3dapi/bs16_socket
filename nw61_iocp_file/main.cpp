//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     4

void LogGetLastError(int hr);
int  GetSystemProcessorCount();


HANDLE          g_hFile {};                 // File Handle
DWORD           g_TotalSize{};              // total file size

// for read buffer
OVERLAPPED      g_rOL   ={0};
char            g_rBuf[MAX_BUF+4]={0};      // Io Completion Buffer for receive

HANDLE          g_hIocp  {};                // IOCP Handle
unsigned WINAPI WorkThread(void*);          // Work 쓰레드


int     AsyncRead();


int main()
{
    int hr {};

    g_hFile = CreateFile("flag.txt"
                        , GENERIC_READ | GENERIC_WRITE
                        , 0
                        , nullptr
                        , OPEN_ALWAYS
                        , FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED
                        , nullptr);

    if(g_hFile == INVALID_HANDLE_VALUE)
        return -1;

    if(0 == (g_TotalSize = GetFileSize(g_hFile, nullptr)) )
        return -1;


    // IOCP 객체 생성
    g_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if(nullptr == g_hIocp)
        return -1;


    // 2 * processor 개수만큼 Work 쓰레드 생성
    int nWrkPrc = 2 * GetSystemProcessorCount();
    for(int i=0; i<nWrkPrc; ++i)
    {
        HANDLE hThWrk = (HANDLE)_beginthreadex(nullptr, 0
                        , WorkThread
                        , nullptr, 0, nullptr);

        CloseHandle(hThWrk);
    }


    ULONG_PTR   pIoKey = (ULONG_PTR)&g_hFile;                                   // IO Key is File
    HANDLE      hRet   = CreateIoCompletionPort(g_hFile, g_hIocp, pIoKey, 0);   // <IOCP, File, key> 바인딩


    // 비동기 read 요청
    hr = AsyncRead();
    if(FAILED(hr))
        return -1;


    while(g_hFile)
    {
        Sleep(10);
    }


    if(g_hFile)
        CloseHandle(g_hFile);

    CloseHandle(g_hIocp);

    return 0;
}


unsigned WINAPI WorkThread(void* pParam)
{
    int         hr {};
    ULONG_PTR   pIoKey  {};
    OVERLAPPED* pOL     {};
    DWORD       dTran   {};
    DWORD       OLType  {};


    while(g_hFile)
    {
        pIoKey = {};
        pOL    = {};
        dTran  = {};

        hr = GetQueuedCompletionStatus(
                g_hIocp                         // Completion Port
            ,   &dTran                          // 전송 된 바이트 수
            ,   (PULONG_PTR)&pIoKey             // 완료키 주소
            ,   (LPOVERLAPPED*)&pOL             // OVERLAPPED 구조체
            ,   INFINITE
            );


        if(0 == hr)                             // 실패
        {
            hr = GetLastError();

            if(!pOL && !pIoKey)
                continue;

            break;
        }

        if(0 == dTran)                          // Read complete
        {
            printf("Read complete.\n");
            break;
        }


        printf("%s", g_rBuf);                   // 버퍼 출력

        g_rOL.Offset += dTran;                  // offset 조정
        if(g_TotalSize <= g_rOL.Offset)         // 전체 파일 크기 비교
            break;

        Sleep(10);

        memset(g_rBuf, 0, MAX_BUF);             // 버퍼 초기화
        hr = AsyncRead();                       // Read file 요청
        if(FAILED(hr))
            break;
    }

    CloseHandle(g_hFile);
    g_hFile = {};

    _endthreadex(0);
    return 0;
}


int AsyncRead()
{
    int hr {};
    DWORD   dTran {};
    memset(g_rBuf, 0, MAX_BUF+4);

    // 비동기 read 요청
    hr = ReadFile(g_hFile, g_rBuf, MAX_BUF, &dTran, &g_rOL);
    if(ERROR_SUCCESS == hr)
    {
        hr = GetLastError();
        if(ERROR_IO_PENDING != hr)
        {
            LogGetLastError(hr);
            return -1;
        }

        Sleep(10);
    }

    return 0;
}


int GetSystemProcessorCount()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    return (int)SystemInfo.dwNumberOfProcessors;
}


void LogGetLastError(int hr)
{
    char* lpMsgBuf;
    FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS
                , nullptr, hr, 0, (LPSTR)&lpMsgBuf, 0, nullptr );

    printf( "%s\n", lpMsgBuf);
    LocalFree( lpMsgBuf );
}
