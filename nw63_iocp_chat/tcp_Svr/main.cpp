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
void GetIp(char* s, SOCKET scH);
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
		//return ReadFile((HANDLE)s, csBuf, MAX_BUF, &dTran, this);
		return WSARecv(s, &wsBuf, 1, &dTran, &dFlag, this, nullptr);
	}

	// for sending
	int AsyncSnd(SOCKET s)
	{
		//return WriteFile((HANDLE)s, csBuf, wsBuf.len, &dTran, this);
		int hr = WSASend(s, &wsBuf, 1, &dTran, dFlag, this, nullptr);
		if(0 == hr)
			printf("Send complete: %d byte\n", dTran);

		return hr;
	}
};


struct RemoteHost
{
	SOCKET			scH	;	// socket descriptor
	int				nUse;	// enable

	OVERLAP_EX		olSnd;	// For WSASend
	OVERLAP_EX		olRcv;	// For WSARecv

	RemoteHost()
	{
		scH		= 0;
		nUse	= 1;
		olSnd.dType	= FD_WRITE;
		olRcv.dType	= FD_READ;
	}

	RemoteHost(SOCKET s)
	{
		scH		= s;
		nUse	= 1;
		olSnd.dType	= FD_WRITE;
		olRcv.dType	= FD_READ;
	}

	void Destroy()
	{
		if(scH)
		{
			shutdown(scH, SD_BOTH);
			closesocket(scH);
			scH = 0;
		}

		nUse	= -1;
	}

	void SetUse(int v){	nUse	= v;	}

	int	AsyncSend(char* s, int l)
	{
		olSnd.Reset();
		olSnd.SetBuf(s, l);
		return olSnd.AsyncSnd(scH);
	}

	int	AsyncRecv()
	{
		olRcv.Reset();
		return olRcv.AsyncRcv(scH);
	}
};


SOCKET					g_scLstn = 0;			// listen socket

vector<RemoteHost* >	g_vHost;				// Client list
CRITICAL_SECTION		m_CS;					// critical section

HANDLE	g_hIocp			= nullptr;					// IOCP Handle

DWORD	WINAPI	WorkThread(void*);				// Work 쓰레드
void	EchoMsg(char* s, int l);


void DeleteNotUseHost()
{
	EnterCriticalSection(&m_CS);
	vector<RemoteHost* >::iterator _F = g_vHost.begin();

	for( ; _F != g_vHost.end(); )
	{
		RemoteHost* pCln = (*_F);

		if(pCln && 0 >= pCln->nUse)
		{
			pCln->Destroy();

			delete pCln;
			_F = g_vHost.erase(_F);
			continue;
		}

		++_F;
	}

	LeaveCriticalSection(&m_CS);
}


RemoteHost* FindNotUseHost()
{
	int iSize = (int)g_vHost.size();

	for(int i=0; i<iSize; ++i)
	{
		if(0 >= g_vHost[i]->nUse)
			return g_vHost[i];
	}

	return nullptr;
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
	InitializeCriticalSection(&m_CS);


	WSADATA		wsData={0};
	int			hr =-1;

	printf("Starting Server.\nPort: %s\n", sPt);


	if(0 != WSAStartup(MAKEWORD(2, 2), &wsData))
		return -1;


	g_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(INVALID_SOCKET == g_scLstn)
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


	SOCKADDR_IN	sdLstn = {0};

	sdLstn.sin_family      = AF_INET;
	sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);
	sdLstn.sin_port        = htons(atoi(sPt) );

	hr = bind(g_scLstn, (SOCKADDR*)&sdLstn, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR == hr)
		return -1;

	hr = listen(g_scLstn, SOMAXCONN);
	if(SOCKET_ERROR ==hr)
		return -1;

	// Accept
	while(g_scLstn)
	{
		SOCKET		scCln= 0;
		SOCKADDR_IN	sdCln= {0};
		int			scSize= sizeof(sdCln);


		DeleteNotUseHost();

		scCln = accept(g_scLstn, (SOCKADDR*)&sdCln, &scSize);
		if(INVALID_SOCKET == scCln)
			break;

		printf("New Client: %5d %s\n", (int)scCln, inet_ntoa(sdCln.sin_addr));



		// Event를 Notify 하기 위해 소켓 번호를 접속한 클라이언트에게 전송
		//char sId[128]={0};
		//sprintf(sId, "Connected: %d", (int)scCln);
		//send(scCln, sId, strlen(sId), 0);


		// Nagle off
		int v = 1;
		hr = setsockopt(scCln, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
		if(SOCKET_ERROR == hr)
		{
			hr = WSAGetLastError();
			LogGetLastError(hr);
		}


		// create client intance
		RemoteHost* pCln   = new RemoteHost(scCln);
		ULONG_PTR	pIoKey = (ULONG_PTR)pCln;											// Key is client Object
		HANDLE		hRet   = CreateIoCompletionPort((HANDLE)scCln, g_hIocp, pIoKey, 0);	// <클라이언트 소켓, IOCP, key> binding

		// 비동기 수신 요청
		hr = pCln->AsyncRecv();
		if(SOCKET_ERROR == hr)
		{
			hr =  WSAGetLastError();
			if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
			{
				LogGetLastError(hr);
				pCln->Destroy();
				delete pCln;

				continue;
			}
		}

		// add the client list
		g_vHost.push_back(pCln);
	}


	DeleteAllHost();
	CloseHostSocket();

	WSACleanup();

	DeleteCriticalSection(&m_CS);

	return 0;
}


