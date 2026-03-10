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
#define	MAX_HOST	WSA_MAXIMUM_WAIT_EVENTS


char	sPt[32]="60000";

void LogGetLastError(int hr);


struct RemoteHost
{
	SOCKET		scH;					// socket
	SOCKADDR_IN sdH;					// address
	WSAEVENT	seH;					// event
	int			nBuf;					// recorded byte
	char		sBuf[MAX_BUF];			// Send buffer

	RemoteHost()
	{
		scH	= 0;
		seH	= nullptr;
		memset(&sdH, 0, sizeof(SOCKADDR_IN));

		nBuf	= 0;
		memset(sBuf, 0, MAX_BUF);
	}

	RemoteHost(SOCKET s, SOCKADDR_IN* d, WSAEVENT e)
	{
		scH	= s;
		seH	= e;
		memcpy(&sdH, d, sizeof(SOCKADDR_IN));

		nBuf	= 0;
		memset(sBuf, 0, MAX_BUF);
	}

	~RemoteHost()
	{
		Close();
	}

	void Close()
	{
		if(0 == scH)
			return;

		shutdown(scH, SD_BOTH);
		closesocket(scH);
		scH = 0;

		CloseHandle(seH);
		seH = nullptr;

		nBuf	= 0;
		memset(sBuf, 0, MAX_BUF);
	}

	void SetupBuf(char* s, int len)
	{
		nBuf = len;
		memcpy(sBuf, s, nBuf);
		memset(sBuf+nBuf, 0, MAX_BUF-len);
	}
};


vector<RemoteHost* >	g_vHost;			// host list


HANDLE			g_hThSnd = nullptr;			// Send용 스레드 핸들
HANDLE			g_hThRcv = nullptr;			// 비동기 통지용 스레드 핸들
DWORD WINAPI	WorkSnd(void*);				// Send용 스레드
DWORD WINAPI	WorkRcv(void*);				// 비동기 통지용 스레드

CRITICAL_SECTION m_CS;						// 임계 구역


void		EchoMsg(char* s, int len);


RemoteHost* FindHost(HANDLE e)
{
	if(nullptr == e)
		return nullptr;

	int iSize = (int)g_vHost.size();

	for(int i=0; i<iSize; ++i)
	{
		if(e == g_vHost[i]->seH)
			return g_vHost[i];
	}

	return nullptr;
}

void DeleteHost(SOCKET scH)
{
	int iSize = (int)g_vHost.size();

	for(int i=0; i<iSize; ++i)
	{
		if(scH == g_vHost[i]->scH)
		{
			delete g_vHost[i];
			g_vHost.erase( g_vHost.begin() + i);
			return;
		}
	}

}

void DeleteNotUseHost()
{
	EnterCriticalSection(&m_CS);
	vector<RemoteHost* >::iterator _F = g_vHost.begin();

	for( ; _F != g_vHost.end(); )
	{
		RemoteHost* p = (*_F);

		if(p && 0 >= p->scH)
		{
			delete p;
			_F = g_vHost.erase(_F);
			continue;
		}

		++_F;
	}

	LeaveCriticalSection(&m_CS);
}


void DeleteAllHost()
{
	int iSize = (int)g_vHost.size();

	for(int i=0; i<iSize; ++i)
		delete g_vHost[i];

	g_vHost.clear();
}



int main()
{
	// 임계 구역 초기화
	InitializeCriticalSection(&m_CS);


	WSADATA		wsData={0};
	int			hr =-1;

	printf("Starting Server.\nPort: %s\n", sPt);


	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
		return -1;

	SOCKET		scLstn	= 0;
	SOCKADDR_IN	sdLstn	= {0};
	WSAEVENT	seLstn  = nullptr;

	scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(INVALID_SOCKET == scLstn)
		return -1;


	memset(&sdLstn, 0, sizeof(sdLstn));
	sdLstn.sin_family      = AF_INET;
	sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
	sdLstn.sin_port        = htons( atoi(sPt) );

	hr = bind(scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));

	if(SOCKET_ERROR == hr)
		return -1;


	// 이벤트 객체 생성
	seLstn = WSACreateEvent();

	// <소켓, 이벤트, 이벤트 통지 종류> 바인딩
	// 소켓은 자동으로 Non-blocking으로 전환
	hr = WSAEventSelect(scLstn, seLstn, (FD_ACCEPT|FD_CLOSE));


	// Listen 소켓과 이벤트 등을 호스트 리스트에 추가
	g_vHost.push_back(new RemoteHost(scLstn, &sdLstn, seLstn));


	// Listen 상태
	hr = listen(scLstn, SOMAXCONN);
	if(SOCKET_ERROR ==hr)
		return -1;


	//데이터 송신용 스레드 생성. 대기 상태
	g_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
							, (unsigned (__stdcall*)(void*))WorkSnd
							, nullptr, CREATE_SUSPENDED, nullptr);

	// 비동기 통지용 스레드 생성
	g_hThRcv = (HANDLE)_beginthreadex(nullptr, 0
							, (unsigned (__stdcall*)(void*))WorkRcv
							, nullptr, 0, nullptr);


	while(1)
	{
		// 서버 컨텐츠 실행
		Sleep(500);
	}

	// 스레드 핸들 해제
	CloseHandle(g_hThRcv);
	CloseHandle(g_hThSnd);

	DeleteAllHost();

	WSACleanup();

	DeleteCriticalSection(&m_CS);

	return 0;
}


