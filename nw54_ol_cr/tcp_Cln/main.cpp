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
	DWORD		dType;
	DWORD		dFlag;
	DWORD		dTran;
	WSABUF		wsBuf;
	char		csBuf[MAX_BUF+4];

	OVERLAP_EX()
	{
		Reset();
		dType	= 0;
	}

	OVERLAP_EX(int type)
	{
		Reset();
		dType	= type;
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


SOCKET					g_scHost = 0;
OVERLAP_EX				g_olRcv(FD_READ);
OVERLAP_EX				g_olSnd(FD_WRITE);

DWORD WINAPI WorkThread(void*);								// work thread
void  CALLBACK	CompletionRoutine(DWORD, DWORD
							,LPWSAOVERLAPPED,DWORD);		// completion routine

void CloseSocket();


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
	{
		Sleep(10);
		hr = WSAGetLastError();

		if(WSAEWOULDBLOCK !=hr)
			return -1;
	}


	// Work 쓰레드 생성
	HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkThread
						, nullptr, 0, nullptr);


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
		g_olSnd.SetBuf(sSnd, iLen);
		hr = g_olSnd.AsyncSnd(g_scHost, CompletionRoutine);
		if(0 == hr)
		{
			printf("Complete sending: %d byte\n", g_olSnd.dTran);
			g_olSnd.Reset();
		}
		else if(SOCKET_ERROR == hr)
		{
			hr =  WSAGetLastError();
			if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
			{
				LogGetLastError(hr);
				break;
			}
		}
	}


	CloseHandle(hWork);
	CloseSocket();

	WSACleanup();

	return 0;
}



DWORD WINAPI WorkThread(void* pParam)
{
	while(g_scHost)
	{
		int	hr = 0;

		// receiving state
		hr =g_olRcv.AsyncRcv(g_scHost, CompletionRoutine);
		if(SOCKET_ERROR == hr)
		{
			hr =  WSAGetLastError();
			if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
			{
				LogGetLastError(hr);
				return -1;
			}
		}

		hr = SleepEx(INFINITE, TRUE);
		if(WAIT_IO_COMPLETION == hr)
			continue;

	}

	_endthreadex(0);

	return 0;
}


// Completion Routine
void CALLBACK CompletionRoutine(DWORD dErr, DWORD dTran,LPWSAOVERLAPPED pOl, DWORD dFlag)
{
	int	hr = 0;

	OVERLAP_EX* pExOl = (OVERLAP_EX*)pOl;

	// Error or disconnected
	if(dErr != 0 || dTran == 0)
	{
		printf("Disconnect\n");
		CloseSocket();
		return;
	}

	// sending completion
	else if(FD_WRITE == pExOl->dType)
	{
		printf("Complete sending: %d byte\n", dTran);
	}

	// receiving completion
	else if(FD_READ == pExOl->dType)
		printf("Recv[%3d]: %s\n", dTran, pExOl->csBuf);


	pExOl->Reset();
}


void CloseSocket()
{
	if(0 == g_scHost)
		return;

	shutdown(g_scHost, SD_BOTH);
	closesocket(g_scHost);
	g_scHost = 0;
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

