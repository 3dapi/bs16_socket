//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     8

void LogGetLastError(int hr);


HANDLE          g_hFile = nullptr;                      // File Handle
DWORD           g_TotalSize= 0;                     // total file size
DWORD           g_TotalRead= 0;                     // total read

// for read buffer
OVERLAPPED      g_rOL   ={0};
char            g_rBuf[MAX_BUF+4]={0};              // Io Completion Buffer for receive

DWORD   WINAPI  WorkThread(void*);                  // Work 쓰레드
void  CALLBACK  CompletionRoutine(DWORD, DWORD
                            , LPOVERLAPPED);        // completion routine

int     AsyncRead();



int main()
{
    int hr = 0;

    g_hFile = CreateFile("kingsj.txt"
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



    // Work 쓰레드 생성
    HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
                        , (unsigned (__stdcall*)(void*))WorkThread
                        , nullptr, 0, nullptr);

    while(g_hFile)
    {
        Sleep(100);
    }

    if(g_hFile)
        CloseHandle(g_hFile);

    CloseHandle(g_rOL.hEvent);

    return 0;
}


DWORD WINAPI WorkThread(void* pParam)
{
    int     hr   = 0;
    DWORD   dTran= 0;

    while(g_hFile)
    {
        int hr = 0;

        // 비동기 read 요청
        hr = AsyncRead();
        if(FAILED(hr))
            return -1;

        hr = SleepEx(INFINITE, TRUE);
        if(WAIT_IO_COMPLETION == hr)
            continue;

    }

    CloseHandle(g_hFile);
    g_hFile = 0;

    _endthreadex(0);
    return 0;   
}


int AsyncRead()
{
    int     hr = 0;
    DWORD   dTran = 0;

    // buffer clear
    memset(g_rBuf, 0, MAX_BUF);

    // 비동기 read 요청
    hr= ReadFileEx(g_hFile, g_rBuf, MAX_BUF, &g_rOL, CompletionRoutine);
    if(ERROR_SUCCESS == hr)
    {
        Sleep(10);
        hr = GetLastError();
        if(ERROR_IO_PENDING != hr)
        {
            LogGetLastError(hr);
            return -1;
        }
    }

    return 0;
}


// Completion Routine
void CALLBACK CompletionRoutine(DWORD dErr, DWORD dTran, LPOVERLAPPED pOl)
{
    int hr=0;

    printf(g_rBuf);
    g_TotalRead += dTran;

    if(g_TotalSize <= g_TotalRead)
    {
        if(g_hFile)
            CloseHandle(g_hFile);

        g_hFile = 0;
        return ;
    }


    // Read file 재 요청
    g_rOL.Offset += dTran;

    hr = AsyncRead();

    Sleep(30);

    return;
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