DWORD WINAPI WorkRcv(void* pParam)
{
	while(1)
	{
		RemoteHost*		pHost = nullptr;

		WSANETWORKEVENTS wnE={0};
		WSAEVENT		vEvn[MAX_HOST]={0};		// Event List

		int				hr = 0;
		int				i  = 0, nLst=0;
		int				nE=-1;


		// listen 포함, 클라이언트의 모든 이벤트를 리스트에 설정
		for(i=0; i<(int)g_vHost.size(); ++i)
		{
			RemoteHost* p =  g_vHost[i];
			if(0 == p->scH)
				continue;

			vEvn[nLst] = p->seH;
			++nLst;
		}


		// Accept 포함한 네트워크 이벤트를 기다린다.
		nE = WSAWaitForMultipleEvents(nLst, vEvn, FALSE, WSA_INFINITE, FALSE);

		if(nE == WSA_WAIT_FAILED)
		{
			hr = WSAGetLastError();
			LogGetLastError(hr);
			break;
		}


		nE -= WSA_WAIT_EVENT_0;		// 인덱스 재조정

		for(i= nE; i<nLst; ++i)
		{
			hr = WSAWaitForMultipleEvents(1, &vEvn[i], TRUE, 0, FALSE);
			if( WSA_WAIT_FAILED  == hr || WSA_WAIT_TIMEOUT == hr)
				continue;


			// get the host
			pHost = FindHost(vEvn[i]);
			if(nullptr == pHost)
				continue;

			SOCKET scHost = pHost->scH;

			// 이벤트 분해
			hr = WSAEnumNetworkEvents(scHost, vEvn[i], &wnE);
			if(SOCKET_ERROR == hr)
			{
				hr= WSAGetLastError();
				printf("WSAEnumNetworkEvents Error\n");


				// listen socket
				if(scHost == g_vHost[0]->scH)
				{
					printf("Listen socket or Network Error\n");
					goto END;
				}

				pHost->Close();
				printf("Client Socket Close: %d\n", (int)scHost);

				continue;
			}


			////////////////////////////////////////////////////////////////////
			// Accept event
			if( FD_ACCEPT & wnE.lNetworkEvents)
			{
				SOCKET		scLstn = scHost;
				SOCKET		scCln;
				SOCKADDR_IN	sdCln;
				WSAEVENT	seCln;

				int iSize = sizeof(SOCKADDR_IN);

				// accept()함수로 클라이언트 소켓, 주소 얻기
				scCln = accept(scLstn, (SOCKADDR*)&sdCln, &iSize);

				// Nagle off
				int v = 1;
				hr = setsockopt(scCln, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
				if(SOCKET_ERROR == hr)
				{
					hr = WSAGetLastError();
					LogGetLastError(hr);
					continue;
				}

				// 이벤트 객체 생성
				seCln = WSACreateEvent();

				// 접속한 클라이언트도 비동기로 통지 받을 수 있도록
				// <소켓, 이벤트 객체, 비동기 통지 이벤트 종류> 바인딩
				WSAEventSelect(scCln, seCln, FD_READ|FD_WRITE|FD_CLOSE);
				

				// 사용하지 않는 Host list에 추가
				if(MAX_HOST <= (int)g_vHost.size() )
				{
					printf("There is no empty element for client.\n");

					// disconnect the client
					shutdown(scCln, SD_BOTH);
					closesocket(scCln);
				}
				else
					g_vHost.push_back(new RemoteHost(scCln, &sdCln, seCln));


				continue;
			}


			////////////////////////////////////////////////////////////////////
			// I/O event
			// Sending event
			if( FD_WRITE & wnE.lNetworkEvents)
			{
			}

			// closing event
			else if( FD_CLOSE & wnE.lNetworkEvents)
			{
				pHost->Close();
				printf("Disconnect Client: %d\n", (int)scHost);
			}

			// receive event
			else if( FD_READ & wnE.lNetworkEvents)
			{
				int		iRcv=0;
				char	sBufRcv[MAX_BUF+4]={0};

				iRcv=recv(scHost, sBufRcv, MAX_BUF, 0);

				if(0 > iRcv)
				{
					hr = WSAGetLastError();

					if(WSAEWOULDBLOCK != hr)
					{
						pHost->Close();
						printf("Disconnect Client: %d\n", (int)scHost);
						LogGetLastError(hr);

						continue;
					}
				}
				else if(0 == iRcv)
				{
					pHost->Close();
					printf("Disconnect Client: %d\n", (int)scHost);
				}
				else
				{
					printf("Recv from Client : %d %s\n", scHost, sBufRcv);

					// test message
					char	sBufBuf[MAX_BUF]={0};
					sprintf(sBufBuf, "%5d : %s", scHost, sBufRcv);

					// send the message to all client
					EchoMsg(sBufBuf, strlen(sBufBuf) );
				}
			}// else if FD_READ
		}// for


		// clear the host list (not used)
		DeleteNotUseHost();

	}// while

END:

	_endthreadex(0);
	return 0;
}


void EchoMsg(char* s, int iLen)
{
	EnterCriticalSection(&m_CS);

	int iSize = (int)g_vHost.size();

	for(int i=1; i<iSize; ++i)
	{
		RemoteHost* pCln = g_vHost[i];
		if(0 >= pCln->scH)
			continue;

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

		int iSize = (int)g_vHost.size();

		for(int i=1; i<iSize; ++i)
		{
			RemoteHost* pCln = g_vHost[i];
			if(0 >= pCln->scH)
				continue;


			int	iLen = pCln->nBuf;
			int iSnd = 0;
			int	iTot = 0;
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

