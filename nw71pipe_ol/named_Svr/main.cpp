//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4996)

#include <windows.h>

#include <stdio.h>
#include <time.h>

#define MAX_BUF     8196
void LogGetLastError(int hr);

char g_sPipeName[] = "\\\\.\\pipe\\mynamedpipe";

char g_sSnd[MAX_BUF+4]{};


int main()
{
    int hr = 0;
    int iLen = 0;
    HANDLE hPipe = nullptr;

    DWORD dwWrite = 0;
    time_t tNow = 0;
    struct tm *t = nullptr;

    hPipe = CreateNamedPipe(g_sPipeName
                ,   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED      // read/write access
                ,   PIPE_TYPE_MESSAGE |         // message type pipe
                    PIPE_READMODE_MESSAGE |     // message-read mode
                    PIPE_WAIT                   // blocking mode
                ,   PIPE_UNLIMITED_INSTANCES    // max. instances
                ,   MAX_BUF                     // output buffer size
                ,   MAX_BUF                     // input buffer size
                ,   NMPWAIT_USE_DEFAULT_WAIT    // client time-out
                ,   nullptr);                       // default security attribute

    if(hPipe == INVALID_HANDLE_VALUE )
    {
        hr = GetLastError();
        LogGetLastError(hr);
        return 0;
    }



    while (1)
    {
        if(::ConnectNamedPipe(hPipe, nullptr) )
        {
            while(1)
            {
                tNow = time(0);
                t = localtime(&tNow);
                iLen = sprintf(g_sSnd, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

                hr = WriteFile(hPipe, g_sSnd, iLen, &dwWrite, nullptr);

                if((!hr) || (iLen != (int)dwWrite))
                {
                    printf("fail to WriteFile, %d\n", GetLastError());
                    exit(0);
                }

                FlushFileBuffers(hPipe);
            }
            DisconnectNamedPipe(hPipe);
        }
    }

    CloseHandle(hPipe);
    return 0;
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

