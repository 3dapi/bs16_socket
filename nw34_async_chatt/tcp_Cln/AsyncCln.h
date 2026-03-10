//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _AsyncCln_H_
#define _AsyncCln_H_


class CAsyncCln
{
public:
	enum
	{
		MAX_BUF_SND = 8192,
		MAX_BUF_RCV = 8192,
	};

protected:
	char				m_sPt[16];
	char				m_sIp[64];

	HWND				m_hWnd	;
	UINT				m_wmNotify;

	WSADATA				m_wsData;
	SOCKET				m_scHost;
	SOCKADDR_IN			m_sdHost;

	HANDLE				m_hThSnd;						// Send용 쓰레드 핸들
	CRITICAL_SECTION	m_CS;							// 임계영역: 동기화에 필요

	int					m_nSnd;							// recorded 
	char				m_sSnd[MAX_BUF_SND  ];			// send buffer
	int					m_nRcv;							// received
	char				m_sRcv[MAX_BUF_RCV+4];			// recv buffer

public:
	CAsyncCln();
	virtual ~CAsyncCln();

	INT		Create(HWND hWnd, UINT wm);
	void	Destroy();

	INT		Connect(char* sIp, char* sPt);
	INT		DisConnect();
	INT		SendMsg(char* s, int len);

	void	SendBuf();

	LRESULT	NetProc(WPARAM, LPARAM);					// Network Message Procedure
	static DWORD WINAPI WorkSnd(void *pParam);			// Send용 쓰레드

protected:
	void	LogGetLastError(int hr);
};

#endif

