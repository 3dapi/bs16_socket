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


#define	MAX_BUF		8192


char	sPt[32]="60000";
char	sIp[64]="127.0.0.1";

void	LogGetLastError(int hr);


SOCKET	g_scHost=0;					// 소켓
FD_SET	g_fdSet;					// 소켓 FD_SET

int		TestSendData();
int		FrameMove();
void	CloseSocket();


int main()
{
	WSADATA		wsData = {0};
	SOCKADDR_IN	sdHost = {0};
	int hr=-1;

	if(WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
		return -1;

	// create the host socket
	g_scHost=socket(AF_INET, SOCK_STREAM, 0);
	if(g_scHost==INVALID_SOCKET)
		return -1;


	memset(&sdHost, 0, sizeof(sdHost));
	sdHost.sin_family      = AF_INET;
	sdHost.sin_addr.s_addr = inet_addr(sIp);
	sdHost.sin_port        = htons( atoi(sPt) );
	hr = connect(g_scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR == hr)
	{
		hr = WSAGetLastError();
		LogGetLastError(hr);

		return -1;
	}
	
	// Non blocking socket
	u_long on =1;
	hr = ioctlsocket(g_scHost, FIONBIO, &on);

	
	FD_ZERO(&g_fdSet);

	// read_fds의 카운터를 하나만 증가
	FD_SET(g_scHost, &g_fdSet);


	while(g_scHost)
	{
		if(FAILED(FrameMove()))
			break;

		// 프로세스를 다 사용하므로 
		// 이렇게라도 해야 한다.
		Sleep(1);
	}
	
	CloseSocket();
	WSACleanup();
}


int TestSendData()
{
	int		hr;
	char	sBufSnd[MAX_BUF+4]={0};
	int		iSnd;
	static int c=0;
	++c;

	static int d=0;

	if(c<50)
		return 0;


	c=0;
	++d;
	sprintf(sBufSnd, "Client Send: %4d", d);

	//6. 데이터 전송
	iSnd = strlen(sBufSnd);
	hr = send(g_scHost, sBufSnd, iSnd, 0);

	if(SOCKET_ERROR == hr)
	{
		hr = WSAGetLastError();
		if(WSAEWOULDBLOCK !=hr)
			return -1;
	}

	return 0;
}



void CloseSocket()
{
	//5. 종료
	if(g_scHost)
	{
		shutdown(g_scHost, SD_BOTH);
		closesocket(g_scHost);
		g_scHost = 0;
	}
}


int FrameMove()
{
	int		hr=-1;

	if(!g_scHost)
		return -1;

	FD_SET	fdsTmp;
	TIMEVAL	timeout;

	if(FAILED(TestSendData()))
		return -1;

	// 이렇게 매번 읽는 루틴에서 시간과 FD_SET을 다시 할당해야
	// 제대로 동작한다.
	fdsTmp = g_fdSet;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	hr = select(0, &fdsTmp, nullptr, nullptr, &timeout);

	if(SOCKET_ERROR == hr)
		return -1;

	// time out
	if(0 == hr)
		return 0;


	if(!FD_ISSET(g_scHost, &fdsTmp))
		return 0;

	int		iRcv=0;
	char	sBufRcv[MAX_BUF+4]={0};

	iRcv = recv(g_scHost, sBufRcv, MAX_BUF, 0);

	// connection terminated
	if(0>=iRcv)
		return -1;

	// print the received message
	printf("Recv: %s\n", sBufRcv);

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

