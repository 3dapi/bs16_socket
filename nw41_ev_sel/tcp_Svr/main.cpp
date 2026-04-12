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
#define MAX_HOST    WSA_MAXIMUM_WAIT_EVENTS

char    sPt[32]="60000";

void LogGetLastError(int hr);



struct RemoteHost
{
    SOCKET      scH{};                      // socket
    SOCKADDR_IN sdH{};                      // address
    WSAEVENT    seH{};                      // Event

    RemoteHost()
    {
    }

    void Set(SOCKET s, SOCKADDR_IN* d , WSAEVENT v)
    {
        scH     = s;
        memcpy(&sdH, d, sizeof(SOCKADDR_IN));
        seH = v;
    }

    void Close()
    {
        if(0==scH)
            return;

        shutdown(scH, SD_BOTH);
        closesocket(scH);
        scH = 0;

        CloseHandle(seH);
        seH  = nullptr;
    }
};


SOCKET          g_scLstn=0;             // listen socket
WSAEVENT        g_seLstn=0;             // event
RemoteHost      g_rmHost[MAX_HOST];     // Host list: listen + client list

void    EchoMsg(char* s,int iLen);      // echo message



RemoteHost* FindHost(SOCKET scH)
{
    for(int i=0; i<MAX_HOST; ++i)
    {
        if(scH == g_rmHost[i].scH)
            return &g_rmHost[i];
    }

    return nullptr;
}


RemoteHost* FindNotUseHost()
{
    for(int i=0; i<MAX_HOST; ++i)
    {
        if(0 >= g_rmHost[i].scH)
            return &g_rmHost[i];
    }

    return nullptr;
}


void DeleteHost(SOCKET scH)
{
    if(0>=scH)
        return;

    for(int i=0; i<MAX_HOST; ++i)
    {
        if(scH == g_rmHost[i].scH)
        {
            g_rmHost[i].Close();
            return;
        }
    }
}

void DeleteAllHost()
{
    for(int i=0; i<MAX_HOST; ++i)
    {
        if(0 < g_rmHost[i].scH)
            g_rmHost[i].Close();
    }
}



