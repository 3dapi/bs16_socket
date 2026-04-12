//
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


SOCKET              g_scHost=0;             // 소켓
SOCKADDR_IN         g_sdHost={0};           // 소켓 어드레스
WSAEVENT            g_seHost=0;             // Event
int                 g_nBuf=0;               // recorded byte
char                g_sBuf[MAX_BUF+4]={0};  // send buffer

HANDLE              g_hThSnd;               //
HANDLE              g_hThRcv;               //
DWORD WINAPI        WorkRcv(void*);         // 비동기 통지용 쓰레드
DWORD WINAPI        WorkSnd(void*);

CRITICAL_SECTION    m_CS;                   // 임계 구역



int main()
{
    // 임계 구역 초기화
    InitializeCriticalSection(&m_CS);


    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Client.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_scHost)
        return -1;


    // Nagle off
    int v = 1;
    hr = setsockopt(g_scHost, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
    if(SOCKET_ERROR == hr)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
        return -1;
    }


    memset(&g_sdHost, 0, sizeof(g_sdHost));
    g_sdHost.sin_family      = AF_INET;
    g_sdHost.sin_addr.s_addr = inet_addr(sIp);
    g_sdHost.sin_port        = htons( atoi(sPt) );


    // event 객체 생성
    g_seHost = WSACreateEvent();

    // <소켓, 이벤트, 이벤트 통지 종류> 바인딩
    // 소켓은 자동으로 Non-blocking으로 전환
    hr = WSAEventSelect(g_scHost, g_seHost, FD_CONNECT|FD_READ|FD_WRITE|FD_CLOSE);


    hr = connect(g_scHost, (SOCKADDR*)&g_sdHost, sizeof(SOCKADDR_IN));

    // EventSelect모델은 자동으로 Non-blocking 모델로 바뀝니다.
    // 따라서 에러가 발생했을 때 정확하게 이것이 에러인지
    // WSAGetLastError() 함수를 통해서 확인해야 합니다.
    if(SOCKET_ERROR ==hr)
    {
        Sleep(10);
        hr = WSAGetLastError();

        if(WSAEWOULDBLOCK !=hr)
        {
            return -1;
        }
    }

    //데이터 송신용 스레드 생성. 일시 중지 상태
    g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
                    , (unsigned (__stdcall*)(void*))WorkSnd
                    , nullptr, CREATE_SUSPENDED, nullptr);

    // 비동기 통지용 쓰레드생성
    g_hThRcv = (HANDLE)_beginthreadex(nullptr, 0
                    , (unsigned (__stdcall*)(void*))WorkRcv
                    , nullptr, 0, nullptr);



    while(g_scHost)
    {
        int     hr = 0;
        int     iLen=0;                 //
        char    sSnd[MAX_BUF]={0};      // 테스트용 송신 버퍼

        // 채팅
        fgets(sSnd, MAX_BUF, stdin);
        iLen = strlen(sSnd);

        if(1 >iLen)
            continue;

        if('\n' == sSnd[iLen-1])
        {
            sSnd[iLen-1] =0;
            --iLen;
        }

        /*
        // 테스트
        static int nTstValue = 0;

        Sleep(100);
        ++nTstValue;
        sprintf(sSnd, "ClientMsg- %4d", nTstValue);
        iLen = strlen(sSnd);
        */


        if(0 >= iLen)
            continue;


        // 송신 버퍼에 채우기
        EnterCriticalSection(&m_CS);
            g_nBuf= iLen;
            memcpy(g_sBuf, sSnd, iLen);
        LeaveCriticalSection(&m_CS);

        // 송신 스레드 재개
        ResumeThread(g_hThSnd);
    }


    shutdown(g_scHost, SD_BOTH);
    closesocket(g_scHost);

    g_scHost = {};

    SetEvent(g_seHost);

    // 쓰레드 핸들 해제
    CloseHandle(g_hThRcv);
    CloseHandle(g_hThSnd);

    WSACleanup();

    DeleteCriticalSection(&m_CS);

    return 0;
}



DWORD WINAPI WorkSnd(void* pParam)
{
    while(g_scHost)
    {
        EnterCriticalSection(&m_CS);

        int hr = 0;
        int iTot=0;         // Total Sending Data
        int iLen=g_nBuf;    //

        while(iTot<iLen)
        {
            char* p = g_sBuf + iTot;

            hr = send(g_scHost, p, iLen-iTot, 0);
            if(SOCKET_ERROR == hr)
            {
                hr = WSAGetLastError();
                if(WSAEWOULDBLOCK == hr)
                    continue;

                // socket error
                hr = -1;
                printf("Network closeclosed\n");
                break;
            }

            iTot += hr;
        }

        // send buffer clear
        memset(g_sBuf, 0, g_nBuf);
        g_nBuf = 0;

        LeaveCriticalSection(&m_CS);


        if(FAILED(hr))
            goto END;

        // 송신 스레드 일시 중지
        HANDLE hThread = GetCurrentThread();
        SuspendThread(hThread);
    }

END:
    _endthreadex(0);
    return 0;
}



DWORD WINAPI WorkRcv(void* pParam)
{
    while(g_scHost)
    {
        int hr = 0;
        WSANETWORKEVENTS wnE={0};


        // event가 발생할 때까지 기다림
        //hr = WaitForSingleObject(g_seHost, FALSE, INFINITE);
        hr = WSAWaitForMultipleEvents(1, &g_seHost, FALSE, WSA_INFINITE, FALSE);
        if(0 == g_scHost)
            break;


        // 에러
        if(WSA_WAIT_FAILED == hr)
        {
            printf("Err::WSAWaitForMultipleEvents\n");
            hr = WSAGetLastError();
            LogGetLastError(hr);
            break;
        }

        // event 분해
        hr = WSAEnumNetworkEvents(g_scHost, g_seHost, &wnE);
        if(SOCKET_ERROR == hr)
        {
            printf("Err::WSAEnumNetworkEvents\n");

            hr = WSAGetLastError();
            LogGetLastError(hr);
            break;
        }


        ////////////////////////////////////////////////////////////////////////
        // event process

        // connection event
        if( FD_CONNECT & wnE.lNetworkEvents)
        {
            if(wnE.iErrorCode[FD_CONNECT_BIT])
            {
                hr = wnE.iErrorCode[FD_CONNECT_BIT];
                LogGetLastError(hr);
                break;
            }

            printf("Connection Successed\n");
        }

        // Sending event
        else if( FD_WRITE & wnE.lNetworkEvents)
        {
        }

        // receive event
        else if( FD_READ & wnE.lNetworkEvents)
        {
            char sRcv[MAX_BUF+4]={0};

            int iRcv = recv(g_scHost, sRcv, MAX_BUF, 0);

            if(0>iRcv)
            {
                hr = WSAGetLastError();
                if(WSAEWOULDBLOCK == hr)
                    continue;

                printf("Network Close\n");
                break;
            }
            if(0 == iRcv)
            {
                printf("Network Close\n");
                break;
            }
            else
            {
                printf("Recv from server : %s\n", sRcv);
            }
        }

        // close event
        else if( FD_CLOSE & wnE.lNetworkEvents)
        {
            printf("Network Close\n");
            break;
        }

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

