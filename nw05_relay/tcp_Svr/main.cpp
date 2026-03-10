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


#define	MAX_BUF		8192


char	sPt[32]="60000";

void LogGetLastError(int hr);


vector<SOCKET >		m_vCln;


void DeleteNotUseHost()
{
	vector<SOCKET >::iterator _F = m_vCln.begin();

	for( ; _F != m_vCln.end(); )
	{
		if(0 >= (int)(*_F))
		{
			_F = m_vCln.erase(_F);
			continue;
		}

		++_F;
	}
}


void DeleteAllHost()
{
	int i=0;
	int iSize = m_vCln.size();

	for(i=0; i< iSize; ++i)
	{
		if(0 >= (int)m_vCln[i])
			continue;

		shutdown(m_vCln[i], SD_BOTH);
		closesocket(m_vCln[i]);
	}

	m_vCln.clear();
}




void EchoMsg(char* buf, int iLen);


int main()
{
	WSADATA		wsData={0};
	SOCKET		scLstn=0;
	SOCKADDR_IN sdLstn={0};

	int			hr=-1;
	int			i = 0;


	printf("Starting Server.\nPort: %s\n", sPt);


	// Load Winsock DLL
	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
		return -1;



	// Listen 소켓 생성
	scLstn=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == scLstn)
		return -1;


	// Listen 소켓 주소 할당
	memset(&sdLstn, 0, sizeof(sdLstn));
	sdLstn.sin_family      = AF_INET;
	sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
	sdLstn.sin_port        = htons( atoi(sPt) );
	hr = bind(scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR == hr)
		return -1;


	hr = listen(scLstn, SOMAXCONN);
	if( SOCKET_ERROR == hr)
		return -1;


	// Non-blocking 소켓 설정
	unsigned long nonBlocking =1;
	hr = ioctlsocket(scLstn, FIONBIO, &nonBlocking);

	if(SOCKET_ERROR == hr)
	{
		hr = WSAGetLastError();
		LogGetLastError(hr);
	}


	while(1)
	{
		SOCKET		scCln=0;
		SOCKADDR_IN sdCln={0};
		int			iSize = sizeof(SOCKADDR_IN);

		// accept process
		scCln=accept(scLstn, (SOCKADDR*)&sdCln, &iSize);
		if(INVALID_SOCKET == scCln)
		{
			hr = WSAGetLastError();
			if( WSAEWOULDBLOCK != hr)
			{
				printf("Listen Socket Error.\n");
				LogGetLastError(hr);
				break;
			}
		}
		else
		{
			// add the new client to list
			char* sIP = inet_ntoa( sdCln.sin_addr);
			printf("Connect Client(%d) IP: %s\n", (int)scCln, sIP);

			m_vCln.push_back(scCln);
		}


		// receive process
		vector<SOCKET >::iterator _F= m_vCln.begin();
		for( ; _F != m_vCln.end();	++_F)
		{
			char sRcv[MAX_BUF+4]={0};

			SOCKET scH = (*_F);
			int iRcv = recv(scH, sRcv, MAX_BUF, 0);

			if(SOCKET_ERROR == iRcv)
			{
				hr = WSAGetLastError();
				if(WSAEWOULDBLOCK != hr)
				{
					// Err:close
					printf("Rcv:%d, Disconnect Client: %d\n", iRcv, (int)scH);
					shutdown(scH, SD_BOTH);
					closesocket(scH);
					(*_F) = 0;
				}

				continue;
			}
			else if(0 == iRcv)
			{
				// close
				printf("Rcv:%d, Disconnect Client: %d\n", iRcv, (int)scH);
				shutdown(scH, SD_BOTH);
				closesocket(scH);
				(*_F) = 0;
				continue;
			}

			char	sSnd[MAX_BUF]={0};
			int		iLen  =0;
			sprintf(sSnd, "Recv %2d> %s", (int)scH, sRcv);
			printf("%s\n", sSnd);

			iLen = strlen(sSnd);

			// send the received message to all client
			EchoMsg(sSnd, iLen);
		}


		// 사용할 수 없는 소켓 제거
		DeleteNotUseHost();
	}


	// 연결 종료
	DeleteAllHost();

	shutdown(scLstn, SD_BOTH);
	closesocket(scLstn);

	WSACleanup();

	return 0;
}


// 접속한 모든 클라이언트에 전송
void EchoMsg(char* buf, int iLen)
{
	int		hr;
	SOCKET scH;

	vector<SOCKET >::iterator _F= m_vCln.begin();
	for( ; _F != m_vCln.end();	++_F)
	{
		if( 0 >= (*_F))
			continue;

		scH = (*_F);

		hr = send( scH, buf, iLen, 0);
		if(SOCKET_ERROR == hr)
		{
			hr = WSAGetLastError();

			if(WSAEWOULDBLOCK == hr)
				continue;


			// error 소켓: 0으로 설정
			(*_F) = 0;
		}
	}
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

