//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "WS2_32.LIB")

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     8192

char    sPt[32]="60000";

void LogGetLastError(int hr);



SOCKET          g_scLstn{};             // listen socket
SOCKADDR_IN     g_sdLstn{};

SOCKET          g_scCln{};              // client socket
SOCKADDR_IN     g_sdCln{};

char            g_bufSnd[MAX_BUF]{};    // send buffer
CRITICAL_SECTION    m_CS{};


HANDLE          g_hThAcc{};             // accept thread handle
HANDLE          g_hThRcv{};             // receive thread handle
HANDLE          g_hThSnd{};             // send thread handle


DWORD WINAPI WorkAcc(void*);            // Accept용 쓰레드
DWORD WINAPI WorkRcv(void*);            // Receive용 쓰레드
DWORD WINAPI WorkSnd(void*);            // Send용 쓰레드


int main()
{
    InitializeCriticalSection(&m_CS);

    WSADATA     wsData;
    int         hr=-1;

    printf("Starting Server.\nPort: %s\n", sPt);


    if(WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
        return -1;


    g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(INVALID_SOCKET == g_scLstn)
        return -1;


    memset(&g_sdLstn, 0, sizeof(g_sdLstn));
    g_sdLstn.sin_family      = AF_INET;
    g_sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    g_sdLstn.sin_port        = htons( atoi(sPt) );

    hr = bind(g_scLstn, (SOCKADDR*)&g_sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;


    hr = listen(g_scLstn, SOMAXCONN);
    if( SOCKET_ERROR == hr)
        return -1;


    //Accept용 Thread
    g_hThAcc = (HANDLE)_beginthreadex(nullptr, 0
                        , (unsigned (__stdcall*)(void*))WorkAcc
                        , nullptr, 0, nullptr);


    // Main process
    while(1)
    {
        //
        Sleep(1000);
    }


    WaitForSingleObject(g_hThAcc, INFINITE);
    CloseHandle(g_hThAcc);


    if(g_hThRcv)
        CloseHandle(g_hThRcv);

    if(g_hThSnd)
        CloseHandle(g_hThRcv);


    closesocket(g_scLstn);
    WSACleanup();


    DeleteCriticalSection(&m_CS);

    return 0;
}



DWORD WINAPI WorkAcc(void *)
{
    int     hr = 0;

    while(1)
    {
        int iSize=sizeof(SOCKADDR_IN);

        // 클라이언트 접속을 기다린다.
        g_scCln = accept(g_scLstn, (SOCKADDR*)&g_sdCln, &iSize);
        if(INVALID_SOCKET == g_scCln)
            continue;

        // create recv/send thread
        // 수신용 Thread
        g_hThRcv = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkRcv
                            , nullptr, 0, nullptr);
            
        
        // 송신용 Thread생성
        g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkSnd
                            , nullptr, 0, nullptr);


        // copy to send buffer
        EnterCriticalSection(&m_CS);

        memset(g_bufSnd, 0, MAX_BUF);
        sprintf(g_bufSnd, "Connected: %d", (int)g_scCln);

        LeaveCriticalSection(&m_CS);

    }

    _endthreadex(0);
    return 0;
}


DWORD WINAPI WorkRcv(void *)
{
    int     hr = 0;

    while(1)
    {
        Sleep(1);
        if(g_scCln)
        {
            int iRcv = 0;
            char bufRcv[MAX_BUF+4]={0};

            iRcv=recv(g_scCln, bufRcv, MAX_BUF, 0);
            if(SOCKET_ERROR == iRcv)
            {
                hr = WSAGetLastError();
                LogGetLastError(hr);
                printf("DisConnect Client:%d\n", (int)g_scCln);
                break;
            }
            else if(0 == iRcv)
            {
                printf("DisConnect Client:%d\n", (int)g_scCln);
                break;
            }


            printf("Recv from Client : %d %s\n", (int)g_scCln, bufRcv);

            // copy to send buffer
            EnterCriticalSection(&m_CS);

            memset(g_bufSnd, 0, MAX_BUF);
            sprintf(g_bufSnd, "%5d : %s", g_scCln, bufRcv);

            LeaveCriticalSection(&m_CS);
        }
    }

    _endthreadex(0);
    return 0;
}


DWORD WINAPI WorkSnd(void *pParam)
{
    int hr = 0;

    while(1)
    {
        Sleep(1);

        int iSnd=0;
        int iLen=0;

        iLen = strlen(g_bufSnd);

        if(0 >= iLen)
            continue;


        iSnd=send(g_scCln, g_bufSnd, iLen, 0);
        if(0 >= iSnd)
        {
            hr = WSAGetLastError();
            LogGetLastError(hr);
            printf("Disconnnect Client:%d\n", (int)g_scCln);
        }


        // clear the send buffer
        EnterCriticalSection(&m_CS);
        memset(g_bufSnd, 0, sizeof( g_bufSnd));
        LeaveCriticalSection(&m_CS);
    }

    _endthreadex(0);
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

