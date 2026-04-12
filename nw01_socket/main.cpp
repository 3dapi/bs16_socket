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

#include <stdio.h>


void LogGetLastError();
void LogGetLastError(int hr);


int main()
{
    WSADATA     wsData{};
    int         hr = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Load WinSock DLL
    hr = WSAStartup(MAKEWORD(2, 2), &wsData);
    if(0 != hr)
        return -1;

    DWORD dProtoBuf=0;

    // 첫 번째, 두 번째 인수가 NULL로 설정되어 있어서 단순히 크기만 얻고
    // 함수 호출 자체는 Error가 되어야 됨.
    hr = WSAEnumProtocols(nullptr, nullptr, &dProtoBuf);

    int nCnt = dProtoBuf/sizeof(WSAPROTOCOL_INFO);

    if(SOCKET_ERROR != hr || 0 == nCnt)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
        return -1;
    }

    ////////////////////////////////////////////////////////////////////////////
    // TCP, UDP를 사용할 수 있는지 확인
    auto proto    = new int[nCnt+1]{};
    auto protoInf = new WSAPROTOCOL_INFO[nCnt+1]{};

    proto[0] = IPPROTO_TCP;
    proto[1] = IPPROTO_UDP;

    hr = WSAEnumProtocols(proto, protoInf, &dProtoBuf);

    delete [] proto;
    delete [] protoInf;

    if(SOCKET_ERROR == hr)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
        return -1;
    }


    ////////////////////////////////////////////////////////////////////////////
    // TCP socket descriptor 생성
    SOCKET scHost=0;
    scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //scHost = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if(INVALID_SOCKET == scHost)
    {
        hr = WSAGetLastError();
        LogGetLastError(hr);
        return -1;
    }


    // network process



    ////////////////////////////////////////////////////////////////////////////
    // release socket descriptor
    closesocket(scHost);


    ////////////////////////////////////////////////////////////////////////////
    // Unload WinSock DLL
    WSACleanup();

    printf("The winsock environment is OK.\n");

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

