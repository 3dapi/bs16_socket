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
#define MAX_CLIENT  64


char    sPt[32]="60000";


struct RemoteHost
{
    SOCKET      scH{};                      // socket
    SOCKADDR_IN sdH{};                      // address

    RemoteHost()
    {
    }

    void Set(SOCKET s, SOCKADDR_IN* d)
    {
        scH = s;
        memcpy(&sdH, &d, sizeof(SOCKADDR_IN) );
    }

    void Close()
    {
        if(scH)
        {
            shutdown(scH, SD_BOTH);
            closesocket(scH);
        }

        scH     = 0;
        memset(&sdH, 0, sizeof(SOCKADDR_IN));
    }
};


SOCKET              g_scLstn;                           // Listen socket
RemoteHost          g_rmCln[MAX_CLIENT];

// 사용자가 정의한 네트워크 메시지
#define WM_SOCKET_NOTIFY    (WM_USER+1000)

LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);     // Window Message Callback Function
LRESULT        NetProc(HWND, UINT, WPARAM, LPARAM);     // Network Message Procedure

void        EchoMsg(char* s, int len);

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
            g_rmCln[i].Close();
            return;
        }
    }
}

void DeleteAllHost()
{
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        if(0 < g_rmCln[i].scH)
            g_rmCln[i].Close();
    }
}



int main()
{
    // 윈도우 생성
    char    sCls[128]="AsycSelect Server";
    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASS wc = {0};
    wc.style        = CS_CLASSDC;
    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInst;
    wc.hCursor      = LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground= (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName= sCls;

    RegisterClass( &wc );

    HWND hWnd = CreateWindow( sCls
        , sCls
        , WS_OVERLAPPEDWINDOW| WS_VISIBLE
        , CW_USEDEFAULT, CW_USEDEFAULT
        , 480, 320
        , nullptr, nullptr
        , hInst, nullptr );

    ShowWindow( hWnd, SW_SHOW );
    UpdateWindow( hWnd );




    // 윈도우를 다 만들고 나서 네트워크 코드를 추가
    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Server.\nPort: %s\n", sPt);


    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(INVALID_SOCKET == g_scLstn)
        return -1;


    SOCKADDR_IN sdLstn ={0};
    sdLstn.sin_family      = AF_INET;
    sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
    sdLstn.sin_port        = htons( atoi(sPt) );
    hr = bind(g_scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
    if(SOCKET_ERROR == hr)
        return -1;


    // AsycSelect에 연결. 소켓은 자동으로 Non-blocking으로 전환
    hr = WSAAsyncSelect(g_scLstn, hWnd, WM_SOCKET_NOTIFY, FD_ACCEPT|FD_CLOSE);


    // Listen 상태
    hr = listen(g_scLstn, SOMAXCONN);
    if(SOCKET_ERROR ==hr)
        return -1;


    // 윈도우 메시지 처리
    MSG msg={0};
    while( WM_QUIT != msg.message )
    {
        if(GetMessage( &msg, nullptr, 0U, 0U))
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    UnregisterClass( sCls, hInst);

    DeleteAllHost();

    shutdown(g_scLstn, SD_BOTH);
    closesocket(g_scLstn);

    WSACleanup();

    return 0;
}



LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_KEYDOWN:
        {
            switch(wParam)
            {
                case VK_ESCAPE:
                {
                    SendMessage(hWnd, WM_DESTROY, 0,0);
                    break;
                }
            }

            return 0;
        }

        case WM_CLOSE:
        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
            return 0;
        }

        // 네트워크 메시지 처리
        case WM_SOCKET_NOTIFY:
        {
            NetProc(hWnd, msg, wParam, lParam);
            return 0;
        }
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}



LRESULT NetProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    SOCKET  scHost  = (SOCKET)wParam;
    DWORD   dError  = WSAGETSELECTERROR(lParam);
    DWORD   dEvent  = WSAGETSELECTEVENT(lParam);

    int     hr = 0;

    RemoteHost* pCln = nullptr;

    // 에러 체크
    if(dError)
    {
        if(scHost == g_scLstn)
        {
            SetWindowText(hWnd, "Listen Error");
            SendMessage(hWnd, WM_DESTROY, 0,0);
        }
        else
        {
            DeleteHost(scHost);
            printf("Disconnect Client: %d\n", scHost);
        }

        return 0;
    }


    // Accept 메시지
    if(FD_ACCEPT == dEvent)
    {
        SOCKET  scLstn  = scHost;

        SOCKET      scCln;
        SOCKADDR_IN sdCln;

        int iSize=sizeof(SOCKADDR_IN);

        // accept() 함수로 접속한 클라이언트 소켓을 얻는다.
        scCln = accept(scLstn, (SOCKADDR*)&sdCln, &iSize);

        // 접속한 클라이언트도 메시지로 처리할 수 있도록 AsyncSelect로 설정
        WSAAsyncSelect(scCln, hWnd, WM_SOCKET_NOTIFY, FD_READ|FD_WRITE|FD_CLOSE);


        // 배열에서 사용하지 않은 클라이언트 찾기
        // 클라이언트 list에 추가
        pCln = FindNotUseHost();
        pCln->Set(scCln, &sdCln);

        return 0;
    }



    // 배열에 있는 클라이언트 주소를 가져옴
    pCln = FindHost(scHost);

    // 클라이언트 리스트의 소켓에서 발생한 메시지가 아님
    if(nullptr == pCln)
        return 0;


    // 접속 해제 메시지
    if(FD_CLOSE == dEvent)
    {
        // client를 list에서 제거
        pCln->Close();
        printf("Disconnect Client: %d\n", scHost);
    }

    // Sending 메시지
    else if(FD_WRITE == dEvent)
    {
    }

    // 수신 메시지
    else if(FD_READ == dEvent)
    {
        int     iRcv=0;
        char    sBufRcv[MAX_BUF+4]={0};

        // 수신 버퍼에서 데이터 꺼내오기
        iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);

        // socket close
        if(SOCKET_ERROR == iRcv)
        {
            hr = WSAGetLastError();
            if(WSAEWOULDBLOCK != hr)
            {
                // client를 list에서 제거
                pCln->Close();
                printf("Disconnect Client: %d\n", scHost);
            }
        }
        else if(0 == iRcv)
        {
            // client를 list에서 제거
            pCln->Close();
            printf("Disconnect Client: %d\n", scHost);
        }
        else
        {
            printf("Recv from Client : %d %s\n", scHost, sBufRcv);

            // test message
            char    sSndBuf[MAX_BUF]={0};
            sprintf(sSndBuf, "%5d : %s", scHost, sBufRcv);

            // send the message to all client
            EchoMsg(sSndBuf, strlen(sSndBuf) );
        }
    }

    return 0;
}


void EchoMsg(char* s, int iLen)
{
    int     iSnd= 0;
    int     iTot= 0;

    // 받은 데이터를 접속한 모든 클라이언트에 전송
    for(int i=0; i<MAX_CLIENT; ++i)
    {
        RemoteHost* pClnT = &g_rmCln[i];

        if(0 == pClnT->scH)
            continue;

        iSnd= 0;
        iTot= 0;

        while(iTot<iLen)
        {
            char* p = s + iTot;

            iSnd = send(pClnT->scH, p, iLen-iTot, 0);
            if(SOCKET_ERROR == iSnd)
            {
                iSnd = WSAGetLastError();
                if(WSAEWOULDBLOCK == iSnd)
                    continue;

                // 전송 error. client를 list에서 제거
                pClnT->Close();
                break;
            }

            iTot += iSnd;
        }
    }
}

