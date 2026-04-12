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

void    LogGetLastError(int hr);


SOCKET  g_scLstn{};                 // 소켓
FD_SET  g_fdSet;                    // 소켓 FD_SET

int     FrameMove();
void    CloseSocket();
void    EchoMsg(char* s, int iLen);

int main()
{
    WSADATA     wsData{};
    SOCKADDR_IN sdLstn{};           // 소켓 어드레스

    int hr=-1;

    printf("Start Server:: Port: %s\n", sPt);


    if(WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    // create the listen socket
    g_scLstn=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(INVALID_SOCKET == g_scLstn)
        return -1;

    // Non blocking socket
    u_long on =1;
    hr = ioctlsocket(g_scLstn, FIONBIO, &on);


    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons( atoi(sPt) );
    hr = bind(g_scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;

    hr =listen(g_scLstn, SOMAXCONN);
    if(SOCKET_ERROR == hr)
        return -1;


    FD_ZERO(&g_fdSet);
    FD_SET(g_scLstn, &g_fdSet);


    while(1)
    {
        if(FAILED(FrameMove()))
            break;

        // 프로세스를 다 사용하므로
        // 이렇게라도 해야 한다.
        Sleep(1);
    }

    CloseSocket();
    WSACleanup();

    return 0;
}



int FrameMove()
{
    int hr=-1;

    if(!g_scLstn)
        return -1;

    FD_SET  fdsTmp;
    TIMEVAL timeout;

    // 이렇게 매번 읽는 루틴에서 시간과 FD_SET을 다시 할당해야
    // 제대로 동작한다.
    fdsTmp = g_fdSet;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    hr = select(0, &fdsTmp, nullptr, nullptr, &timeout);

    if(SOCKET_ERROR == hr)
    {
        // for non-blocking socket
        hr = WSAGetLastError();
        if(WSAEWOULDBLOCK !=hr)
            return -1;

        return 0;
    }

    // time out
    if(0 == hr)
        return 0;


    for(UINT i=0; i<g_fdSet.fd_count; ++i)
    {
        // find the event on fdset
        if(!FD_ISSET(g_fdSet.fd_array[i], &fdsTmp))
            continue;


        // from listen socket
        if(g_scLstn == g_fdSet.fd_array[i])
        {
            SOCKET scCln=0;
            scCln = accept(g_scLstn, nullptr, nullptr);

            // g_fdSet.fd_count를 증가 시킨다.
            FD_SET(scCln, &g_fdSet);

            printf("New Client: %d, Number: %d\n", scCln, (g_fdSet.fd_count-1));
            continue;
        }


        // other socket
        int     iRcv=0;
        char    sBufRcv[MAX_BUF+4]={0};

        iRcv = recv(g_fdSet.fd_array[i], sBufRcv, MAX_BUF, 0);

        // close socket
        if(0 >= iRcv)
        {
            // close
            printf("Socket Close: %d \n", g_fdSet.fd_array[i]);

            shutdown(g_fdSet.fd_array[i], SD_BOTH);
            closesocket(g_fdSet.fd_array[i]);

            // g_fdSet.fd_count를 감소 시킨다.
            FD_CLR(g_fdSet.fd_array[i], &g_fdSet);
            printf("Client Number: %d \n", (g_fdSet.fd_count-1));
            continue;
        }


        // test. send the receive message to all client
        int     iSnd=0;
        char    sBufSnd[MAX_BUF+4]={0};

        sprintf(sBufSnd, "%d %s", g_fdSet.fd_array[i], sBufRcv);
        iSnd = strlen(sBufSnd);

        EchoMsg(sBufSnd, iSnd);
    }

    return 0;
}



void CloseSocket()
{
    // 종료
    if(g_scLstn)
    {
        shutdown(g_scLstn, SD_BOTH);
        closesocket(g_scLstn);
        g_scLstn=0;
    }
}



void EchoMsg(char* s, int iLen)
{
    for(UINT j=0;j<g_fdSet.fd_count; ++j)
    {
        if(g_scLstn != g_fdSet.fd_array[j])
            send(g_fdSet.fd_array[j], s, iLen, 0);
    }
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

