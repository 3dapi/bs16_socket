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
#define MAX_HOST    WSA_MAXIMUM_WAIT_EVENTS

char    sPt[32]="60000";

void LogGetLastError(int hr);
void GetIp(char* s, SOCKET scH);


struct RemoteHost
{
    WSAOVERLAPPED   ol{};
    WSABUF          wsBuf{};

    SOCKET          scH{};                  // 소켓
    char            csBuf[MAX_BUF+4];       // 읽기 버퍼

    RemoteHost()
    {
        wsBuf.len = MAX_BUF;
        wsBuf.buf = csBuf;
    }

    RemoteHost(SOCKET s, WSAEVENT e)
    {
        memset(&ol, 0, sizeof(WSAOVERLAPPED));
        memset(csBuf, 0, sizeof(csBuf));
        wsBuf.len = MAX_BUF;
        wsBuf.buf = csBuf;

        ol.hEvent = e;
        scH       = s;
    }

    void SetEvent() {   WSASetEvent(ol.hEvent);     }
    void ResetEvent(){  WSAResetEvent(ol.hEvent);   }

    void Close()
    {
        if(0 == scH)
            return;

        shutdown(scH, SD_BOTH);
        closesocket(scH);
        scH = 0;

        CloseHandle(ol.hEvent);
        ol.hEvent = nullptr;

        memset(csBuf, 0, MAX_BUF);
    }
};

SOCKET                  g_scLstn = 0;           // listen socket
vector<RemoteHost* >    g_vHost;                // host list
CRITICAL_SECTION        m_CS;                   // critical section

DWORD WINAPI    WorkThread(void*);              // work thread

void            EchoMsg(char* s, int len);      // echo message



RemoteHost* FindHost(HANDLE e)
{
    if(nullptr == e)
        return nullptr;

    auto iSize = (int)g_vHost.size();

    for(int i=0; i<iSize; ++i)
    {
        if(e == g_vHost[i]->ol.hEvent)
            return g_vHost[i];
    }

    return nullptr;
}

void DeleteHost(SOCKET scH)
{
    int iSize = (int)g_vHost.size();

    for(int i=0; i<iSize; ++i)
    {
        if(scH == g_vHost[i]->scH)
        {
            delete g_vHost[i];
            g_vHost.erase( g_vHost.begin() + i);
            return;
        }
    }

}

void DeleteNotUseHost()
{
    EnterCriticalSection(&m_CS);
    auto _F = g_vHost.begin();

    for( ; _F != g_vHost.end(); )
    {
        RemoteHost* p = (*_F);

        if(p && 0 >= p->scH)
        {
            delete p;
            _F = g_vHost.erase(_F);
            continue;
        }

        ++_F;
    }

    LeaveCriticalSection(&m_CS);
}


void DeleteAllHost()
{
    for(auto host : g_vHost)
        delete host;

    g_vHost.clear();
}



int main()
{
    InitializeCriticalSection(&m_CS);

    WSADATA     wsData{};
    int         hr =-1;

    SOCKADDR_IN sdLstn ={0};
    WSAEVENT    seLstn =nullptr;

    printf("Starting Server.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;

    g_scLstn = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    //g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_scLstn)
        return -1;


    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons(atoi(sPt) );

    hr = bind(g_scLstn, (SOCKADDR *)&sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;

    hr = listen(g_scLstn, SOMAXCONN);
    if(SOCKET_ERROR ==hr)
        return -1;

    // create event and add the host list to zero index
    seLstn = WSACreateEvent();
    g_vHost.push_back( new RemoteHost(g_scLstn, seLstn) );

    // work 스레드 생성
    HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkThread
                            , nullptr, 0, nullptr);

    // Work thread test..
    // work thread에서 dead lock이 발생하지 않도록 이벤트를 signaled로 전환
    g_vHost[0]->SetEvent();

    while(1)
    {
        SOCKET      scCln= 0;
        SOCKADDR_IN sdCln= {0};
        WSAEVENT    seCln= nullptr;
        int         scSize= sizeof(sdCln);

        scCln = accept(g_scLstn, (SOCKADDR*)&sdCln, &scSize);
        if(INVALID_SOCKET == scCln)
            break;

        printf("New Client: %5d %s\n", (int)scCln, inet_ntoa(sdCln.sin_addr));


        // 호스트 리스트를 추가할 수 있는 지 검사.
        if(MAX_HOST <= (int)g_vHost.size() )
        {
            printf("There is no empty element for client.\n");

            // disconnect the client
            shutdown(scCln, SD_BOTH);
            closesocket(scCln);
        }

        // Event를 Notify 하기 위해 접속한 클라이언트에게 소켓 번호를 전송
        char sId[128]={0};
        sprintf(sId, "Connected: %d", (int)scCln);
        send(scCln, sId, strlen(sId), 0);


        // Nagle off
        int v = 1;
        hr = setsockopt(scCln, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
        if(SOCKET_ERROR == hr)
            hr = WSAGetLastError();



        // read용 이벤트 생성
        seCln       = WSACreateEvent();

        RemoteHost* pCln = new RemoteHost(scCln, seCln);
        DWORD       dFlag=0;
        DWORD       dTran= 0;

        // 비동기 입출력 요청
        hr = ReadFile((HANDLE)pCln->scH, pCln->csBuf, MAX_BUF, &dTran, &pCln->ol);
        //hr = WSARecv(pCln->scH, &pCln->wsBuf, 1, &dTran, &dFlag, &pCln->ol, nullptr);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                delete pCln;
                continue;
            }
        }

        // add the host list
        g_vHost.push_back(pCln);

        // work thread에 클라이언트 추가됨을 알림
        g_vHost[0]->SetEvent();
    }


    CloseHandle(hWork);


    // 윈속 종료
    WSACleanup();
    DeleteCriticalSection(&m_CS);

    return 0;
}