int main()
{
    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Server.\nPort: %s\n", sPt);


    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_scLstn)
        return -1;

    SOCKADDR_IN sdLstn={0};         // address

    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons( atoi(sPt) );

    hr = bind(g_scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));

    if(SOCKET_ERROR == hr)
        return -1;


    // 이벤트 객체 생성
    g_seLstn = WSACreateEvent();


    // <소켓, 이벤트 객체, 비동기 통지 이벤트 종류(accept, close)> 바인딩.
    // 소켓은 자동으로 Non-blocking으로 전환
    hr = WSAEventSelect(g_scLstn, g_seLstn, FD_ACCEPT|FD_CLOSE);

    // Listen 소켓과 이벤트 등을 호스트 리스트 0번에 추가.
    g_rmHost[0].Set(g_scLstn, &sdLstn, g_seLstn);

    // Listen 상태
    hr = listen(g_scLstn, SOMAXCONN);
    if(SOCKET_ERROR ==hr)
        return -1;


    while(1)
    {
        RemoteHost*     pHost = nullptr;

        WSANETWORKEVENTS wnE={0};
        WSAEVENT        vEvn[MAX_HOST]={0};     // Event List
        SOCKET          vSck[MAX_HOST]={0};     // Socket List

        int             hr = 0;
        int             i  = 0, nLst=0;
        int             nE=-1;


        // listen 포함, 클라이언트의 모든 이벤트를 리스트에 설정
        // listen 소켓 포함, 클라이언트의 모든 소켓을 리스트에 설정

        for(i=0; i<MAX_HOST; ++i)
        {
            if(0 == g_rmHost[i].scH)
                continue;

            vEvn[nLst] = g_rmHost[i].seH;
            vSck[nLst] = g_rmHost[i].scH;
            ++nLst;
        }


        // Accept 포함한 네트워크 이벤트를 기다린다.
        //nE = WaitForMultipleObjects(nLst, vEvn, FALSE, WSA_INFINITE);
        nE = WSAWaitForMultipleEvents(nLst, vEvn, FALSE, WSA_INFINITE, FALSE);

        if(nE == WSA_WAIT_FAILED)
        {
            hr = WSAGetLastError();
            LogGetLastError(hr);
            break;
        }

        nE -= WSA_WAIT_EVENT_0;     // 인덱스 재조정

        for(i= nE; i<nLst; ++i)
        {
            hr = WSAWaitForMultipleEvents(1, &vEvn[i], TRUE, 0, FALSE);

            if( WSA_WAIT_FAILED  == hr || WSA_WAIT_TIMEOUT == hr)
                continue;


            // 이벤트 분해
            hr = WSAEnumNetworkEvents(vSck[i], vEvn[i], &wnE);

            // get the host
            SOCKET scHost = vSck[i];
            pHost = FindHost(scHost);


            // WSAEnumNetworkEvents() 함수의 에러 반환 체크
            if(SOCKET_ERROR == hr)
            {
                hr= WSAGetLastError();
                printf("WSAEnumNetworkEvents Error\n");


                // listen socket
                if(scHost == g_scLstn)
                {
                    printf("Listen socket or Network Error\n");
                    goto END;
                }

                pHost->Close();
                printf("Client Socket Close: %d\n", (int)scHost);

                continue;
            }


            ////////////////////////////////////////////////////////////////////
            // Accept event
            if( FD_ACCEPT & wnE.lNetworkEvents)
            {
                SOCKET      scLstn = vSck[i];
                SOCKET      scCln;
                SOCKADDR_IN sdCln;
                WSAEVENT    seCln;

                int iSize = sizeof(SOCKADDR_IN);

                // accept()함수로 클라이언트 소켓, 주소 얻기
                scCln = accept(scLstn, (SOCKADDR*)&sdCln, &iSize);

                // 이벤트 객체 생성
                seCln = WSACreateEvent();

                // 접속한 클라이언트도 비동기로 통지 받을 수 있도록
                // <소켓, 이벤트 객체, 비동기 통지 이벤트 종류> 바인딩
                WSAEventSelect(scCln, seCln, (FD_READ|FD_WRITE|FD_CLOSE));

                // 사용하지 않는 Host list에 추가
                pHost = FindNotUseHost();

                if(nullptr == pHost)
                {
                    printf("There is no empty element for client.\n");

                    // disconnect the client
                    shutdown(scCln, SD_BOTH);
                    closesocket(scCln);
                }
                else
                    pHost->Set(scCln, &sdCln, seCln);


                continue;
            }


            ////////////////////////////////////////////////////////////////////
            // I/O event

            // Sending event
            if( FD_WRITE & wnE.lNetworkEvents)
            {
            }

            // closing event
            else if( FD_CLOSE & wnE.lNetworkEvents)
            {
                pHost->Close();
                printf("Client Close: %d\n", (int)scHost);
            }

            // receive event
            else if( FD_READ & wnE.lNetworkEvents)
            {
                int     iRcv = 0;
                char    bufRcv[MAX_BUF+4]={0};

                // 데이터 수신
                iRcv=recv(vSck[i], bufRcv, MAX_BUF, 0);

                if(0 > iRcv)
                {
                    hr = WSAGetLastError();

                    if(WSAEWOULDBLOCK != hr)
                    {
                        pHost->Close();
                        printf("Client Close: %d\n", (int)scHost);
                        LogGetLastError(hr);

                        continue;
                    }
                }
                else if(0 == iRcv)
                {
                    pHost->Close();
                    printf("Client Close: %d\n", (int)scHost);
                }
                else
                {
                    printf("Recv from Client : %d %s\n", (int)scHost, bufRcv);

                    char    bufSnd[MAX_BUF+4]={0};
                    int     iLen=0;

                    sprintf(bufSnd, "%5d> %s", (int)scHost, bufRcv);
                    iLen = strlen(bufSnd);

                    EchoMsg(bufSnd, iLen);      // echo message
                }
            }// else if FD_READ
        }// for
    }// while

END:
    DeleteAllHost();
    WSACleanup();

    return 0;
}



void EchoMsg(char* s, int iLen)
{
    int     iSnd= 0;
    int     iTot= 0;

    // 받은 데이터를 접속한 모든 클라이언트에 전송
    // listen socket 제외. 1부터 시작
    for(int i=1; i<MAX_HOST; ++i)
    {
        RemoteHost* pCln = &g_rmHost[i];

        if(0 == pCln->scH)
            continue;

        iSnd= 0;
        iTot= 0;

        while(iTot<iLen)
        {
            char* p = s + iTot;

            iSnd = send(pCln->scH, p, iLen-iTot, 0);

            if(SOCKET_ERROR == iSnd)
            {
                iSnd = WSAGetLastError();
                if(WSAEWOULDBLOCK == iSnd)
                    continue;

                // 전송 error. client를 list에서 제거
                pCln->Close();
                break;
            }

            iTot += iSnd;
        }
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

