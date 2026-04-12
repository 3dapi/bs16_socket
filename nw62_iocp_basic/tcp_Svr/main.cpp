//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "ws2_32.lib")

#include <vector>
using namespace std;

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUF     8192

char    sPt[32]="60000";

void LogGetLastError(int hr);
void GetIp(char* s, SOCKET scH);
int  GetSystemProcessorCount();
void CloseHostSocket();


struct RemoteHost
{
    SOCKET          scH{};                  // socket
    int             nUse{1};                // enable

    OVERLAPPED      olRcv;                  // For WSARecv
    WSABUF          wsBuf;
    char            csBuf[MAX_BUF+4];       // Io Completion Buffer

    RemoteHost()
    {
        nUse    = 1;
        Reset();
    }

    RemoteHost(SOCKET s)
    {
        scH     = s;
        nUse    = 1;
        Reset();
    }

    void Reset()
    {
        memset(&olRcv, 0, sizeof(OVERLAPPED));
        memset(csBuf, 0, MAX_BUF+4);
        wsBuf.len = MAX_BUF;
        wsBuf.buf = csBuf;
    }

    void Destroy()
    {
        if(scH)
        {
            shutdown(scH, SD_BOTH);
            closesocket(scH);
            scH = 0;
        }

        nUse    = -1;
        Reset();
    }


    void SetUse(int v){ nUse    = v;    }
};


SOCKET                  g_scLstn = 0;           // listen socket

vector<RemoteHost* >    g_vHost;                // Client list
CRITICAL_SECTION        m_CS;                   // critical section

HANDLE  g_hIocp         {};                     // IOCP Handle
unsigned WINAPI WorkThread(void*);              // Work 쓰레드


// for sending buffer
DWORD   g_sndTran = 0;
WSABUF  g_sndWsBuf={0};
char    g_sndBuf[MAX_BUF+4]={0};                // Io Completion Buffer for send

void    EchoMsg(char* s, int l);                // echo message



void DeleteNotUseHost()
{
    EnterCriticalSection(&m_CS);
    vector<RemoteHost* >::iterator _F = g_vHost.begin();

    for( ; _F != g_vHost.end(); )
    {
        RemoteHost* pCln = (*_F);

        if(pCln && 0 >= pCln->nUse)
        {
            pCln->Destroy();

            delete pCln;
            _F = g_vHost.erase(_F);
            continue;
        }

        ++_F;
    }

    LeaveCriticalSection(&m_CS);
}


RemoteHost* FindNotUseHost()
{
    int iSize = (int)g_vHost.size();

    for(int i=0; i<iSize; ++i)
    {
        if(0 >= g_vHost[i]->nUse)
            return g_vHost[i];
    }

    return nullptr;
}


void DeleteAllHost()
{
    for(auto host : g_vHost)
        delete host;

    g_vHost.clear();
}


unsigned WINAPI TstThread(void* pParam)
{
    Sleep(5000);
    CloseHandle(g_hIocp);
    return 0;
}



int main()
{
    InitializeCriticalSection(&m_CS);


    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Server.\nPort: %s\n", sPt);


    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;

                //socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    g_scLstn = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP
                        , nullptr, 0, WSA_FLAG_OVERLAPPED);


    if(INVALID_SOCKET == g_scLstn)
        return -1;


    // IOCP 객체 생성
    g_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

    if(nullptr == g_hIocp)
        return -1;


    // 2 * processor 개수만큼 Work 쓰레드 생성
    int nWrkPrc = 2 * GetSystemProcessorCount();
    for(int i=0; i<nWrkPrc; ++i)
    {
        auto hThWrk = (HANDLE)_beginthreadex(nullptr, 0
                        , WorkThread
                        , nullptr, 0, nullptr);

        CloseHandle(hThWrk);
    }

    //_beginthreadex(nullptr, 0, TstThread, nullptr, 0, nullptr);


    SOCKADDR_IN sdLstn = {0};

    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons(atoi(sPt) );

    hr = bind(g_scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;

    hr = listen(g_scLstn, SOMAXCONN);
    if(SOCKET_ERROR ==hr)
        return -1;

    // Accept
    while(1)
    {
        SOCKET      scCln= 0;
        SOCKADDR_IN sdCln= {0};
        int         scSize= sizeof(sdCln);


        DeleteNotUseHost();

        scCln = accept(g_scLstn, (SOCKADDR*)&sdCln, &scSize);
        if(INVALID_SOCKET == scCln)
            break;

        printf("New Client: %5d %s\n", (int)scCln, inet_ntoa(sdCln.sin_addr));


        // create client intance
        RemoteHost* pCln = new RemoteHost(scCln);
        ULONG_PTR   pIoKey = (ULONG_PTR)pCln;                                           // Key is client Object
        HANDLE      hRet = CreateIoCompletionPort((HANDLE)scCln, g_hIocp, pIoKey, 0);   // <클라이언트 소켓, IOCP, key> binding


        // 비동기 수신 요청에 대한 초기화
        DWORD dTran = 0;
        DWORD dFlag = 0;

        memset(&pCln->olRcv, 0, sizeof(OVERLAPPED));    // OL 구조체 초기화
        memset(pCln->csBuf, 0, MAX_BUF+4);              // 수신 버퍼 초기화
        pCln->wsBuf.len = MAX_BUF;                      // 최대 버퍼 크기로 설정함
        pCln->wsBuf.buf = pCln->csBuf;                  // 수신 받을 데이터 저장소 지정

        // 비동기 수신 요청

        //hr = ReadFile((HANDLE)pCln->scH, pCln->csBuf, MAX_BUF, &dTran, &pCln->olRcv);
        hr = WSARecv(pCln->scH, &pCln->wsBuf, 1, &dTran, &dFlag, &pCln->olRcv, nullptr);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                pCln->Destroy();
                delete pCln;

                continue;
            }
        }

        // add the client list
        g_vHost.push_back(pCln);
    }


    DeleteAllHost();
    CloseHostSocket();

    WSACleanup();

    DeleteCriticalSection(&m_CS);

    return 0;
}


