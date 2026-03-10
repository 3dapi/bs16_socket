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

void LogGetLastError(int hr);


int main()
{
	WSADATA		wsData={0};
	SOCKET		scHost=0;
	SOCKADDR_IN	sdHost={0};

	int			hr =-1;

	printf("Starting Client.\nPort: %s\n", sPt);


	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
		return -1;


	scHost=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == scHost)
		return -1;


	memset(&sdHost, 0, sizeof(sdHost));
	sdHost.sin_family      = AF_INET;
	sdHost.sin_addr.s_addr = inet_addr(sIp);
	sdHost.sin_port        = htons( atoi(sPt) );
	if(SOCKET_ERROR == connect(scHost, (SOCKADDR*)&sdHost, sizeof(SOCKADDR_IN)))
		return -1;

	// Non-blocking 소켓
	u_long	nonBlocking =1;
	ioctlsocket(scHost, FIONBIO, &nonBlocking);


	DWORD dCur = GetTickCount();
	DWORD dOld = dCur;

	int tst = 0;

	while(1)
	{
		dCur = GetTickCount();
		if( dCur - dOld >= 1000)
		{
			dOld = dCur;
			++tst;

			char sSnd[MAX_BUF]={0};
			sprintf(sSnd, "Send from client: %d", tst);

			int iLen = strlen(sSnd);
			send(scHost, sSnd, iLen, 0);
		}


		{
			char sBufRcv[MAX_BUF+4]={0};
			int iRcv=0;

			iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);
			if(SOCKET_ERROR == iRcv)
			{
				hr = WSAGetLastError();
				if( WSAEWOULDBLOCK == hr)
					continue;


				printf("Socket Error::Disconnect.\n");
				LogGetLastError(hr);
				break;
			}
			else if(0 == iRcv)
			{
				printf("Close::Disconnect.\n");
				break;
			}

			printf("Recv from server : %s\n", sBufRcv);
		}
	}


	// 연결 종료
	shutdown(scHost, SD_BOTH);
	closesocket(scHost);

	// Unload WinSock DLL
	WSACleanup();

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

