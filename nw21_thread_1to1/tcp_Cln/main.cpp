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


#define	MAX_BUF		8192

char	sPt[32]="60000";
char	sIp[64]="127.0.0.1";

void LogGetLastError(int hr);


SOCKET			g_scHost=0;				// listen socket
SOCKADDR_IN		g_sdHost={0};

char			g_bufSnd[MAX_BUF]={0};	// send buffer
CRITICAL_SECTION	m_CS;


HANDLE			g_hThRcv;				// receive thread handle
HANDLE			g_hThSnd;				// send thread handle


DWORD WINAPI WorkRcv(void*);			// Receive용 쓰레드
DWORD WINAPI WorkSnd(void*);			// Send용 쓰레드


int main()
{
	InitializeCriticalSection(&m_CS);

	WSADATA		wsData;
	int			hr=-1;

	printf("Starting.\nPort: %s\n", sPt);


	if(WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
		return -1;


	g_scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == g_scHost)
		return -1;


	if(INVALID_SOCKET == g_scHost)
		return -1;


	memset(&g_sdHost, 0, sizeof(g_sdHost));
	g_sdHost.sin_family      = AF_INET;
	g_sdHost.sin_addr.s_addr = inet_addr(sIp);
	g_sdHost.sin_port        = htons( atoi(sPt) );

	hr = connect(g_scHost, (SOCKADDR*)&g_sdHost, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR == hr)
		return -1;

	// create recv/send thread
	// 수신용 Thread
	g_hThRcv = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkRcv
						, nullptr, 0, nullptr);

	// 송신용 Thread생성
	g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkSnd
						, nullptr, 0, nullptr);

	static int cnt = 0;

	// Main process
	while(100>cnt++)
	{
		Sleep(100);

		// fill the send buffer
		EnterCriticalSection(&m_CS);
		sprintf(g_bufSnd, "Send Data %d", cnt);
		LeaveCriticalSection(&m_CS);
	}

	WaitForSingleObject(g_hThRcv, INFINITE);

	CloseHandle(g_hThRcv);
	CloseHandle(g_hThRcv);

	closesocket(g_scHost);
	WSACleanup();

	DeleteCriticalSection(&m_CS);

	return 0;
}



DWORD WINAPI WorkRcv(void *)
{
	int		hr = 0;

	while(1)
	{
		Sleep(1);

		if(g_scHost)
		{
			int iRcv = 0;
			char bufRcv[MAX_BUF+4]={0};

			iRcv=recv(g_scHost, bufRcv, MAX_BUF, 0);
			if(SOCKET_ERROR == iRcv)
			{
				hr = WSAGetLastError();
				LogGetLastError(hr);
				printf("DisConnect: %d\n", (int)g_scHost);
				break;
			}
			else if(0 == iRcv)
			{
				printf("DisConnect: %d\n", (int)g_scHost);
				break;
			}

			printf("Recv: %s\n", bufRcv);
		}
	}

	_endthreadex(0);
	return 0;
}


DWORD WINAPI WorkSnd(void *pParam)
{
	int hr = 0;

	while(1)
	{
		Sleep(1);

		int iSnd=0;
		int	iLen=0;

		iLen = strlen(g_bufSnd);

		if(0 >= iLen)
			continue;

		iSnd=send(g_scHost, g_bufSnd, iLen, 0);
		if(0 >= iSnd)
		{
			hr = WSAGetLastError();
			LogGetLastError(hr);
			printf("Disconnnect: %d\n", (int)g_scHost);
			break;
		}


		// clear the send buffer
		EnterCriticalSection(&m_CS);
		memset(g_bufSnd, 0, sizeof( g_bufSnd));
		LeaveCriticalSection(&m_CS);
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

