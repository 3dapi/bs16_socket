//
//
////////////////////////////////////////////////////////////////////////////////

#define USE_CS			1
#define USE_MUTEX		0
#define USE_SEMAPHORE	0
#define USE_EVENT		0


#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>
#include <stdio.h>


char	m_sMsg[260]	= {0};								// test message buffer
int		m_Total		= 0;								// test value
FILE*	m_fp		= nullptr;								// for log file


////////////////////////////////////////////////////////////////////////////////
//
#if USE_CS

CRITICAL_SECTION m_CS={0};								// critical section

// work thread.
DWORD WINAPI WorkThread(void*)
{
	DWORD	dId = GetCurrentThreadId();					// thread id

	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
		EnterCriticalSection(&m_CS);					// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "th_%3d		%3d\n",(int)dId, m_Total);
		fprintf(m_fp, m_sMsg);

		LeaveCriticalSection(&m_CS);					// UnLock
	}

	_endthreadex(0);
	return 0;
}


int main()
{
	InitializeCriticalSection(&m_CS);					// create critical section

	m_fp = fopen("log.txt", "wt");

	// test Thread 2개 생성
	int n=0;
	HANDLE hThread[2];
	for(n=0; n<2; ++n)
	{
		hThread[n] = (HANDLE)_beginthreadex(nullptr, 0
					, (unsigned (__stdcall*)(void*))WorkThread
					, nullptr, CREATE_SUSPENDED, nullptr);
	}

	for(n=0; n<2; ++n)
		ResumeThread(hThread[n]);


	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		EnterCriticalSection(&m_CS);					// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "MainProc	%3d\n", m_Total);
		fprintf(m_fp, m_sMsg);

		LeaveCriticalSection(&m_CS);					// UnLock
	}


	for(n=0; n<2; ++n)
	{
		WaitForSingleObject(hThread[n], INFINITE);
		CloseHandle(hThread[n]);
	}

	fclose(m_fp);


	DeleteCriticalSection(&m_CS);						// release critical section

	return 0;
}

#endif // USE_CS
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
#if USE_MUTEX

HANDLE	m_hMx	= nullptr;									// Mutex handle

// work thread.
DWORD WINAPI WorkThread(void*)
{
	DWORD	dId = GetCurrentThreadId();					// thread id

	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
		WaitForSingleObject(m_hMx, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "th_%3d		%3d\n",(int)dId, m_Total);
		fprintf(m_fp, m_sMsg);

		ReleaseMutex(m_hMx);							// UnLock
	}

	_endthreadex(0);
	return 0;
}


int main()
{
	// 동일한 mutex가 있는지 확인
	m_hMx = OpenMutex(SYNCHRONIZE, FALSE, "My thread mutex");
	if(!m_hMx)
		m_hMx = CreateMutex(nullptr, FALSE, "My thread mutex");

	
	// m_hMx = CreateMutex(nullptr, FALSE, nullptr);			// 이름 없는 mutex


	m_fp = fopen("log.txt", "wt");

	// test Thread 2개 생성
	int n=0;
	HANDLE hThread[2];
	for(n=0; n<2; ++n)
	{
		hThread[n] = (HANDLE)_beginthreadex(nullptr, 0
					, (unsigned (__stdcall*)(void*))WorkThread
					, nullptr, CREATE_SUSPENDED, nullptr);
	}

	for(n=0; n<2; ++n)
		ResumeThread(hThread[n]);


	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		WaitForSingleObject(m_hMx, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "MainProc	%3d\n", m_Total);
		fprintf(m_fp, m_sMsg);

		ReleaseMutex(m_hMx);							// UnLock
	}


	for(n=0; n<2; ++n)
	{
		WaitForSingleObject(hThread[n], INFINITE);
		CloseHandle(hThread[n]);
	}

	fclose(m_fp);

	CloseHandle(m_hMx);									// release mutex

	return 0;
}
#endif // USE_MUTEX
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
#if USE_SEMAPHORE

HANDLE	m_hSp	= nullptr;									// Semaphore handle

// work thread.
DWORD WINAPI WorkThread(void*)
{
	DWORD	dId = GetCurrentThreadId();					// thread id

	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		WaitForSingleObject(m_hSp, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "th_%3d		%3d\n",(int)dId, m_Total);
		fprintf(m_fp, m_sMsg);

		ReleaseSemaphore(m_hSp, 1, nullptr);				// UnLock
	}

	_endthreadex(0);
	return 0;
}


int main()
{
	// 동일한 semaphore가 있는지 확인
	m_hSp = OpenSemaphore(SYNCHRONIZE, FALSE, "My thread semaphore");
	if(!m_hSp)
		m_hSp = CreateSemaphore(nullptr, 1, 1, "My thread semaphore");

	
	m_fp = fopen("log.txt", "wt");

	// test Thread 2개 생성
	int n=0;
	HANDLE hThread[2];
	for(n=0; n<2; ++n)
	{
		hThread[n] = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkThread
						, nullptr, CREATE_SUSPENDED, nullptr);
	}

	for(n=0; n<2; ++n)
		ResumeThread(hThread[n]);


	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		WaitForSingleObject(m_hSp, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "MainProc	%3d\n", m_Total);
		fprintf(m_fp, m_sMsg);

		ReleaseSemaphore(m_hSp, 1, nullptr);				// UnLock
	}


	for(n=0; n<2; ++n)
	{
		WaitForSingleObject(hThread[n], INFINITE);
		CloseHandle(hThread[n]);
	}

	fclose(m_fp);

	CloseHandle(m_hSp);									// release semaphore

	return 0;
}

#endif // USE_SEMAPHORE
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
#if USE_EVENT

HANDLE	m_hEv	= nullptr;									// Event handle

// work thread.
DWORD WINAPI WorkThread(void*)
{
	DWORD	dId = GetCurrentThreadId();					// thread id

	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		WaitForSingleObject(m_hEv, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "th_%3d		%3d\n",(int)dId, m_Total);
		fprintf(m_fp, m_sMsg);

		SetEvent(m_hEv);								// UnLock
	}

	_endthreadex(0);
	return 0;
}


int main()
{
	// 동일한 event가 있는지 확인
	m_hEv = OpenEvent(SYNCHRONIZE, FALSE, "My thread event");
	if(!m_hEv)
		m_hEv = CreateEvent(nullptr, FALSE, TRUE, "My thread event");	// auto reset and signaled

	
	m_fp = fopen("log.txt", "wt");

	// test Thread 2개 생성
	int n=0;
	HANDLE hThread[2];
	for(n=0; n<2; ++n)
	{
		hThread[n] = (HANDLE)_beginthreadex(nullptr, 0
						, (unsigned (__stdcall*)(void*))WorkThread
						, nullptr, CREATE_SUSPENDED, nullptr);
	}

	for(n=0; n<2; ++n)
		ResumeThread(hThread[n]);


	int		nLimit=0;									// while limit

	while(5000>nLimit++)
	{
	
		WaitForSingleObject(m_hEv, INFINITE);			// Lock: 다른 프로세스는 기다림

		++m_Total;										// 명령문 실행: global test  value를 변경한다.

		sprintf(m_sMsg, "MainProc	%3d\n", m_Total);
		fprintf(m_fp, m_sMsg);

		SetEvent(m_hEv);								// UnLock
	}


	for(n=0; n<2; ++n)
	{
		WaitForSingleObject(hThread[n], INFINITE);
		CloseHandle(hThread[n]);
	}

	fclose(m_fp);

	CloseHandle(m_hEv);									// release event

	return 0;
}

#endif // USE_EVENT




