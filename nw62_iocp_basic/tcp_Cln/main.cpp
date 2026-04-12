//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     8192

char    sPt[32]="60000";
char    sIp[64]="127.0.0.1";

void LogGetLastError(int hr);
int  GetSystemProcessorCount();
void CloseHostSocket();


SOCKET          g_scHost = {};              // listen socket

// for sending buffer
OVERLAPPED      g_sndOL   {};
DWORD           g_sndTran {};
WSABUF          g_sndWsBuf{};
char            g_sndBuf[MAX_BUF+4]{};      // Io Completion Buffer for send

// for receiving buffer
OVERLAPPED      g_rcvOL   {};
DWORD           g_rcvTran {};
WSABUF          g_rcvWsBuf{};
char            g_rcvBuf[MAX_BUF+4]{};      // Io Completion Buffer for receive

HANDLE          g_hIocp  {};                // IOCP Handle
DWORD   WINAPI  WorkThread(void*);          // Work 쓰레드


int AsyncSend(char* s, int l)
{
    DWORD dFlag = 0;
    memset(&g_sndOL, 0, sizeof(OVERLAPPED));
    memset(g_sndBuf, 0, MAX_BUF);

    memcpy(g_sndBuf, s, l);

    g_sndWsBuf.len = l;
    g_sndWsBuf.buf = g_sndBuf;

    //return WriteFile((HANDLE)g_scHost, g_sndBuf, l, &g_sndTran, &g_sndOL);
    int hr= WSASend(g_scHost, &g_sndWsBuf, 1, &g_sndTran, dFlag, &g_sndOL, nullptr);
    if(0 == hr)
        printf("Send complete: %d byte\n", g_sndTran);

    return hr;
}

int AsyncRecv()
{
    DWORD dFlag = 0;
    memset(&g_rcvOL, 0, sizeof(OVERLAPPED));
    memset(g_rcvBuf, 0, MAX_BUF);

    g_rcvWsBuf.len = MAX_BUF;
    g_rcvWsBuf.buf = g_rcvBuf;

    //return ReadFile((HANDLE)g_scHost, g_rcvBuf, MAX_BUF, &g_rcvTran, &g_rcvOL);
    return WSARecv(g_scHost, &g_rcvWsBuf, 1, &g_rcvTran, &dFlag, &g_rcvOL, nullptr);
}


int main()
{
    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Client.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


                //socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    g_scHost = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP
                        , nullptr, 0, WSA_FLAG_OVERLAPPED);

    if(INVALID_SOCKET == g_scHost)
        return -1;


    // Connection
    SOCKADDR_IN sdHost{};

    sdHost.sin_family      = AF_INET;
    sdHost.sin_addr.s_addr = inet_addr(sIp);
    sdHost.sin_port        = htons( atoi(sPt) );

    hr = connect(g_scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR ==hr)
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
                        , (unsigned (__stdcall*)(void*))WorkThread
                        , nullptr, 0, nullptr);

        CloseHandle(hThWrk);
    }

    ULONG_PTR   pIoKey = (ULONG_PTR)&g_scHost;                                          // IO Key  is Socket
    HANDLE      hRet   = CreateIoCompletionPort((HANDLE)g_scHost, g_hIocp, pIoKey, 0);  // <소켓, IOCP, key> 바인딩


    // 비동기 수신 요청
    AsyncRecv();


    while(g_scHost)
    {
        int     iLen=0;
        char    tsBuf[MAX_BUF]={0};     // Send용 버퍼

        if(false)                       // chatting
        {
            fgets(tsBuf, MAX_BUF, stdin);
            iLen = strlen(tsBuf);

            if(1 >iLen)
                continue;

            if('\n' == tsBuf[iLen-1])
            {
                tsBuf[iLen-1] =0;
                --iLen;
            }
        }
        else                            // Test Sending
        {
            Sleep(500);
            static int nTstValue=0;
            ++nTstValue;
            sprintf(tsBuf, "ClientMsg- %4d", nTstValue);
            iLen = strlen(tsBuf);
        }
    


        if(0 == g_scHost)
            break;

        if(1>iLen)
            continue;

        // 데이터 송신
        hr = AsyncSend(tsBuf, iLen);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                break;
            }
        }
    }
    


    CloseHandle(g_hIocp);

    CloseHostSocket();

    WSACleanup();

    return 0;
}


