//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <vector>
using namespace std;

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AsyncSvr.h"


// display to List
void DisplayList(HWND hWnd, char* sMsg)
{
	#define IDC_LIST   1003
	HWND hList = GetDlgItem(hWnd, IDC_LIST);
	SendMessage(hList,LB_ADDSTRING,0,(LPARAM)sMsg);
	printf(sMsg);
}

CAsyncSvr::RemoteHost::RemoteHost()
{
	scH		= 0;
	memset(&sdH, 0, sizeof(SOCKADDR_IN));

	nBuf	= 0;
	memset(sBuf, 0, MAX_BUF_SND);
}

CAsyncSvr::RemoteHost::RemoteHost(SOCKET s, SOCKADDR_IN* d)
{
	scH = s;
	memcpy(&sdH, &d, sizeof(SOCKADDR_IN) );

	nBuf	= 0;
	memset(sBuf, 0, MAX_BUF_SND);
}

CAsyncSvr::RemoteHost::~RemoteHost()
{
	Close();
}

void CAsyncSvr::RemoteHost::Close()
{
	if(scH)
	{
		shutdown(scH, SD_BOTH);
		closesocket(scH);
	}

	scH		= 0;
	memset(&sdH, 0, sizeof(SOCKADDR_IN));

	nBuf	= 0;
	memset(sBuf, 0, MAX_BUF_SND);
}

void CAsyncSvr::RemoteHost::SetupBuf(char* s, int len)
{
	nBuf = len;
	memcpy(sBuf, s, nBuf);
	memset(sBuf+nBuf, 0, MAX_BUF_SND-len);
}



CAsyncSvr::CAsyncSvr()
{
	memset(m_sIp, 0, sizeof(m_sIp));
	memset(m_sPt, 0, sizeof(m_sPt));

	m_hWnd		= nullptr;
	m_wmNotify	= nullptr;

	memset(&m_wsData, 0, 	sizeof(WSADATA));

	m_scLstn = 0;
	memset(&m_sdLstn, 0, sizeof(SOCKADDR_IN));

	m_hThSnd = nullptr;
	memset(&m_CS, 0, sizeof(CRITICAL_SECTION));
}


CAsyncSvr::~CAsyncSvr()
{
	Destroy();
}


void CAsyncSvr::Destroy()
{
	int hr;

	if(0 == m_scLstn)
		return;

	DeleteAllHost();

	shutdown(m_scLstn, SD_BOTH);
	closesocket(m_scLstn);
	hr = WSACleanup();

	m_scLstn = 0;


	DWORD dExit = 0;
	GetExitCodeThread(m_hThSnd, &dExit);
	TerminateThread(m_hThSnd, dExit);
	CloseHandle(m_hThSnd);

	DeleteCriticalSection(&m_CS);
}


int CAsyncSvr::Create(char* sIp, char* sPt, HWND hWnd, UINT wm)
{
	int hr;

	if(!sPt || nullptr == hWnd || 0 == wm)
		return -1;

	if(sIp)
		strcpy(m_sIp, sIp);

	strcpy(m_sPt, sPt);

	m_hWnd		= hWnd;
	m_wmNotify	= wm;


	InitializeCriticalSection(&m_CS);



	hr = WSAStartup(MAKEWORD(2,2), &m_wsData);

	m_scLstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == m_scLstn)
		return 0;

	m_sdLstn.sin_family              = AF_INET;

	if(sIp) m_sdLstn.sin_addr.s_addr = inet_addr(m_sIp);
	else    m_sdLstn.sin_addr.s_addr = htonl(INADDR_ANY);

	m_sdLstn.sin_port                = htons( atoi(m_sPt) );

	hr = bind(m_scLstn, (SOCKADDR*)&m_sdLstn, sizeof(SOCKADDR_IN));

	if(SOCKET_ERROR == hr)
		return 0;



	//데이터 송신용 Thread 생성. 대기 상태
	m_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))CAsyncSvr::WorkSnd
						, this
						, CREATE_SUSPENDED
						, nullptr);


	// AsycSelect에 연결. 소켓은 자동으로 Non-blocking으로 전환
	hr = WSAAsyncSelect(m_scLstn, m_hWnd, m_wmNotify, FD_ACCEPT|FD_CLOSE);

	hr = listen(m_scLstn, SOMAXCONN);
	if(SOCKET_ERROR ==hr)
		return -1;

	return 0;
}