// 비동기 입출력 완료  처리 함수
DWORD WINAPI WorkThread(void* pParam)
{
    while(1)
    {
        RemoteHost*     pHost = nullptr;

        WSAEVENT        vEvn[MAX_HOST]={0};     // Event List

        int             hr = 0;
        int             i  = 0, nLst=0;
        int             nE =-1;

        // listen 포함, 클라이언트의 모든 이벤트를 리스트에 설정
        for(i=0; i<(int)g_vHost.size(); ++i)
        {
            RemoteHost* p =  g_vHost[i];
            if(0 == p->scH)
                continue;

            vEvn[nLst] = p->ol.hEvent;
            ++nLst;
        }

        // 신호 상태 이벤트 기다림
        nE = WSAWaitForMultipleEvents(nLst, vEvn, FALSE, WSA_INFINITE, FALSE);
        if(nE == WSA_WAIT_FAILED)
            continue;

        nE -= WSA_WAIT_EVENT_0;     // 인덱스 재조정

        for(i= nE; i<nLst; ++i)     // find the signaled event
        {
            hr = WSAWaitForMultipleEvents(1, &vEvn[i], TRUE, 0, FALSE);
            if( WSA_WAIT_FAILED  == hr || WSA_WAIT_TIMEOUT == hr)
                continue;


            // get the host
            pHost = FindHost(vEvn[i]);
            if(nullptr == pHost)
                continue;

            SOCKET scHost = pHost->scH;


            // event to non-signal
            pHost->ResetEvent();

            // listen socket
            if(g_scLstn == scHost)
                continue;


            // other client list process

            // 비동기 입출력 결과 확인
            DWORD   dFlag=0;
            DWORD   dTran= 0;
            hr = WSAGetOverlappedResult(scHost, &pHost->ol, &dTran, FALSE, &dFlag);

            if(FALSE == hr || 0 == dTran)
            {
                pHost->Close();
                printf("Disconnect Client: %d\n", (int)scHost);
                continue;
            }

            // receive data
            if(0<dTran)
            {
                printf("Recv from Client : %d %s\n", (int)scHost, pHost->csBuf);

                // test message
                char    sBufBuf[MAX_BUF]={0};
                sprintf(sBufBuf, "%5d : %s", (int)scHost, pHost->csBuf);

                // send the message to all client
                EchoMsg(sBufBuf, strlen(sBufBuf) );
            }



            // clear the transfer buffer
            memset(pHost->csBuf, 0, MAX_BUF);


            // 다시 요청
            dFlag=0; dTran= 0;

            hr = ReadFile((HANDLE)scHost, &pHost->csBuf, MAX_BUF, &dTran, &pHost->ol);
            //hr = WSARecv(scHost, &pHost->wsBuf, 1, &dTran, &dFlag, &pHost->ol, nullptr);
            if(SOCKET_ERROR == hr)
            {
                hr =  WSAGetLastError();
                if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
                {
                    LogGetLastError(hr);
                    pHost->Close();
                }
            }
        }

        DeleteNotUseHost();
    }

    _endthreadex(0);
    return 0;
}

void EchoMsg(char* s, int iLen)
{
    EnterCriticalSection(&m_CS);

    int hr = 0;
    int iSize = (int)g_vHost.size();

    for(int i=1; i<iSize; ++i)
    {
        RemoteHost* pCln = g_vHost[i];
        if(0 >= pCln->scH)
            continue;

        WSABUF  wsBuf={0};
        DWORD   dSent=0;


        wsBuf.buf = s;
        wsBuf.len = iLen;
        //hr = send(pCln->scH, s, iLen, 0);
        hr = WSASend(pCln->scH, &wsBuf, 1, &dSent, 0, nullptr, nullptr);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                pCln->Close();
            }
        }
    }

    LeaveCriticalSection(&m_CS);
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

