#include <stdio.h>
#include <windows.h>

#define PIPENAME    "\\\\.\\Pipe\\DayTime"
#define MAX_BUF     1024


HANDLE g_hPipe = nullptr;

char    g_sRcv[MAX_BUF+4]{};

int main()
{
    int     hr = FALSE;

    while(1)
    {
        g_hPipe = CreateFile(PIPENAME, GENERIC_READ | GENERIC_WRITE, 
                        0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (INVALID_HANDLE_VALUE != g_hPipe) 
            break;
    }

    while(1)
    {
        Sleep(1000);

        DWORD   dTran = 0;
        memset(g_sRcv, 0, MAX_BUF+4);

        hr = ReadFile(g_hPipe, g_sRcv, MAX_BUF, &dTran, nullptr);

        if (0 == hr) 
        {
            hr = GetLastError();
            if(ERROR_HANDLE_EOF == hr)
            {
                printf("fail to ReadFile(), %d\n", hr); 
                break;
            }
        }
    
        printf("%s\n", g_sRcv);
    }

    CloseHandle(g_hPipe);

    return 0;
}
