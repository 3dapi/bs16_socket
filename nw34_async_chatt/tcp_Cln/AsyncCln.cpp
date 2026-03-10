//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4996)

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AsyncCln.h"


// display to List
void DisplayList(HWND hWnd, char* sMsg)
{
	#define IDC_LSTCHAT 1005
	HWND hList = GetDlgItem(hWnd, IDC_LSTCHAT);
	SendMessage(hList,LB_ADDSTRING,0,(LPARAM)sMsg);
	printf(sMsg);
	printf("\n");
}



CAsyncCln::CAsyncCln()
{
	memset(&m_sPt, 0, sizeof(m_sPt));
	memset(&m_sIp, 0, sizeof(m_sIp));

	m_hWnd		= nullptr;
	m_wmNotify	= nullptr;

	memset(&m_wsData, 0, sizeof(WSADATA));

	m_scHost	= 0;
	memset(&m_sdHost, 0, sizeof(SOCKADDR_IN));

	m_hThSnd	= nullptr;
	memset(&m_CS, 0, sizeof(CRITICAL_SECTION));

	m_nSnd		= 0;
	memset(m_sSnd, 0, MAX_BUF_SND);

	m_nRcv		= 0;
	memset(m_sRcv, 0, MAX_BUF_RCV+4);
}



CAsyncCln::~CAsyncCln()
{
	Destroy();
}


void CAsyncCln::Destroy()
{
	DisConnect();
	WSACleanup();

	DWORD dExit = 0;
	GetExitCodeThread(m_hThSnd, &dExit);
	TerminateThread(m_hThSnd, dExit);
	CloseHandle(m_hThSnd);


	DeleteCriticalSection(&m_CS);
}


int	CAsyncCln::Create(HWND hWnd, UINT wm)
{
	int hr = 0;

	if(nullptr == hWnd || 0 == wm)
		return -1;


	m_hWnd		= hWnd;
	m_wmNotify	= wm;


	InitializeCriticalSection(&m_CS);


	hr = WSAStartup(MAKEWORD(2,2), &m_wsData);
	if(0 != hr)
		return -1;


	//데이터 송신용 Thread 생성. 일시 중지 상태
	m_hThSnd = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))CAsyncCln::WorkSnd
						, this
						, CREATE_SUSPENDED
						, nullptr);

	return 0;
}

int	CAsyncCln::Connect(char* sIp, char* sPt)
{
	int hr;

	if(nullptr == sIp || nullptr == sPt || nullptr == m_hWnd)
		return -1;

	DisConnect();

	strcpy(m_sIp, sIp);
	strcpy(m_sPt, sPt);

	m_scHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == m_scHost)
		return 0;


	hr = WSAAsyncSelect(m_scHost, m_hWnd, m_wmNotify
						, FD_CONNECT|FD_READ|FD_WRITE|FD_CLOSE);

	m_sdHost.sin_family      = AF_INET;
	m_sdHost.sin_addr.s_addr = inet_addr(m_sIp);
	m_sdHost.sin_port        = htons( atoi(m_sPt) );

	hr = connect(m_scHost, (SOCKADDR*)&m_sdHost, sizeof(SOCKADDR_IN));
	if(SOCKET_ERROR == hr)
	{
		hr = WSAGetLastError();

		if(WSAEWOULDBLOCK != hr)
			return 0;
	}

	return 0;
}

int CAsyncCln::DisConnect()
{
	if(!m_scHost)
		return 0;


	shutdown(m_scHost, SD_BOTH);
	closesocket(m_scHost);
	m_scHost = 0;

	DisplayList(m_hWnd, "Disconnected.\n");

	return 0;
}


LRESULT CAsyncCln::NetProc(WPARAM wParam, LPARAM lParam)
{
	int		hr = 0;

	SOCKET	scHost = (SOCKET)wParam;

	DWORD   dError  = WSAGETSELECTERROR(lParam);
    DWORD   dEvent  = WSAGETSELECTEVENT(lParam);

	if(dError)
	{
		if(FD_CONNECT == dEvent)
			DisplayList(m_hWnd, "Connection failed\n");

		DisConnect();
		return -1;
	}


	if(FD_WRITE == dEvent)
	{
	}
	else if(FD_CONNECT == dEvent)
	{
		DisplayList(m_hWnd, "Connection succeeded\n");
		return 0;
	}
	else if(FD_CLOSE == dEvent)
	{
		DisConnect();
		return 0;
	}
	else if(FD_READ == dEvent)
	{
		hr = recv(scHost, m_sRcv, MAX_BUF_RCV, 0);

		if(0 > hr)
		{
			hr = WSAGetLastError();
			if(WSAEWOULDBLOCK == hr)
				return 0;

			LogGetLastError(hr);
			DisConnect();
			return -1;
		}
		else if(0 == hr)
		{
			DisConnect();
			return 0;
		}

		else
		{
			m_nRcv = hr;
			memset(m_sRcv+m_nRcv, 0, MAX_BUF_RCV -m_nRcv);
			DisplayList(m_hWnd, m_sRcv);
		}
	}

	return 0;
}



int	CAsyncCln::SendMsg(char* s, int len)
{
	if(!m_scHost || 0 >= len || nullptr == s)
		return -1;

	EnterCriticalSection(&m_CS);

	m_nSnd = len;
	memcpy(m_sSnd, s, len);
	LeaveCriticalSection(&m_CS);

	ResumeThread(m_hThSnd);

	return 0;
}


void CAsyncCln::SendBuf()
{
	EnterCriticalSection(&m_CS);

	int	iLen = m_nSnd;
	int iSnd = 0;
	int	iTot = 0;
	int hr   = 0;

	if(0 < iLen && 0 < m_scHost)
	{
		while(iTot<iLen)
		{
			char* p = m_sSnd + iTot;

			iSnd = send(m_scHost, p, iLen-iTot, 0);

			if(SOCKET_ERROR == iSnd)
			{
				iSnd = WSAGetLastError();
				if(WSAEWOULDBLOCK == iSnd)
					continue;

				// 전송 error.
				LogGetLastError(hr);
				break;
			}

			iTot += iSnd;
		}

		m_nSnd = 0;

		if(FAILED(hr))
			DisConnect();
	}

	LeaveCriticalSection(&m_CS);
}


DWORD WINAPI CAsyncCln::WorkSnd(void *pParam)
{
	CAsyncCln* pNet = (CAsyncCln*)pParam;

	while(1)
	{
		pNet->SendBuf();

		HANDLE hThread = GetCurrentThread();
		SuspendThread(hThread);
	}

	_endthreadex(0);
	return 0;
}


void CAsyncCln::LogGetLastError(int hr)
{
	char* lpMsgBuf;
	FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_IGNORE_INSERTS
				, nullptr, hr, 0, (LPSTR)&lpMsgBuf, 0, nullptr );

	printf( "%s\n", lpMsgBuf);
	LocalFree( lpMsgBuf );
}

