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


#define	MAX_BUF		8192

char	sPt[32]="60000";
char	sIp[64]="127.0.0.1";

void LogGetLastError(int hr);


struct OVERLAP_EX : public WSAOVERLAPPED
{
	DWORD		dTran;					// Transfered
	DWORD		dFlag;					// Flag
	WSABUF		wsBuf;
	char		csBuf[MAX_BUF+4];

	OVERLAP_EX()
	{
		memset(this, 0, sizeof(OVERLAP_EX) );
		wsBuf.len = MAX_BUF;
		wsBuf.buf = csBuf;
	}

	void SetEvent()	{	WSASetEvent(hEvent);	}
	void ResetEvent(){	WSAResetEvent(hEvent);	}

	void Close()
	{
		WSAEVENT e = this->hEvent;
		memset(this, 0, sizeof(OVERLAP_EX) );

		this->hEvent = e;
		wsBuf.len = MAX_BUF;
		wsBuf.buf = csBuf;
	}
};


enum { OVL_CONNECT=0, OVL_SEND, OVL_RECV, OVL_TOT, };


OVERLAP_EX		g_ol[OVL_TOT];				// Overlaps
SOCKET			g_scHost=0	;				// 소켓


DWORD WINAPI WorkThread(void*);				// Work 쓰레드

int		AsyncSnd(char* csBuf, int iLen);
int		AsyncRcv();



int main()
{
	WSADATA		wsData={0};
	int			hr =-1;

	printf("Starting Client.\nPort: %s\n", sPt);

	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
		return -1;


	g_scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == g_scHost)
		return -1;


	g_ol[OVL_CONNECT].hEvent = WSACreateEvent();
	g_ol[OVL_SEND   ].hEvent = WSACreateEvent();
	g_ol[OVL_RECV   ].hEvent = WSACreateEvent();


	// Work 쓰레드 생성
	HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkThread
						, nullptr, 0, nullptr);


	// Connection
	SOCKADDR_IN	sdHost={0};
	sdHost.sin_family      = AF_INET;
	sdHost.sin_addr.s_addr = inet_addr(sIp);
	sdHost.sin_port        = htons( atoi(sPt) );

	hr = connect(g_scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR ==hr)
	{
		Sleep(10);
		hr = WSAGetLastError();

		if(WSAEWOULDBLOCK !=hr)
			return -1;
	}

	hr = AsyncRcv();					// 비동기 수신 요청
	if(-1 == hr)
		return -1;


	while(g_scHost)						// 채팅 process
	{
		int		hr = 0;
		int		iLen=0;
		int		iTot=0;					// Total Sending Data

		char	sSnd[MAX_BUF]={0};		// Send용 버퍼

		fgets(sSnd, MAX_BUF, stdin);
		iLen = strlen(sSnd);

		if(1 >iLen)
			continue;


		if('\n' == sSnd[iLen-1])
		{
			sSnd[iLen-1] =0;
			--iLen;
		}

		AsyncSnd(sSnd, iLen);			// 비 동기 데이터 송신
	}


	WaitForSingleObject(hWork, INFINITE);
	CloseHandle(hWork);


	shutdown(g_scHost, SD_BOTH);
	closesocket(g_scHost);

	WSACleanup();

	return 0;
}




