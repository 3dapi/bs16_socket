//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "WS2_32.LIB")

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char    sPt[32]="60000";

#define MAX_BUF     8192
#define MAX_CLIENT  128

void LogGetLastError(int hr);


struct RemoteHost
{
    SOCKET      scH{};                      // socket
    SOCKADDR_IN sdH{};                      // address
    int         nBuf{};                     // recorded byte
    char        sBuf[MAX_BUF]{};            // Send buffer

    HANDLE      hThRcv{};                   // Receive용 쓰레드 핸들
    HANDLE      hThSnd{};                   // Send용 쓰레드 핸들

    RemoteHost()
    {
    }

    RemoteHost(SOCKET s, SOCKADDR_IN* d)
    {
        scH     = s;
        memcpy(&sdH, d, sizeof(SOCKADDR_IN));

        nBuf = 0;
        memset(sBuf, 0 , MAX_BUF);

        hThRcv= 0;
        hThSnd= 0;
    }

    void Set(SOCKET s, SOCKADDR_IN* d, HANDLE hRcv, HANDLE hSnd)
    {
        scH     = s;
        memcpy(&sdH, d, sizeof(SOCKADDR_IN));

        nBuf = 0;
        memset(sBuf, 0 , MAX_BUF);

        hThRcv= hRcv;
        hThSnd= hSnd;
    }

    void Reset()
    {
        if(scH)
        {
            shutdown(scH, SD_BOTH);
            closesocket(scH);
            scH = 0;
        }

        if(nBuf)
        {
            nBuf = 0;
            memset(sBuf, 0 , MAX_BUF);
        }

        DWORD   dExit=0;

        // Receive Thread 강제 종료
        if(hThRcv)
        {
            GetExitCodeThread(hThRcv, &dExit);
            if(dExit)
                TerminateThread(hThRcv, dExit);

            ::CloseHandle(hThRcv);
            hThRcv = 0;
        }

        // Send Thread 강제 종료
        if(hThSnd)
        {
            GetExitCodeThread(hThSnd, &dExit);
            if(dExit)
                TerminateThread(hThSnd, dExit);

            ::CloseHandle(hThSnd);
            hThSnd = 0;
        }
    }
};


SOCKET              g_scLstn=0;             // listen socket
SOCKADDR_IN         g_sdLstn={0};
RemoteHost          g_rmCln[MAX_CLIENT];    // client list

HANDLE              g_hThAcc;               // accept thread handle
CRITICAL_SECTION    m_CS;


DWORD WINAPI    WorkAcc(void*);             // Accept용 쓰레드
DWORD WINAPI    WorkRcv(void*);             // Receive용 쓰레드
DWORD WINAPI    WorkSnd(void*);             // Send용 쓰레드
void            EchoMsg(char* s, int len);


RemoteHost* FindHost(SOCKET scH)
{
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        if(scH == g_rmCln[i].scH)
            return &g_rmCln[i];
    }

    return nullptr;
}

RemoteHost* FindNotUseHost()
{
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        if(0 >= g_rmCln[i].scH)
            return &g_rmCln[i];
    }

    return nullptr;
}

void DeleteHost(SOCKET scH)
{
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        if(scH == g_rmCln[i].scH)
        {
            g_rmCln[i].Reset();
            return;
        }
    }
}

void DeleteAllHost()
{
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        if(0 < g_rmCln[i].scH)
            g_rmCln[i].Reset();
    }
}



