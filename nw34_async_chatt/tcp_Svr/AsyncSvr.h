//
//
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _AsyncSvr_H_
#define _AsyncSvr_H_


#include <vector>
using namespace std;


class CAsyncSvr
{
public:
	enum
	{
		MAX_BUF_SND = 8192,
		MAX_BUF_RCV = 8192,
	};

	struct RemoteHost
	{
		SOCKET		scH;								// socket
		SOCKADDR_IN sdH;								// address
		int			nBuf;								// recorded byte
		char		sBuf[MAX_BUF_SND];					// Send buffer

		RemoteHost();
		RemoteHost(SOCKET, SOCKADDR_IN*);
		~RemoteHost();

		void Close();
		void SetupBuf(char* s, int len);
	};

protected:
	char				m_sIp[64];
	char				m_sPt[16];

	HWND				m_hWnd	;
	UINT				m_wmNotify;
	WSADATA				m_wsData;
	SOCKET				m_scLstn;
	SOCKADDR_IN			m_sdLstn;

	vector<RemoteHost*>	m_rmCln;						// client list

	HANDLE				m_hThSnd;						// Send용 쓰레드 핸들
	CRITICAL_SECTION	m_CS;							// 임계영역: 동기화에 필요


public:
	CAsyncSvr();
	virtual ~CAsyncSvr();

	INT		Create(char* sIp, char* sPt, HWND hWnd, UINT wm);
	void	Destroy();

	void	SendBuf();									// send data

	LRESULT	NetProc(WPARAM, LPARAM);					// Network Message Procedure
	static DWORD WINAPI WorkSnd(void *pParam);			// Send용 쓰레드

protected:
	RemoteHost* FindHost(SOCKET);
	void	 DeleteHost(SOCKET);
	void	 DeleteNotUseHost();
	void	 DeleteAllHost();

	void	LogGetLastError(int hr);

	void	EchoMsg(char* s, int iLen);
};

#endif

