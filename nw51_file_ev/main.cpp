//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define	MAX_BUF		16

void LogGetLastError(int hr);


HANDLE			g_hFile = nullptr;				// File Handle
DWORD			g_TotalSize= 0;				// total file size
DWORD			g_TotalRead= 0;				// total read

// for read	buffer
OVERLAPPED		g_rOL   ={0};
char			g_rBuf[MAX_BUF+4]={0};		// Io Completion Buffer for receive

DWORD	WINAPI	WorkThread(void*);			// Work 쓰레드


int main()
{
	int hr = 0;

	g_hFile = CreateFile("tiger.txt"
						, GENERIC_READ | GENERIC_WRITE
						, 0
						, nullptr
						, OPEN_ALWAYS
						, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED
						, nullptr);

	if(g_hFile == INVALID_HANDLE_VALUE)
		return -1;

	if(0 == (g_TotalSize = GetFileSize(g_hFile, nullptr)) )
		return -1;


	g_rOL.hEvent =CreateEvent(nullptr, FALSE, FALSE, nullptr);
	

	// Work 쓰레드 생성
	HANDLE hWork = (HANDLE)_beginthreadex(nullptr, 0
					, (unsigned (__stdcall*)(void*))WorkThread
					, nullptr, 0, nullptr);


	DWORD	dTran = 0;


	memset(g_rBuf, 0, MAX_BUF);										// buffer clear

	hr= ReadFile(g_hFile, g_rBuf, MAX_BUF, &dTran, &g_rOL);			// 비동기 read 요청
	if(ERROR_SUCCESS == hr)
	{
		Sleep(10);
		hr = GetLastError();
		if(ERROR_IO_PENDING != hr)
		{
			LogGetLastError(hr);
			return -1;
		}
	}

	while(g_hFile)
	{
		Sleep(100);
	}

	if(g_hFile)
		CloseHandle(g_hFile);

	CloseHandle(g_rOL.hEvent);

	return 0;
}


DWORD WINAPI WorkThread(void* pParam)
{
	int		hr	 = 0;
	DWORD	dTran= 0;

	while(1)
	{
		hr = WaitForSingleObject(g_rOL.hEvent, INFINITE);			// 입출력 완료를 기다림
		if(WAIT_FAILED == hr)
		{
			hr = GetLastError();
			LogGetLastError(hr);
			goto END;
		}

		hr = GetOverlappedResult(g_hFile, &g_rOL, &dTran, FALSE);	// 완료 결과 해석. 전송된 길이 얻기


		printf(g_rBuf);												// 버퍼 출력

		g_TotalRead += dTran;										// 읽어온 전체 길이 누적
		if(g_TotalSize <= g_TotalRead)								// 파일 길이보다 크거나 같으면 종료
			break;


		g_rOL.Offset += dTran;										// offset을 다시 설정
		memset(g_rBuf, 0, MAX_BUF);									// buffer를 초기화


		hr= ReadFile(g_hFile, g_rBuf, MAX_BUF, &dTran, &g_rOL);		// 비동기 read 요청
		if(ERROR_SUCCESS == hr)
		{
			Sleep(10);
			hr = GetLastError();
			if(ERROR_IO_PENDING != hr)
			{
				LogGetLastError(hr);
				return -1;
			}
		}
	}

END:
	CloseHandle(g_hFile);
	g_hFile = 0;

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