unsigned WINAPI WorkThread(void* pParam)
{
    int     hr = 0;

    RemoteHost* pCln    = nullptr;
    OVERLAPPED* pOlEx   = nullptr;
    DWORD       dTran   = 0;

    while(1)
    {
        pCln    = nullptr;
        pOlEx   = nullptr;
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
                        g_hIocp                 // Completion Port
                    ,   &dTran                  // 전송 된 바이트 수
                    ,   (PULONG_PTR)&pCln       // IO Completion Key
                    ,   (LPOVERLAPPED*)&pOlEx   // OVERLAPPED 구조체
                    ,   INFINITE
                    );

        // IO Failed
        if(0 == hr)
        {
            hr = WSAGetLastError();
            LogGetLastError(hr);

            if(nullptr == pOlEx && nullptr == pCln)
                break;


            if(pOlEx && pCln)
            {
                SOCKET  scHost  = pCln->scH;

                printf("Disconnect Client: %d\n", (int)scHost);
                pCln->SetUse(0);
                continue;
            }

            continue;
        }


        SOCKET  scHost  = pCln->scH;

        if(0 == dTran)
        {
            printf("Disconnect Client: %d\n", (int)scHost);
            pCln->SetUse(0);
            continue;
        }


        // 수신 완료
        printf("Recv from Client[%4d]: %s\n", (int)scHost, pCln->csBuf);

        int     iLen=0;
        char    sSnd[MAX_BUF]={0};

        sprintf(sSnd, "%5d> %s", (int)scHost, pCln->csBuf);
        iLen = strlen(sSnd);

        EchoMsg(sSnd, iLen);


        // 비동기 수신 요청에 대한 초기화
        DWORD dTran = 0;
        DWORD dFlag = 0;

        memset(&pCln->olRcv, 0, sizeof(OVERLAPPED));    // OL 구조체 초기화
        memset(pCln->csBuf, 0, MAX_BUF+4);              // 수신 버퍼 초기화
        pCln->wsBuf.len = MAX_BUF;                      // 최대 버퍼 크기로 설정함
        pCln->wsBuf.buf = pCln->csBuf;                  // 수신 받을 데이터 저장소 지정

        // 비동기 수신 요청
        hr = WSARecv(pCln->scH, &pCln->wsBuf, 1, &dTran, &dFlag, &pCln->olRcv, nullptr);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                pCln->Destroy();
                delete pCln;

                continue;
            }
        }
    }

    CloseHostSocket();

    _endthreadex(0);

    return 0;
}




// echo message
void EchoMsg(char* s, int l)
{
    EnterCriticalSection(&m_CS);

    int hr = 0;
    int iSize = (int)g_vHost.size();

    RemoteHost* pCln    = nullptr;
    SOCKET      scHost  = 0;

    DWORD       dTran   = 0;

    memset(g_sndBuf, 0, MAX_BUF+4); // 송신 버퍼 초기화
    memcpy(g_sndBuf, s, l);         // 송신 버퍼에 복사
    g_sndWsBuf.len = l;             // 길이 저장
    g_sndWsBuf.buf = g_sndBuf;      // 수신 받을 데이터 저장소 지정

    for(int i=0; i<iSize; ++i)
    {
        pCln   = g_vHost[i];
        scHost = pCln->scH;

        if(0 >= scHost || 0 >= pCln->nUse)
            continue;


        // 전송: 중첩이 없는 소켓에 대해서 (lpOverlapped, lpCompletionRoutine) 값이 NULL이면 send와 동일
        hr = WSASend(scHost, &g_sndWsBuf, 1, &dTran, 0, nullptr, nullptr);
        if(0 == hr)
        {
            printf("Send complete: %d byte\n", dTran);
        }
        else if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                pCln->nUse = 0;
                continue;
            }
        }
    }

    LeaveCriticalSection(&m_CS);
}


void CloseHostSocket()
{
    shutdown(g_scLstn, SD_BOTH);
    closesocket(g_scLstn);
    g_scLstn = 0;
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


void GetIp(char* s, SOCKET scH)
{
    int size = sizeof(SOCKADDR_IN);
    SOCKADDR_IN sdH ={0};
    getpeername(scH, (SOCKADDR *)&sdH, &size);
    strcpy(s, inet_ntoa(sdH.sin_addr) );
}

