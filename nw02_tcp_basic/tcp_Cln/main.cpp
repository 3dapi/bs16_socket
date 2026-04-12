//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)


// Winsock 을 사용하기 위한 라이브러리
#pragma comment(lib, "ws2_32.lib")

// winsock2.h 헤더파일은 Windows.h 헤더파일보다 항상 앞서있어야 한다.
#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     8192


char    sPt[32]="60000";
char    sIp[64]="127.0.0.1";


// TCP Client Network 설정
// Load WinSock DLL Loading
// 서버 접속을 위한 TCP소켓 생성
// 서버로 연결 요청
// 데이터 입/출력
// 연결 종료
// Unload WinSock DLL

void LogGetLastError(int hr);


int main()
{
    WSADATA     wsData{};
    SOCKET      scHost{};
    SOCKADDR_IN sdHost{};


    int         hr =-1;

    printf("Starting Client.\nPort: %s\n", sPt);

    // Load WinSock DLL
    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;



    // 서버 접속을 위한 TCP소켓 생성
    scHost=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == scHost)
        return -1;


    // 서버로 연결 요청
    sdHost.sin_family      = AF_INET;
    sdHost.sin_addr.s_addr = inet_addr(sIp);
    sdHost.sin_port        = htons( atoi(sPt) );

    hr = connect(scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
        return -1;
    }


    while(1)
    {
        char sBufRcv[MAX_BUF+4]={0};
        int iRcv=0;


        // 데이터 입/출력==>(수신/송신)
        iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);

        if(SOCKET_ERROR == iRcv)
        {
            printf("Receive Socket Error\n");

            hr = WSAGetLastError();
            LogGetLastError(hr);
            break;
        }
        else if(0 == iRcv)
        {
            printf("Gracefully closed\n");
            break;
        }
        else if(iRcv)
        {
            printf("Recv from server : %s\n", sBufRcv);
        }
    }


    // 연결 종료
    closesocket(scHost);

    // Unload WinSock DLL
    WSACleanup();

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