DWORD WINAPI WorkThread(void* pParam)
{
	int		hr = 0;

	RemoteHost*	pCln	= nullptr;
	OVERLAP_EX*	pOlEx	= nullptr;
	DWORD		dTran	= 0;

	while(0 < g_scLstn)
	{
		pCln	= nullptr;
		pOlEx	= nullptr;
		dTran	= 0;

		hr = GetQueuedCompletionStatus(
						g_hIocp					// Completion Port
					,	&dTran					// 전송 된 바이트 수
					,	(PULONG_PTR)&pCln		// IO Completion Key
					,	(LPOVERLAPPED*)&pOlEx	// OVERLAPPED 구조체
					,	INFINITE
					);

		// IO Failed
		if(0 == hr)
		{
			hr = WSAGetLastError();
			LogGetLastError(hr);

			if(nullptr == pOlEx && nullptr == pCln)
				break;


			if(pOlEx && pCln)
			{
				SOCKET	scHost	= pCln->scH;

				printf("Disconnect Client: %d\n", (int)scHost);
				pCln->SetUse(0);
				continue;
			}

			continue;
		}


		SOCKET	scHost	= pCln->scH;

		if(0 == dTran)
		{
			printf("Disconnect Client: %d\n", (int)scHost);
			pCln->SetUse(0);
			continue;
		}

		// 송신 완료
		if(FD_WRITE == pOlEx->dType)
		{
			printf("Complete sending[%4d]: %d byte\n", (int)scHost, dTran);
		}

		// 수신 완료
		else if(FD_READ == pOlEx->dType)
		{
			printf("Recv from Client[%4d]: %s\n", (int)scHost, pOlEx->csBuf);

			int		iLen=0;
			char	sSnd[MAX_BUF]={0};

			sprintf(sSnd, "%5d> %s", (int)scHost, pOlEx->csBuf);
			iLen = strlen(sSnd);

			EchoMsg(sSnd, iLen);

			// 비동기 수신 요청
			pCln->AsyncRecv();
		}
	}

	CloseHostSocket();

	_endthreadex(0);

	return 0;
}



// echo message
void EchoMsg(char* s, int l)
{
	EnterCriticalSection(&m_CS);

	int	hr = 0;
	int iSize = (int)g_vHost.size();

	RemoteHost* pCln	= nullptr;
	OVERLAP_EX* polSnd	= nullptr;
	SOCKET		scHost	= 0;

	for(int i=0; i<iSize; ++i)
	{
		pCln   = g_vHost[i];
		scHost = pCln->scH;
		polSnd = &pCln->olSnd;

		if(0 >= scHost || 0 >= pCln->nUse)
			continue;

		polSnd->Reset();
		polSnd->SetBuf(s, l);

		hr = polSnd->AsyncSnd(scHost);

		if(SOCKET_ERROR == hr)
		{
			hr =  WSAGetLastError();
			if(WSA_IO_PENDING != hr && WSAEWOULDBLOCK != hr)
			{
				LogGetLastError(hr);
				pCln->nUse = 0;
				continue;
			}
		}
	}

	LeaveCriticalSection(&m_CS);
}


void CloseHostSocket()
{
	shutdown(g_scLstn, SD_BOTH);
	closesocket(g_scLstn);
	g_scLstn = 0;
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


void GetIp(char* s, SOCKET scH)
{
	int size = sizeof(SOCKADDR_IN);
	SOCKADDR_IN sdH ={0};
	getpeername(scH, (SOCKADDR *)&sdH, &size);
	strcpy(s, inet_ntoa(sdH.sin_addr) );
}

