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


struct OVERLAP_EX : public WSAOVERLAPPED
{
    void*       pOwn{};
    DWORD       dType{};
    DWORD       dFlag{};
    DWORD       dTran{};
    WSABUF      wsBuf{};
    char        csBuf[MAX_BUF+4]{};

    OVERLAP_EX()
    {
        Reset();
    }

    OVERLAP_EX(void* _pOwn)
    {
        Reset();
        pOwn    = _pOwn;
    }

    void Reset()
    {
        memset(this, 0, sizeof(WSAOVERLAPPED) );
        memset(csBuf, 0, MAX_BUF+4 );

        dFlag   = 0;
        dTran   = 0;
        wsBuf.len = MAX_BUF;
        wsBuf.buf = csBuf;
    }

    void SetBuf(char* s, int len)
    {
        wsBuf.len = len;
        memcpy(csBuf, s, len);
    }

    // for receive
    int AsyncRcv(SOCKET s, LPWSAOVERLAPPED_COMPLETION_ROUTINE pFunc)
    {
         return WSARecv(s, &wsBuf, 1, &dTran, &dFlag, this, pFunc);
    }

    // for sending
    int AsyncSnd(SOCKET s, LPWSAOVERLAPPED_COMPLETION_ROUTINE pFunc)
    {
         return WSASend(s, &wsBuf, 1, &dTran, dFlag, this, pFunc);
    }
};


struct RemoteHost
{
    OVERLAP_EX  olRcv;
    OVERLAP_EX  olSnd;
    SOCKET      scH;
    int         nUse;

    RemoteHost()
    {
        olRcv.pOwn = this;
        olSnd.pOwn = this;
        olRcv.dType= FD_READ;
        olSnd.dType= FD_WRITE;

        scH     = 0;
        nUse    = 1;
    }

    RemoteHost(SOCKET s)
    {
        olRcv.pOwn = this;
        olSnd.pOwn = this;
        olRcv.dType= FD_READ;
        olSnd.dType= FD_WRITE;

        scH     = s;
        nUse    = 1;
    }

    ~RemoteHost()
    {
        //Close();
    }


    void Close()
    {
        if(scH)
        {
            shutdown(scH, SD_BOTH);
            closesocket(scH);
            scH = 0;
        }
    }
};


SOCKET                  g_scLstn = 0;

vector<RemoteHost* >    g_vHost;                            // Client list
WSAEVENT                g_eDmmy[1];                         // dummy event

CRITICAL_SECTION        m_CS;                               // critical section

void EchoMsg(char* s, int l);

DWORD WINAPI WorkThread(void*);                             // work thread
void  CALLBACK  CompletionRoutine(DWORD, DWORD
                            ,LPWSAOVERLAPPED,DWORD);        // completion routine




void DeleteNotUseHost()
{
    EnterCriticalSection(&m_CS);
    vector<RemoteHost* >::iterator _F = g_vHost.begin();

    for( ; _F != g_vHost.end(); )
    {
        RemoteHost* pCln = (*_F);

        if(pCln && 0 >= pCln->nUse)
        {
            pCln->Close();

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
        if(0 >= g_vHost[i]->scH)
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




int main()
{
    InitializeCriticalSection(&m_CS);

    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Server.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(INVALID_SOCKET == g_scLstn)
        return -1;


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


    g_eDmmy[0] = CreateEvent(0,0,0,0);  // auto-reset, dummy event for WSAWait signal


    // create work thread
    HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkThread
                            , nullptr, 0, nullptr);


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



        // Event를 Notify 하기 위해 소켓 번호를 접속한 클라이언트에게 전송
        char sId[128]={0};
        sprintf(sId, "Connected: %d", (int)scCln);
        send(scCln, sId, strlen(sId), 0);


        // Nagle off
        int v = 1;
        hr = setsockopt(scCln, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
        if(SOCKET_ERROR == hr)
        {
            hr = WSAGetLastError();
        }


        // create client intance
        RemoteHost* pCln = FindNotUseHost();

        if(nullptr == pCln)
            pCln = new RemoteHost(scCln);

        OVERLAP_EX* polRcv = &pCln->olRcv;

        // receiving state
        polRcv->Reset();
        hr =polRcv->AsyncRcv(pCln->scH, CompletionRoutine);

        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                break;
            }
        }

        // add the client list
        g_vHost.push_back(pCln);
        WSASetEvent(g_eDmmy[0]);
    }

    CloseHandle(hWork);
    CloseHandle(g_eDmmy[0]);

    DeleteAllHost();
    closesocket(g_scLstn);
    g_scLstn = 0;

    WSACleanup();

    DeleteCriticalSection(&m_CS);

    return 0;
}



DWORD WINAPI WorkThread(void* pParam)
{
    while(1)
    {
        int hr = 0;

        // hr = SleepEx(INFINITE, TRUE);
        hr = WSAWaitForMultipleEvents(1, g_eDmmy, FALSE, INFINITE, TRUE);
        if(WSA_WAIT_IO_COMPLETION == hr)
            continue;

        if(WSA_WAIT_EVENT_0 == hr)
            continue;
    }

    _endthreadex(0);
    return 0;
}


// Completion Routine
void CALLBACK CompletionRoutine(DWORD dErr, DWORD dTran,LPWSAOVERLAPPED pOl, DWORD dFlag)
{
    int hr = 0;

    OVERLAP_EX* pExOl = (OVERLAP_EX*)pOl;
    RemoteHost* pCln   = (RemoteHost*)pExOl->pOwn;
    SOCKET      scHost = pCln->scH;


    // Error or disconnected
    if(dErr != 0 || dTran == 0)
    {
        printf("Disconnect Client: %d\n", (int)scHost);
        pCln->nUse = 0;
        return;
    }

    // sending completion
    if(FD_WRITE == pExOl->dType)
    {
        pExOl->Reset();
        printf("Complete sending[%4d]: %d byte\n", (int)scHost, dTran);
    }

    // receiving completion
    else if(FD_READ == pExOl->dType)
    {
        printf("Recv from Client[%5d]: %2d %s\n", (int)scHost, dTran, pExOl->csBuf);

        // test message
        int  iLen = 0;
        char sSnd[MAX_BUF] ={0};

        sprintf(sSnd, "%5d :", (int)scHost);
        strncat(sSnd, pExOl->csBuf, dTran);
        iLen = strlen(sSnd);

        // send the received message to all client
        EchoMsg(sSnd, iLen);


        // setup the receiving state
        pExOl->Reset();
        hr =pExOl->AsyncRcv(scHost, CompletionRoutine);
        if(SOCKET_ERROR == hr)
        {
            hr =  WSAGetLastError();
            if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
            {
                LogGetLastError(hr);
                pCln->nUse = 0;
            }
        }
    }
}



// echo message
void EchoMsg(char* s, int l)
{
    EnterCriticalSection(&m_CS);

    int hr = 0;
    int iSize = (int)g_vHost.size();

    RemoteHost* pCln    = nullptr;
    OVERLAP_EX* polSnd  = nullptr;
    SOCKET      scHost  = 0;

    for(int i=0; i<iSize; ++i)
    {
        pCln   = g_vHost[i];
        scHost = pCln->scH;
        polSnd = &pCln->olSnd;

        if(0 >= scHost || 0 >= pCln->nUse)
            continue;

        polSnd->Reset();
        polSnd->SetBuf(s, l);

        hr = polSnd->AsyncSnd(scHost, CompletionRoutine);

        if(SOCKET_ERROR == hr)
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

