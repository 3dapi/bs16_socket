//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#pragma comment(lib, "ws2_32.lib")

#include &lt;winsock2.h&gt;
#include &lt;windows.h&gt;
#include &lt;process.h&gt;

#include &lt;stdio.h&gt;
#include &lt;stdlib.h&gt;
#include &lt;string.h&gt;


#define	MAX_BUF		8192


char	sPt[32]="60000";
char	sIp[64]="127.0.0.1";

void LogGetLastError(int hr);



SOCKET					g_scHost=0;						// 소켓
SOCKADDR_IN				g_sdHost={0};					// 소켓 어드레스
HANDLE					g_hThSnd;						// Send용 쓰레드 핸들
CRITICAL_SECTION		m_CS;							// 임계영역

int						g_nBuf=0;						// recorded byte
char					g_sBuf[MAX_BUF]={0};			// send buffer

DWORD WINAPI WorkSnd(void *pParam);						// Send용 쓰레드


// 사용자가 정의한 네트워크 메시지
#define WM_SOCKET_NOTIFY    (WM_USER+1000)

LRESULT	WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);		// Window Message Callback Function
LRESULT        NetProc(HWND, UINT, WPARAM, LPARAM);		// Network Message Procedure

void	CloseSocket();


int main()
{
	// 임계영역 초기화
	InitializeCriticalSection(&m_CS);


	// 윈도우 생성
	char	sCls[128]="AsycSelect Client";
	HINSTANCE hInst	= GetModuleHandle(nullptr);

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



	//데이터 송신용 Thread 생성. 일시 중지 상태
	g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
							, (unsigned (__stdcall*)(void*))WorkSnd
							, nullptr, CREATE_SUSPENDED, nullptr);


	// 네트워크 코드 추가
	WSADATA		wsData={0};
	int			hr =-1;

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


	// AsycSelect에 연결. connect함수 호출 전에 AsycSelect에 연결하면
	// 접속 이벤트를 얻을 수 있음
	hr = WSAAsyncSelect(g_scHost
						, hWnd
						, WM_SOCKET_NOTIFY
						, FD_CONNECT|FD_WRITE|FD_READ|FD_CLOSE);

	hr = connect(g_scHost, (SOCKADDR*)&g_sdHost, sizeof(SOCKADDR_IN));

	// AsyncSelect모델은 Non-blocking 모델로 자동 전환
	// WSAGetLastError() 함수로 에러 판단
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


	// 쓰레드 핸들 해제
	CloseHandle(g_hThSnd);

	shutdown(g_scHost, SD_BOTH);
	closesocket(g_scHost);

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
			EnterCriticalSection(&m_CS);

			static int nTstValue =0;
			++nTstValue;
			sprintf(g_sBuf, "ClientMsg- %4d", nTstValue);

			g_nBuf = strlen(g_sBuf);

			LeaveCriticalSection(&m_CS);

			ResumeThread(g_hThSnd);

			return 0;
		}
	}

	return DefWindowProc( hWnd, msg, wParam, lParam );
}


LRESULT NetProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	SOCKET	scHost	= (SOCKET)wParam;
	DWORD	dError	= WSAGETSELECTERROR(lParam);
	DWORD	dEvent	= WSAGETSELECTEVENT(lParam);

	int		hr = 0;

	// 에러 체크
	if(dError)
	{
		if(FD_CONNECT == dEvent)
			SetWindowText(hWnd, "Connection Failed.");


		CloseSocket();
		SendMessage(hWnd, WM_DESTROY, 0,0);
		SetWindowText(hWnd, "Disconnect\n");
		return 0;
	}


	// connection 메시지
	if(FD_CONNECT == dEvent)
	{
		SetWindowText(hWnd, "Connection succeeded");
		HDC hDC= GetDC(hWnd);
			TextOut(hDC, 10, 10, "Try to R button down.", strlen("Try to L button down."));
		ReleaseDC(hWnd, hDC);
	}

	// Sending 메시지
	else if(FD_WRITE == dEvent)
	{
	}

	// 수신 메시지
	else if(FD_READ == dEvent)
	{
		char sBufRcv[MAX_BUF+4]={0};
		int iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);

		// socket close
		if(0 &gt; iRcv)
		{
			hr = WSAGetLastError();
			if(WSAEWOULDBLOCK != hr)
			{
				CloseSocket();
				SendMessage(hWnd, WM_DESTROY, 0,0);
				SetWindowText(hWnd, "Disconnect\n");
			}
		}
		else if(0 == iRcv)
		{
			CloseSocket();
			SendMessage(hWnd, WM_DESTROY, 0,0);
			SetWindowText(hWnd, "Disconnect\n");
		}
		else
		{
			printf("Recv from Server: %s\n", sBufRcv);
		}
	}

	// 접속 해제 메시지
	else if(FD_CLOSE == dEvent)
	{
		CloseSocket();
		SendMessage(hWnd, WM_DESTROY, 0,0);
		SetWindowText(hWnd, "Disconnect\n");
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


DWORD WINAPI WorkSnd(void *pParam)
{
	int hr = 0;
	int iSnd=0;
	int iTot=0;
	int	iLen=0;

	while(g_scHost)
	{
		EnterCriticalSection(&m_CS);

		hr = 0;
		iSnd=0;
		iTot=0;
		iLen=0;

		iLen = g_nBuf;

		if(0&lt; iLen)
		{
			// 데이터 송신
			while(iTot&lt;iLen)
			{
				iSnd =send(g_scHost, g_sBuf+iTot, iLen-iTot, 0);

				if(0&gt;=iSnd)
				{
					int hr = WSAGetLastError();

					if(WSAEWOULDBLOCK == hr)
						continue;

					printf("Send Socket Error\n");
					LogGetLastError(hr);

					hr = -1;
					break;
				}

				iTot += iSnd;
			}

			g_nBuf = 0;
			memset(g_sBuf, 0, MAX_BUF);
		}

		LeaveCriticalSection(&m_CS);

		// send err
		if(FAILED(hr))
			break;

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