DWORD WINAPI WorkThread(void* pParam)
{
	int hr  =0;
	int		nE, i;

	DWORD		dFlag=0;
	DWORD		dTran= 0;
	WSAEVENT	vEvn[OVL_TOT]={0};		// Event List

	while(1)
	{
		nE = 0;

		vEvn[OVL_CONNECT] = g_ol[OVL_CONNECT].hEvent;
		vEvn[OVL_SEND   ] = g_ol[OVL_SEND   ].hEvent;
		vEvn[OVL_RECV   ] = g_ol[OVL_RECV   ].hEvent;

		nE = WSAWaitForMultipleEvents(OVL_TOT, vEvn, FALSE, WSA_INFINITE, FALSE);

		if(WSA_WAIT_FAILED == nE)
		{
			hr = WSAGetLastError();
			LogGetLastError(hr);
			goto END;
		}

		nE -= WSA_WAIT_EVENT_0;				// 인덱스 재조정

		for(i= nE; i<OVL_TOT; ++i)
		{
			hr = WSAWaitForMultipleEvents(1, &vEvn[i], TRUE, 0, FALSE);
			if( WSA_WAIT_FAILED  == hr || WSA_WAIT_TIMEOUT == hr)
				continue;


			// 비동기 입출력 결과 확인
			hr = WSAGetOverlappedResult(g_scHost, &g_ol[i], &dTran, FALSE, &dFlag);

			g_ol[i].ResetEvent();			// 이벤트 비 신호 상태


			if(OVL_CONNECT == i)
			{
				hr = AsyncRcv();			// 비동기 수신 요청
				if(-1 == hr)
					goto END;

				continue;
			}
			else if(OVL_SEND == i)			// Send Notifiy
			{
				printf("Succeded Sending Message: %d\n", (int)dTran);
				continue;
			}
			else if(OVL_RECV == i)			// Recv Notifiy
			{
				if(FALSE == hr || 0 == dTran)
				{
					printf("Disconnect -----------------------\n");
					goto END;
				}

				printf("Recv: %s\n", g_ol[OVL_RECV].csBuf);

				AsyncRcv();
			}
		}
	}

END:

	_endthreadex(0);
	return 0;
}


int	AsyncSnd(char* csBuf, int iLen)
{
	int hr=-1;

	g_ol[OVL_SEND].Close();

	g_ol[OVL_SEND].wsBuf.len = iLen;
	memcpy(g_ol[OVL_SEND].csBuf, csBuf, iLen);


	//hr = WriteFile((HANDLE)g_scHost		// 송신 소켓
	//			, &g_ol[OVL_SEND].csBuf		// 송신 버퍼 포인터
	//			, iLen
	//			, &g_ol[OVL_SEND].dTran
	//			, &g_ol[OVL_SEND]			// OVERLAPPED 구조체 포인터
	//			);

	hr = WSASend(g_scHost					// 송신 소켓
				, &g_ol[OVL_SEND].wsBuf		// WSBUF 포인터
				, 1							// WSBUF의 수
				, &g_ol[OVL_SEND].dTran
				,  g_ol[OVL_SEND].dFlag
				, &g_ol[OVL_SEND]			// OVERLAPPED 구조체 포인터
				, nullptr
				);


	if(SOCKET_ERROR ==hr)
	{
		hr = WSAGetLastError();
		if(WSA_IO_PENDING !=hr && WSAEWOULDBLOCK !=hr)
			return -1;
	}

	return 0;
}


int	AsyncRcv()
{
	int hr=-1;

	g_ol[OVL_RECV].Close();


	//hr = ReadFile((HANDLE)g_scHost		// 수신 소켓
	//			, g_ol[OVL_RECV].csBuf		// 수신 버퍼
	//			, MAX_BUF
	//			, &g_ol[OVL_RECV].dTran
	//			, &g_ol[OVL_RECV]			// OVERLAPPED 구조체 포인터
	//			);

	hr = WSARecv(g_scHost					// 수신 소켓
				, &g_ol[OVL_RECV].wsBuf		// WSBUF 포인터
				, 1							// WSBUF의 수
				, &g_ol[OVL_RECV].dTran
				, &g_ol[OVL_RECV].dFlag
				, &g_ol[OVL_RECV]			// OVERLAPPED 구조체 포인터
				, nullptr
				);

	if(SOCKET_ERROR ==hr)
	{
		hr = WSAGetLastError();
		if(WSA_IO_PENDING !=hr && WSAEWOULDBLOCK !=hr)
			return -1;
	}

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