LRESULT CAsyncSvr::NetProc(WPARAM wParam, LPARAM lParam)
{
	SOCKET scHost = (SOCKET)wParam;

	DWORD   dError  = WSAGETSELECTERROR(lParam);
    DWORD   dEvent  = WSAGETSELECTEVENT(lParam);

	if(dError)
	{
		if(m_scLstn == scHost)
			return -1;

		else
		{
			char sMsg[128]={0};
			sprintf(sMsg, "Disconnect client:%d\n", scHost);
			DisplayList(m_hWnd, sMsg);

			DeleteHost(scHost);
		}

		return 0;
	}


	if(FD_WRITE == dEvent)
	{
		printf("FD write: %d\n", (int)scHost);
	}
	else if(FD_ACCEPT == dEvent)
	{
		SOCKET		scCln = 0;
		SOCKADDR_IN sdCln = {0};
		int size = sizeof(sdCln);

		scCln = accept(scHost, (SOCKADDR*)&sdCln, &size);

		// setup to asyncselect
		WSAAsyncSelect(scCln, m_hWnd
							, m_wmNotify
							, FD_READ|FD_WRITE|FD_CLOSE);

		// add client list
		m_rmCln.push_back(new RemoteHost(scCln, &sdCln));


		char sMsg[MAX_BUF_SND] = {0};
		sprintf(sMsg, "Connected: %d\n", scCln);

		// display
		DisplayList(m_hWnd, sMsg);


		// send to client the logon message
		int iSnd = 0;
		int iLen = strlen(sMsg);
		send(scCln, sMsg + iSnd, iLen, 0);
	}
	else if(FD_CLOSE == dEvent)
	{
		char sMsg[128]={0};
		sprintf(sMsg, "Disconnect client:%d\n", scHost);
		DisplayList(m_hWnd, sMsg);

		DeleteHost(scHost);
	}
	else if(FD_READ == dEvent)
	{
		char sbuf[MAX_BUF_RCV+4]={0};
		int hr;

		hr = recv(scHost, sbuf, MAX_BUF_RCV, 0);


		if(SOCKET_ERROR == hr)
		{
			hr = WSAGetLastError();
			if(WSAEWOULDBLOCK != hr)
			{
				char sMsg[128]={0};
				sprintf(sMsg, "Disconnect client:%d\n", scHost);
				DisplayList(m_hWnd, sMsg);

				DeleteHost(scHost);
			}
		}
		else if(0 == hr)
		{
			char sMsg[128]={0};
			sprintf(sMsg, "Disconnect client:%d\n", scHost);
			DisplayList(m_hWnd, sMsg);

			DeleteHost(scHost);
		}

		else if(0<hr)
		{
			char sSnd[MAX_BUF_SND]={0};

			sprintf(sSnd, "%d > %s\n", (int)scHost, sbuf);
			printf("%s\n", sSnd);

			// send the message to all client
			EchoMsg(sSnd, strlen(sSnd) );

			//for(int i=0; i< (int)m_rmCln.size(); ++i)
			//{
			//	RemoteHost* pCln = m_rmCln[i];
			//	send(pCln->scH, sbuf, hr, 0);
			//}
		}
	}

	return 0;
}


void CAsyncSvr::EchoMsg(char* s, int iLen)
{
	EnterCriticalSection(&m_CS);

	int iSize = (int)m_rmCln.size();

	for(int i=0; i<iSize; ++i)
	{
		RemoteHost* pCln = m_rmCln[i];

		pCln->SetupBuf(s, iLen);
	}

	LeaveCriticalSection(&m_CS);

	ResumeThread(m_hThSnd);
}


void CAsyncSvr::SendBuf()
{
	EnterCriticalSection(&m_CS);

	int iSize = (int)m_rmCln.size();

	for(int i=0; i<iSize; ++i)
	{
		RemoteHost* pCln = m_rmCln[i];

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
}



DWORD WINAPI CAsyncSvr::WorkSnd(void *pParam)
{
	CAsyncSvr* pNet = (CAsyncSvr*)pParam;

	while(1)
	{
		pNet->SendBuf();

		HANDLE hThread = GetCurrentThread();
		SuspendThread(hThread);
	}

	_endthreadex(0);
	return 0;
}


CAsyncSvr::RemoteHost* CAsyncSvr::FindHost(SOCKET scH)
{
	int iSize = (int)m_rmCln.size();

	for(int i=0; i<iSize; ++i)
	{
		if(scH == m_rmCln[i]->scH)
			return m_rmCln[i];
	}

	return nullptr;
}


void CAsyncSvr::DeleteHost(SOCKET scH)
{
	int iSize = (int)m_rmCln.size();

	for(int i=0; i<iSize; ++i)
	{
		if(scH == m_rmCln[i]->scH)
		{
			delete m_rmCln[i];
			m_rmCln.erase( m_rmCln.begin() + i);
			return;
		}
	}

}


void CAsyncSvr::DeleteNotUseHost()
{
	vector<CAsyncSvr::RemoteHost* >::iterator _F = m_rmCln.begin();

	for( ; _F != m_rmCln.end(); )
	{
		if(0 >= (*_F)->scH)
		{
			delete (*_F);
			_F = m_rmCln.erase(_F);
			continue;
		}

		++_F;
	}
}


void CAsyncSvr::DeleteAllHost()
{
	int iSize = (int)m_rmCln.size();

	for(int i=0; i<iSize; ++i)
		delete m_rmCln[i];

	m_rmCln.clear();
}



void CAsyncSvr::LogGetLastError(int hr)
{
	char* lpMsgBuf;
	FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_IGNORE_INSERTS
				, nullptr, hr, 0, (LPSTR)&lpMsgBuf, 0, nullptr );

	printf( "%s\n", lpMsgBuf);
	LocalFree( lpMsgBuf );
}

