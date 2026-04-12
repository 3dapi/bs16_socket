//
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


struct RemoteHost
{
    SOCKET      scH{};                      // socket
    SOCKADDR_IN sdH{};                      // address
    int         nBuf{};                     // recorded byte
    char        sBuf[MAX_BUF]{};            // Send buffer

    RemoteHost()
    {
    }

    RemoteHost(SOCKET s, SOCKADDR_IN* d)
    {
        scH = s;
        memcpy(&sdH, &d, sizeof(SOCKADDR_IN) );

        nBuf    = 0;
        memset(sBuf, 0, MAX_BUF);
    }

    ~RemoteHost()
    {
        Close();
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

        nBuf    = 0;
        memset(sBuf, 0, MAX_BUF);
    }

    void SetupBuf(char* s, int len)
    {
        nBuf = len;
        memcpy(sBuf, s, nBuf);
        memset(sBuf+nBuf, 0, MAX_BUF-len);
    }
};


RemoteHost              g_rmLstn;                       // For Listen
vector<RemoteHost* >    g_rmCln;                        // client list

HANDLE                  g_hThSnd = nullptr;             // Send용 쓰레드 핸들
CRITICAL_SECTION        m_CS;                           // 임계영역: 동기화에 필요


DWORD WINAPI WorkSnd(void *pParam);                     // Send용 쓰레드


// 사용자가 정의한 네트워크 메시지
#define WM_SOCKET_NOTIFY    (WM_USER+1000)

LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);     // Window Message Callback Function
LRESULT        NetProc(HWND, UINT, WPARAM, LPARAM);     // Network Message Procedure

void        EchoMsg(char* s, int len);

RemoteHost* FindHost(SOCKET scH)
{
    int iSize = (int)g_rmCln.size();

    for(int i=0; i<iSize; ++i)
    {
        if(scH == g_rmCln[i]->scH)
            return g_rmCln[i];
    }

    return nullptr;
}

void DeleteHost(SOCKET scH)
{
    int iSize = (int)g_rmCln.size();

    for(int i=0; i<iSize; ++i)
    {
        if(scH == g_rmCln[i]->scH)
        {
            delete g_rmCln[i];
            g_rmCln.erase( g_rmCln.begin() + i);
            return;
        }
    }

}

void DeleteNotUseHost()
{
    vector<RemoteHost* >::iterator _F = g_rmCln.begin();

    for( ; _F != g_rmCln.end(); )
    {
        if(0 >= (*_F)->scH)
        {
            delete (*_F);
            _F = g_rmCln.erase(_F);
            continue;
        }

        ++_F;
    }
}


void DeleteAllHost()
{
    for(auto host : g_rmCln)
        delete host;

    g_rmCln.clear();
}



int main()
{
    // 임계영역 초기화
    InitializeCriticalSection(&m_CS);


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



    //데이터 송신용 Thread 생성. 대기 상태
    g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkSnd
                            , nullptr, CREATE_SUSPENDED, nullptr);


    // 네트워크 코드 추가
    WSADATA     wsData{};
    int         hr =-1;

    printf("Starting Server.\nPort: %s\n", sPt);

    if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
        return -1;


    g_rmLstn.scH = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(INVALID_SOCKET == g_rmLstn.scH)
        return -1;


    memset(&g_rmLstn.sdH, 0, sizeof(g_rmLstn.sdH));
    g_rmLstn.sdH.sin_family      = AF_INET;
    g_rmLstn.sdH.sin_addr.s_addr = htonl(INADDR_ANY);
    g_rmLstn.sdH.sin_port        = htons( atoi(sPt) );

    hr = bind(g_rmLstn.scH, (SOCKADDR*)&g_rmLstn.sdH, sizeof(SOCKADDR_IN));

    if(SOCKET_ERROR == hr)
        return -1;


    // AsycSelect에 연결. 소켓은 자동으로 Non-blocking으로 전환
    hr = WSAAsyncSelect(g_rmLstn.scH
                        , hWnd
                        , WM_SOCKET_NOTIFY
                        , FD_ACCEPT|FD_CLOSE);


    // Listen 상태
    hr = listen(g_rmLstn.scH, SOMAXCONN);
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


    // 쓰레드 핸들 해제
    CloseHandle(g_hThSnd);

    DeleteAllHost();
    g_rmLstn.Close();

    WSACleanup();

    DeleteCriticalSection(&m_CS);

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
            break;
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
        if(scHost == g_rmLstn.scH)
        {
            g_rmLstn.Close();

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

        // accept()함수로 접속한 클라이언트를 찾는다.
        scCln = accept(scLstn, (SOCKADDR*)&sdCln, &iSize);

        // 접속한 클라이언트도 메시지 풀에서 처리할 수 있도록 AsyncSelect로 설정
        WSAAsyncSelect(scCln
                        , hWnd
                        , WM_SOCKET_NOTIFY
                        , FD_READ|FD_WRITE|FD_CLOSE);

        g_rmCln.push_back(new RemoteHost(scCln, &sdCln) );

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
        DeleteHost(scHost);
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
        if(0 > iRcv)
        {
            hr = WSAGetLastError();
            if(WSAEWOULDBLOCK != hr)
            {
                // client를 list에서 제거
                DeleteHost(scHost);
                printf("Disconnect Client: %d\n", scHost);
            }
        }
        else if(0 == iRcv)
        {
            // client를 list에서 제거
            DeleteHost(scHost);
            printf("Disconnect Client: %d\n", scHost);
        }
        else
        {
            printf("Recv from Client : %d %s\n", scHost, sBufRcv);

            // test message
            char    sBufBuf[MAX_BUF]={0};
            sprintf(sBufBuf, "%5d : %s", scHost, sBufRcv);

            // send the message to all client
            EchoMsg(sBufBuf, strlen(sBufBuf) );
        }
    }

    return 0;
}


void EchoMsg(char* s, int iLen)
{
    EnterCriticalSection(&m_CS);

    int iSize = (int)g_rmCln.size();

    for(int i=0; i<iSize; ++i)
    {
        RemoteHost* pCln = g_rmCln[i];

        pCln->SetupBuf(s, iLen);
    }

    LeaveCriticalSection(&m_CS);

    ResumeThread(g_hThSnd);
}


DWORD WINAPI WorkSnd(void *pParam)
{
    while(1)
    {
        EnterCriticalSection(&m_CS);

        int iSize = (int)g_rmCln.size();

        for(int i=0; i<iSize; ++i)
        {
            RemoteHost* pCln = g_rmCln[i];

            int iLen = pCln->nBuf;
            int iSnd = 0;
            int iTot = 0;
            int hr   = 0;

            if(0 == iLen)
                continue;


            while(iTot<iLen)
            {
                char* p = pCln->sBuf + iTot;

                iSnd = send(pCln->scH, p, iLen-iTot, 0);

                if(SOCKET_ERROR == iSnd)
                {
                    iSnd = WSAGetLastError();
                    if(WSAEWOULDBLOCK == iSnd)
                        continue;

                    // 전송 error. client를 list에서 제거
                    pCln->Close();
                    LogGetLastError(hr);
                    break;
                }

                iTot += iSnd;
            }

            pCln->nBuf = 0;
        }

        // remove send error socketlist
        DeleteNotUseHost();

        LeaveCriticalSection(&m_CS);

        HANDLE hThread = GetCurrentThread();
        SuspendThread(hThread);
    }

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

