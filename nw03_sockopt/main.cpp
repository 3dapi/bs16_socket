//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>


void LogGetLastError();
void LogGetLastError(int hr);

int main()
{
    WSADATA     wsData{};
    int         hr = 0;
    SOCKET      scHost{};

    hr = WSAStartup(MAKEWORD(2, 2), &wsData);
    if(0 != hr)
        return -1;


    scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(SOCKET_ERROR == scHost)
        return -1;


    int v = 0;
    int size = sizeof(v);


    // Non-blocking 소켓 설정
    hr = ioctlsocket(scHost, FIONBIO, (u_long*)&v);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }


    // Nagle off
    v = 1;
    hr = setsockopt(scHost, IPPROTO_TCP, TCP_NODELAY, (char*)&v, size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }


    // SO_REUSEADDR: 반드시 bind 전에
    v = 1;
    hr = setsockopt(scHost, SOL_SOCKET, SO_REUSEADDR, (char*)&v, size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }


    // SO_DONTLINGER: do not linger on close waiting for unsent data to be sent
    v = 1;
    hr = setsockopt(scHost, SOL_SOCKET, SO_DONTLINGER, (char*)&v, size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }


    // SO_RCVBUF: check the receive buffer size for recv

    v = 0;
    size = sizeof(v);
    hr = setsockopt(scHost, SOL_SOCKET, SO_RCVBUF, (char*)&v, size);

    // SO_RCVBUF: recv buffer = 0 => directly perform i/o on the buffer(no buffering and no copy)
    v = 0;
    size = 0;
    hr = getsockopt(scHost, SOL_SOCKET, SO_RCVBUF, (char*)&v, &size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }
    else
        printf("Recv buffer: %d\n", v);



    // SO_SNDBUF: check the send buffer size for send
    v = 0;
    hr = getsockopt(scHost, SOL_SOCKET, SO_SNDBUF, (char*)&v, &size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }
    else
        printf("Send buffer: %d\n", v);


    size = sizeof(v);

    // SO_SNDBUF: send buffer = 0 => directly perform i/o on the buffer(no buffering and no copy)
    v = 0;
    hr = setsockopt(scHost, SOL_SOCKET, SO_SNDBUF, (char*)&v, size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }


    // Linger 설정
    LINGER l={1,0};
    size = sizeof(LINGER);
    hr = setsockopt(scHost, SOL_SOCKET, SO_LINGER, (char*)&l, size);
    if(SOCKET_ERROR == hr)
    {
        LogGetLastError();
        return -1;
    }

    // Shutdown both send and receive operations.
    shutdown(scHost, SD_BOTH);


    closesocket(scHost);
    WSACleanup();

    return 0;
}




void LogGetLastError()
{
    int hr = WSAGetLastError();
    char* lpMsgBuf;
    FormatMessage( 
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS
                , nullptr, hr, 0, (LPSTR)&lpMsgBuf, 0, nullptr );

    printf( "%s\n", lpMsgBuf);
    LocalFree( lpMsgBuf );
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

