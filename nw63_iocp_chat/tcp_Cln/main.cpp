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
int  GetSystemProcessorCount();
void CloseHostSocket();


struct OVERLAP_EX : public WSAOVERLAPPED
{
	DWORD		dType;					// I/O type. FD_READ or FD_WRITE
	DWORD		dTran;					// Transfered
	DWORD		dFlag;					// Flag
	WSABUF		wsBuf;
	char		csBuf[MAX_BUF+4];		// Io Completion Buffer

	OVERLAP_EX()
	{
		Reset();
		dType	= 0;
	}

	OVERLAP_EX(int nType)
	{
		Reset();
		dType	= nType;
	}

	void Reset()
	{
		memset(this, 0, sizeof(WSAOVERLAPPED) );
		memset(csBuf, 0, MAX_BUF+4 );

		dFlag	= 0;
		dTran	= 0;
		wsBuf.len = MAX_BUF;
		wsBuf.buf = csBuf;
	}

	void SetBuf(char* s, int l)
	{
		if(s && 0<l)
		{
			wsBuf.len = l;
			memcpy(csBuf, s, l);
		}
		else
		{
			wsBuf.len = MAX_BUF;
		}
	}

	// for receive
	int AsyncRcv(SOCKET s)
	{
		 return WSARecv(s, &wsBuf, 1, &dTran, &dFlag, this, nullptr);
	}

	// for sending
	int AsyncSnd(SOCKET s)
	{
		int hr = WSASend(s, &wsBuf, 1, &dTran, dFlag, this, nullptr);
		if(0 == hr)
			printf("Send complete: %d byte\n", dTran);

		return hr;
	}
};



SOCKET			g_scHost = 0;			// listen socket
OVERLAP_EX		g_olSnd(FD_WRITE);
OVERLAP_EX		g_olRcv(FD_READ);

HANDLE			g_hIocp	 = nullptr;		// IOCP Handle

DWORD	WINAPI	WorkThread(void*);		// Work 쓰레드


int	AsyncSend(char* s, int l)
{
	g_olSnd.Reset();
	g_olSnd.SetBuf(s, l);
	return g_olSnd.AsyncSnd(g_scHost);
}

int	AsyncRecv()
{
	g_olRcv.Reset();
	return g_olRcv.AsyncRcv(g_scHost);
}



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


	// Connection
	SOCKADDR_IN	sdHost={0};

	sdHost.sin_family      = AF_INET;
	sdHost.sin_addr.s_addr = inet_addr(sIp);
	sdHost.sin_port        = htons( atoi(sPt) );

	hr = connect(g_scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR ==hr)
		return -1;


	// IOCP 객체 생성
	g_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if(nullptr == g_hIocp)
		return -1;


	// 2 * processor 개수만큼 Work 쓰레드 생성
	int nWrkPrc = 2 * GetSystemProcessorCount();
	for(int i=0; i<nWrkPrc; ++i)
	{
		HANDLE hThWrk = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkThread
						, nullptr, 0, nullptr);

		CloseHandle(hThWrk);
	}



	// <소켓, IOCP, key> 바이딩
	ULONG_PTR	pIoKey = (ULONG_PTR)&g_scHost;		// IO Key  is Socket

	// 소켓과 CompletionPort의 연결.
	CreateIoCompletionPort((HANDLE)g_scHost, g_hIocp, pIoKey, 0);


	// post the success connection message
	ULONG_PTR		dKey = FD_CONNECT;
	WSAOVERLAPPED	tOl={0};
	hr = PostQueuedCompletionStatus(g_hIocp, sizeof(ULONG_PTR), dKey, &tOl);


	// 비동기 수신 요청
	AsyncRecv();


	// 채팅...
	while(g_scHost)
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

		if(0 == g_scHost)
			break;


		// 데이터 송신
		hr = AsyncSend(sSnd, iLen);
		if(SOCKET_ERROR == hr)
		{
			hr =  WSAGetLastError();
			if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
			{
				LogGetLastError(hr);
				break;
			}
		}
	}


	CloseHandle(g_hIocp);

	CloseHostSocket();

	WSACleanup();

	return 0;
}




DWORD WINAPI WorkThread(void* pParam)
{
	int			hr = 0;
	ULONG_PTR	pIoKey	= nullptr;
	OVERLAP_EX*	pOL		= nullptr;
	DWORD		dTran	= 0;
	DWORD		OLType	= 0;


	while(g_scHost)
	{
		pIoKey	= nullptr;
		pOL		= nullptr;
		dTran	= 0;

		hr = GetQueuedCompletionStatus(
				g_hIocp									// Completion Port
			,	&dTran									// 전송 된 바이트 수
			,	(PULONG_PTR)&pIoKey
			,	(LPOVERLAPPED*)&pOL						// OVERLAPPED 구조체
			,	INFINITE
			);

		// IO Failed
		if(0 == hr)
		{
			hr = GetLastError();
			LogGetLastError(hr);

			if(nullptr == pOL && nullptr == pIoKey)
				continue;


			break;
		}

		if(0 == dTran)									// disconnect
		{
			printf("Disconnect\n");
			break;
		}

		// PostQueuedCompletionStatus() 함수로 보낸 접속 메시지
		if(sizeof(ULONG_PTR) == dTran && FD_CONNECT == pIoKey)
		{
			printf("Connection Successed\n");
			continue;
		}

		OLType= pOL->dType;

		if(FD_READ == OLType)							// receiving complete
		{
			printf("Recv from Server(%3d): %s\n"
					, dTran, pOL->csBuf);
			AsyncRecv();								// 비동기 수신 요청
		}
		else if(FD_WRITE == OLType)						// Sending complete
			printf("Write Successed\n");
	}


	CloseHostSocket();

	_endthreadex(0);

	return 0;
}


void CloseHostSocket()
{
	shutdown(g_scHost, SD_BOTH);
	closesocket(g_scHost);
	g_scHost = 0;
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

