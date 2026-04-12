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


char    sPt[32]="60000";

void LogGetLastError(int hr);


int main()
{
    WSADATA     wsData{};
    SOCKET      scLstn{};
    SOCKADDR_IN sdLstn{};

    SOCKET      scCln{};
    SOCKADDR_IN sdCln{};
    int         hr=-1;


    char sBufSnd[1024]="Welcome to network programming!!!";

    printf("Starting Server.\nPort: %s\n", sPt);


    // Load Winsock DLL
    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    // Listen 소켓 생성
    scLstn=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(INVALID_SOCKET == scLstn)
        return -1;


    // Listen 소켓 주소 할당
    memset(&sdLstn, 0, sizeof(sdLstn));
    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons( atoi(sPt) );

    hr = bind(scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;


    // Listen 상태
    hr = listen(scLstn, SOMAXCONN);
    if( SOCKET_ERROR == hr)
        return -1;


    // listen socket을 Nonblocking 소켓 설정
    u_long nonBlocking =1;
    hr = ioctlsocket(scLstn, FIONBIO, &nonBlocking);
    if(SOCKET_ERROR == hr)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
    }


    while(1)
    {
        // 연결 요청 수락
        int iSize=sizeof(SOCKADDR_IN);

        scCln=accept(scLstn, (SOCKADDR*)&sdCln, &iSize);

        if(INVALID_SOCKET == scCln)
        {
            hr = WSAGetLastError();

            // This error is returned from operations on nonblocking sockets that
            // cannot be completed immediately
            if( WSAEWOULDBLOCK != hr)
                goto END;

            continue;
        }

        // 클라이언트가 연결됨
        char* sIP = inet_ntoa( sdCln.sin_addr);
        printf("Connect Client IP: %s\n", sIP);

        //printf("Connect Client IP: %d.%d.%d.%d\n"
        //      , sdCln.sin_addr.S_un.S_un_b.s_b1
        //      , sdCln.sin_addr.S_un.S_un_b.s_b2
        //      , sdCln.sin_addr.S_un.S_un_b.s_b3
        //      , sdCln.sin_addr.S_un.S_un_b.s_b4 );


        // 클라이언트와 통신하는 소켓을 Nonblocking 소켓으로 설정
        u_long nonBlocking =1;
        hr = ioctlsocket(scCln, FIONBIO, &nonBlocking);
        if(SOCKET_ERROR == hr)
        {
            hr = WSAGetLastError();
            LogGetLastError(hr);
        }

        break;
    }


    // 테스트 통신
    static int iCnt =0;
    while(50>iCnt)
    {
        Sleep(100);

        ++iCnt;
        sprintf(sBufSnd, "Network message %d", iCnt);

        // 데이터 송신
        send(scCln, sBufSnd, strlen(sBufSnd), 0);
    }


END:
    // 연결 종료
    shutdown(scCln, SD_BOTH);
    closesocket(scCln);
    closesocket(scLstn);

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