DWORD WINAPI WorkThread(void* pParam)
{
    int         hr = 0;
    ULONG_PTR   pIoKey  = nullptr;
    OVERLAPPED* pOL     = nullptr;
    DWORD       dTran   = 0;
    DWORD       OLType  = 0;


    while(g_scHost)
    {
        pIoKey  = nullptr;
        pOL     = nullptr;
        dTran   = 0;

        //  If the function dequeues a completion packet for a successful I/O operation from the completion port, the return value is TRUE.
        //  The function stores information in the variables pointed to by the lpNumberOfBytes, lpCompletionKey, and lpOverlapped parameters.
        //
        //  GQCS 함수가 완료포트로부터 성공적인 입출력 처리로 완료패킷을 dequeue 한다면 반환은 TRUE다.
        //  GQCS 함수는 lpNumberOfBytes, lpCompletionKey, lpOverlapped 인수 등의 지정된 변수에 정보들을 저장한다.
        //
        //  If *lpOverlapped is nullptr and the function does not dequeue a completion packet from the completion port, the return value is FALSE.
        //  The function does not store information in the variables pointed to by the lpNumberOfBytes and lpCompletionKey parameters.
        //  To get extended error information, call GetLastError.
        //  If the function did not dequeue a completion packet because the wait timed out, GetLastError returns WAIT_TIMEOUT.
        //
        //  (*lpOverlapped)의 값이 NULL이고, GQCS 함수가 완료패킷을 완료포트로부터 dequeue하지 않았다면 반환 값은 FALSE이다.
        //  GQCS 함수는 lpNumberOfBytes, lpCompletionKey, lpOverlapped 인수 등의 지정된 변수에 정보를 저장하지 않는다.
        //  확장된 에러를 얻기 위해서 GetLastError 함수를 호출하라.
        //  만약 GQCS 함수가 완료패킷을 dequeue하지 못했다면 시간을 초과한 것이고, GetLastError 함수는 WAIT_TIMEOUT을 반환한다.
        //
        //  If *lpOverlapped is not nullptr and the function dequeues a completion packet for a failed I/O operation from the completion port, the return value is FALSE.
        //  The function stores information in the variables pointed to by lpNumberOfBytes, lpCompletionKey, and lpOverlapped.
        //  To get extended error information, call GetLastError.
        //
        //  (*lpOverlapped)의 값이 NULL이 아니고 GQCS 함수가 완료포트로부터 실패한 입출력 처리로 완료패킷을 dequeue하면 반환 값은 FALSE다.
        //  GQCS 함수는 lpNumberOfBytes, lpCompletionKey, lpOverlapped 인수 등의 지정된 변수에 정보들을 저장한다.
        //  확장된 에러를 얻기 위해서 GetLastError 함수를 호출하라. ==> 입/출력 처리가 실패하더라도 변수들의 값들이 채워질 수 있음.

        hr = GetQueuedCompletionStatus(
                g_hIocp                                 // Completion Port
            ,   &dTran                                  // 전송 된 바이트 수
            ,   (PULONG_PTR)&pIoKey
            ,   (LPOVERLAPPED*)&pOL                     // OVERLAPPED 구조체
            ,   INFINITE
            );

        // IO Failed
        if(0 == hr)
        {
            hr = GetLastError();
            LogGetLastError(hr);
            break;
        }

        if(0 == dTran)                                  // disconnect
        {
            printf("Disconnect\n");
            break;
        }

        if(sizeof(ULONG_PTR) == dTran && FD_CONNECT == (DWORD)pIoKey)
        {
            printf("Connection Successed\n");
            continue;
        }


        // Sending complete
        if( ((void*)&g_sndOL) == ((void*)pOL) )
        {
            printf("Write Successed.\n");
        }

        // receiving complete
        else if( ((void*)&g_rcvOL) == ((void*)pOL) )
        {
            printf("Recv from Server(%3d): %s\n", dTran, g_rcvBuf);
            AsyncRecv();
        }

    }


    CloseHostSocket();

    _endthreadex(0);

    return 0;
}


void CloseHostSocket()
{
    shutdown(g_scHost, SD_BOTH);
    closesocket(g_scHost);
    g_scHost = {};
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

