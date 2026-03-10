//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>

#include <stdio.h>


// test thread.
DWORD WINAPI ThreadProc(void* pParam)
{
	//HANDLE	hCur = GetCurrentThread();
	//DWORD	dCur = GetCurrentThreadId();
	//dCur = GetThreadId(hCur);


	int nCnt=0;

	while(++nCnt<20)
	{
		Sleep(200);

		printf("Thread Proc: %d\n", nCnt);
	}

	_endthreadex(0);

	return 0;
}


int main()
{
	// create thread
	HANDLE	hThread = nullptr;
	DWORD	dThread = 0;
	
	hThread = (HANDLE)_beginthreadex(nullptr, 0
								, (unsigned (__stdcall*)(void*))ThreadProc
								, nullptr, 0, (unsigned*)&dThread);
	//hThread = (HANDLE)CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);


	//if(dThread != GetThreadId(hThread))
	//	return 0;


	int nCnt=0;
	while(++nCnt<20)
	{
		Sleep(200);
		printf("Main Proc: %d\n", nCnt);
	}

	WaitForSingleObject(hThread, INFINITE);


	// Thread 강제 종료
	//{
	//	DWORD	dExit=0;
	//	int hr = GetExitCodeThread(hThread, &dExit);
	//
	//	if(0 != hr && STILL_ACTIVE == dExit)
	//		TerminateThread(hThread, dExit);
	//}

	::CloseHandle(hThread);

	return 0;
}

