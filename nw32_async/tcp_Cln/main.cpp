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
char    sIp[64]="127.0.0.1";

SOCKET      g_scHost=0;                                 // 소켓
SOCKADDR_IN g_sdHost={0};                               // 소켓 어드레스


// 사용자가 정의한 네트워크 메시지
#define WM_SOCKET_NOTIFY    (WM_USER+1000)


LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);     // Window Message Callback Function
LRESULT        NetProc(HWND, UINT, WPARAM, LPARAM);     // Network Message Procedure

void    CloseSocket();


int main()
{
    // 윈도우 생성
    char    sCls[128]="AsycSelect Client";
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





    // 네트워크 코드를 추가
    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Client.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_scHost)
        return -1;


    memset(&g_sdHost, 0, sizeof(g_sdHost));
    g_sdHost.sin_family      = AF_INET;
    g_sdHost.sin_addr.s_addr = inet_addr(sIp);
    g_sdHost.sin_port        = htons( atoi(sPt) );

    // AsycSelect에 연결. connect함수 호출 전에 AsycSelect에
    // 연결하면 접속 이벤트를 얻을 수 있음
    hr = WSAAsyncSelect(g_scHost, hWnd, WM_SOCKET_NOTIFY
                        , FD_CONNECT|FD_WRITE|FD_READ|FD_CLOSE);

    hr = connect(g_scHost, (SOCKADDR*)&g_sdHost, sizeof(SOCKADDR_IN));

    // AsyncSelect모델은 Non-blocking 모델로 자동 전환
    // 처리 중이면 에러로 반환. WSAGetLastError() 함수로 에러 판단
    if(SOCKET_ERROR ==hr)
    {
        Sleep(10);
        hr = WSAGetLastError();

        if(WSAEWOULDBLOCK !=hr)
            return -1;
    }



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

    CloseSocket();

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

        case WM_RBUTTONDOWN:
        {
            // 테스트용 패킷
            char        bufSnd[MAX_BUF]={0};
            static int  nTstValue=0;

            ++nTstValue;
            sprintf(bufSnd, "ClientMsg- %4d", nTstValue);

            int iSnd=0;
            int iTot=0;
            int iLen = strlen(bufSnd);

            // 패킷이 다 전송이 안될 수 있으므로 전부 보낼 때 까지 While로 전송
            while(iTot<iLen)
            {
                iSnd =send(g_scHost, bufSnd+iTot, iLen-iTot, 0);
                iTot += iSnd;
            }

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

    // 에러 체크
    if(dError)
    {
        if(FD_CONNECT == dEvent)
            SetWindowText(hWnd, "Connection Failed.");

        SetWindowText(hWnd, "Network Closed");
        SendMessage(hWnd, WM_DESTROY, 0,0);
        return 0;
    }

    // connection 메시지
    if(FD_CONNECT == dEvent)
    {
        SetWindowText(hWnd, "Connection Successed");
        HDC hDC= GetDC(hWnd);
            TextOut(hDC, 10, 10, "Try to R button down.", strlen("Try to L button down."));
        ReleaseDC(hWnd, hDC);

        return 0;
    }


    // 송신 메시지
    if(FD_WRITE == dEvent)
    {
        return 0;
    }

    // 수신 메시지
    if(FD_READ == dEvent)
    {
        char sBufRcv[MAX_BUF+4]={0};
        int iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);

        // socket close
        if(SOCKET_ERROR == iRcv)
        {
            hr = WSAGetLastError();
            if(WSAEWOULDBLOCK != hr)
            {
                SetWindowText(hWnd, "Network Closed");
                SendMessage(hWnd, WM_DESTROY, 0,0);
            }
        }

        else if(0 == iRcv)
        {
            SetWindowText(hWnd, "Network Closed");
            SendMessage(hWnd, WM_DESTROY, 0,0);
        }

        if(0<iRcv)
        {
            sBufRcv[iRcv]=0;
            printf("Recv from server : %s\n", sBufRcv);
        }
    }

    // 접속 해제 메시지
    else if(FD_CLOSE == dEvent)
    {
        SetWindowText(hWnd, "Network Closed");
        SendMessage(hWnd, WM_DESTROY, 0,0);
    }

    return 0;
}



void CloseSocket()
{
    if(0 == g_scHost)
        return;

    shutdown(g_scHost, SD_BOTH);
    closesocket(g_scHost);

    g_scHost = 0;
}