int main()
{
    InitializeCriticalSection(&m_CS);

    WSADATA     wsData;
    int         hr=-1;

    printf("Starting Server.\nPort: %s\n", sPt);

    if(WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
        return -1;


    g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_scLstn)
        return -1;


    memset(&g_sdLstn, 0, sizeof(g_sdLstn));
    g_sdLstn.sin_family      = AF_INET;
    g_sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    g_sdLstn.sin_port        = htons( atoi(sPt) );

    hr = bind(g_scLstn, (SOCKADDR*)&g_sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;


    hr = listen(g_scLstn, SOMAXCONN);
    if( SOCKET_ERROR == hr)
        return -1;


    //Accept Thread
    g_hThAcc = (HANDLE)_beginthreadex(nullptr, 0
                    , (unsigned (__stdcall*)(void*))WorkAcc
                    , nullptr, 0, nullptr);

    WaitForSingleObject(g_hThAcc, INFINITE);
    CloseHandle(g_hThAcc);

    DeleteAllHost();

    shutdown(g_scLstn, SD_BOTH);
    closesocket(g_scLstn);

    WSACleanup();

    DeleteCriticalSection(&m_CS);

    return 0;
}


void EchoMsg(char* s, int len)
{
    EnterCriticalSection(&m_CS);

    RemoteHost* pCln;

    for(int i=0; i<MAX_CLIENT; ++i)
    {
        pCln = &g_rmCln[i];
        if(0 >= pCln->scH)
            continue;

        memset(pCln->sBuf, 0, sizeof(pCln->sBuf));
        strcpy(pCln->sBuf, s);

        pCln->nBuf = len;
    }

    LeaveCriticalSection(&m_CS);
}



DWORD WINAPI WorkAcc(void *pParam)
{
    while(1)
    {
        SOCKET      scH = 0;
        SOCKADDR_IN sdH ={0};
        int         iSize=sizeof(SOCKADDR_IN);

        // 클라이언트 접속을 기다린다.
        scH = accept(g_scLstn, (SOCKADDR*)&sdH, &iSize);
        if(INVALID_SOCKET == scH)
            continue;


        RemoteHost* pCln = FindNotUseHost();

        // 수용할 클라이언트 공간 없음
        if(nullptr == pCln)
        {
            shutdown(scH, SD_BOTH);
            closesocket(scH);
            continue;
        }

        //데이터 수신용 Thread생성
        HANDLE hRcv = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkRcv
                            , (void*)pCln, CREATE_SUSPENDED , nullptr);

        //데이터 송신용 Thread생성
        HANDLE hSnd = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkSnd
                            , (void*)pCln, CREATE_SUSPENDED , nullptr);

        // setup the client
        pCln->Set(scH, &sdH, hRcv, hSnd);

        ResumeThread(hRcv);
        ResumeThread(hSnd);
    }

    _endthreadex(0);
    return 0;
}





DWORD WINAPI WorkRcv(void *pParam)
{
    RemoteHost* pCln = (RemoteHost*)pParam;
    int     hr = 0;

    while(0<pCln->scH)
    {
        int iRcv = 0;
        char sBufRcv[MAX_BUF+4]={0};

        iRcv=recv(pCln->scH, sBufRcv, MAX_BUF, 0);
        if(0 > iRcv)
        {
            hr = WSAGetLastError();
            LogGetLastError(hr);
            printf("DisConnect: %d\n", (int)pCln->scH);
            break;
        }
        else if(0 == iRcv)
        {
            printf("DisConnect: %d\n", (int)pCln->scH);
            break;
        }

        printf("Recv from Client : %d %s\n", (int)pCln->scH, sBufRcv);

        int     iLen;
        char    sSndBuf[MAX_BUF]={0};
        
        sprintf(sSndBuf, "%5d : %s", pCln->scH, sBufRcv);
        iLen = strlen(sSndBuf);

        EchoMsg(sSndBuf, iLen);
    }

    pCln->Reset();
    _endthreadex(0);
    return 0;
}


DWORD WINAPI WorkSnd(void *pParam)
{
    RemoteHost* pCln = (RemoteHost*)pParam;
    int     hr = 0;

    while(0<pCln->scH)
    {
        hr = 0;
        if(1 > pCln->nBuf)
            continue;

        int iSnd = 0;
        int iTot = 0;
        while(iTot<pCln->nBuf)
        {
            iSnd=send(pCln->scH, pCln->sBuf+iTot, pCln->nBuf - iTot, 0);

            if(SOCKET_ERROR == iSnd)
            {
                hr = WSAGetLastError();
                LogGetLastError(hr);
                printf("Send Error::DisConnect: %d\n", (int)pCln->scH);

                goto END;
            }

            iTot += iSnd;
        }


        EnterCriticalSection(&m_CS);
        pCln->nBuf = 0;
        memset(pCln->sBuf, 0, sizeof(pCln->sBuf));
        LeaveCriticalSection(&m_CS);
    }

END:
    pCln->Reset();

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

