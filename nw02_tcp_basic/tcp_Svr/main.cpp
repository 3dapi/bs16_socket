//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)


// Winsock 을 사용하기 위한 라이브러리
#pragma comment(lib, "ws2_32.lib")

// winsock2.h 헤더파일은 Windows.h 헤더파일보다 항상 앞서있어야 한다.
#include <winsock2.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char	sPt[32]="60000";


// TCP Server Network 설정
// Load Winsock DLL
// Listen 용 TCP 소켓 생성
// Listen 용 소켓에 주소 할당
// 연결 요청 대기 상태
// 연결 요청 수락
// 데이터 입/출력
// 연결 종료
// Unload WinSock DLL


int main()
{
	WSADATA		wsData={0};
	SOCKET		scLstn=0;
	SOCKADDR_IN sdLstn={0};

	SOCKET		scCln=0;
	SOCKADDR_IN sdCln={0};
	int			hr=-1;


	char sBufSnd[1024]="Welcome to network programming!!!";

	printf("Starting Server.\nPort: %s\n", sPt);


	// Load Winsock DLL
	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
	{
		printf("Winsock Err\n");
		return -1;
	}


	// Listen 용 TCP 소켓 생성
	scLstn=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == scLstn)
		return -1;


	// Listen 용 소켓에 주소 할당
	memset(&sdLstn, 0, sizeof(sdLstn));
	sdLstn.sin_family     = AF_INET;
	sdLstn.sin_addr.s_addr= htonl(INADDR_ANY);
	sdLstn.sin_port       = htons( atoi(sPt) );

	hr = bind(scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));

	if(SOCKET_ERROR == hr)
		return -1;


	// Listen 상태
	hr = listen(scLstn, SOMAXCONN);
	if( SOCKET_ERROR == hr)
		return -1;


	// 연결 요청 수락
	int iSize=sizeof(sdCln);

	scCln=accept(scLstn, (SOCKADDR*)&sdCln,&iSize);

	if(scCln==INVALID_SOCKET)
		return -1;


	static int iCnt =0;

	while(50>iCnt)
	{
		Sleep(100);

		++iCnt;
		sprintf(sBufSnd, "Network message %d", iCnt);

		// 데이터 입/출력==>(수신/송신)
		send(scCln, sBufSnd, strlen(sBufSnd), 0);
	}


	// 연결 종료
	closesocket(scCln);		// client socket
	closesocket(scLstn);	// listen socket


	// Unload WinSock DLL
	WSACleanup();

	return 0;
}

