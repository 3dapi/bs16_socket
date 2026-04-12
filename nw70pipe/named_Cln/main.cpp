//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4996)

#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define MAX_BUF 8192

char    g_sPipeName[] = "\\\\.\\pipe\\mynamedpipe";
char    g_sSnd[MAX_BUF] = "Default message from client.";
char    g_sRcv[MAX_BUF]={0};

HANDLE g_hPipe = nullptr;

int main()
{

    
    int hr = FALSE;
    DWORD  cbRead, iSize, dTran, dwMode;

    while(1)
    {
        g_hPipe = CreateFile(g_sPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

        if(g_hPipe != INVALID_HANDLE_VALUE)
            break;

        hr = GetLastError();
        if(ERROR_PIPE_BUSY != hr)
        {
            printf( "Could not open pipe. GLE=%d\n", GetLastError() );
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds.
        hr = WaitNamedPipe(g_sPipeName, 60000);
        if(0 == hr)
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }

    // The pipe connected; change to message-read mode.

    dwMode = PIPE_READMODE_MESSAGE;
    hr = SetNamedPipeHandleState(g_hPipe, &dwMode, nullptr, nullptr);
    if( ! hr)
    {
        printf( "SetNamedPipeHandleState failed. GLE=%d\n", GetLastError() );
        return -1;
    }

    // Send a message to the pipe server.
    iSize = lstrlen(g_sSnd);
    printf( "Sending: %d byte\n", iSize);

    hr = WriteFile(g_hPipe, g_sSnd, iSize, &dTran,  nullptr);
    if(0 == hr)
    {
        hr = GetLastError();
        printf( "WriteFile to pipe failed. GLE=%d\n", hr );
        return -1;
    }

    printf("\nMessage sent to server, receiving reply as follows:\n");

    do {
        hr = ReadFile(g_hPipe, g_sRcv, MAX_BUF, &cbRead, nullptr);

        if( ! hr && GetLastError() != ERROR_MORE_DATA )
            break;

        printf( "\"%s\"\n", g_sRcv );
    } while( ! hr);  // repeat loop if ERROR_MORE_DATA

    if( ! hr)
    {
        printf( "ReadFile from pipe failed. GLE=%d\n", GetLastError() );
        return -1;
    }

    CloseHandle(g_hPipe);

    return 0;
}

