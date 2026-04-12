#include <stdio.h>
#include <time.h>
#include <windows.h>

#define PIPENAME    "\\\\.\\Pipe\\DayTime"
#define MAX_BUF     1024

int main()
{
    int nLen = 0;
    HANDLE hPipe = nullptr;
    BOOL bRet = FALSE;
    char szTime[0xFF] = {0};
    DWORD dwWrite = 0;
    time_t tNow = 0;
    struct tm *t = nullptr;
    
    hPipe = CreateNamedPipe( 
        PIPENAME,                 // pipe name 
        PIPE_ACCESS_DUPLEX,       // read/write access 
        PIPE_TYPE_MESSAGE |       // message type pipe 
        PIPE_READMODE_MESSAGE |   // message-read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // max. instances  
        MAX_BUF,            // output buffer size 
        MAX_BUF,             // input buffer size 
        NMPWAIT_USE_DEFAULT_WAIT, // client time-out 
        nullptr);                    // default security attribute 
    
    if ( hPipe == INVALID_HANDLE_VALUE )
    {
        printf("fail to CreateNamedPipe(), %d\n", GetLastError());
        exit(0);
    }
    
    while (1)
    {
        if ( ConnectNamedPipe(hPipe, nullptr) )
        {
            while(1)
            {
                tNow = time(0);
                t = localtime(&tNow);
                nLen = _snprintf(szTime, 0xFF, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

                bRet = WriteFile(hPipe, szTime, nLen, &dwWrite, nullptr);
                
                if ( (!bRet) || (nLen != (int)dwWrite))
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
