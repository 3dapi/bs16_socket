//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <set>
using namespace std;

#include <windows.h>
#include <process.h>

#include <stdio.h>

#define		MAX_EVENT	64

HANDLE		m_vEvnt[MAX_EVENT] = {0};				// Event Handle List


// work thread.
DWORD WINAPI WorkThread(void*)
{
	int		nLimit= 0;	// while limit value
	int		hr = 0;

	int		nCnt = MAX_EVENT;
	int		nIdx = 0;
	int		n;

	while(1)
	{
		// find the signaled events
		// return [WAIT_OBJECT_0, WAIT_OBJECT_0 + nCnt - 1]

		nIdx = WaitForMultipleObjects(nCnt, m_vEvnt, FALSE, INFINITE);

		// setup the start index [0, nCnt)
		nIdx -= WAIT_OBJECT_0;

		// error
		if( !(0 <= nIdx && nIdx <nCnt) )
			break;

		printf("Work --------------------------------------------\n");

		for(n=nIdx; n<nCnt; ++n)	// find all signaled event
		{
			// 이벤트 리스트의 인덱스를 찾기위해 WFMO 함수를 한번더 호출
			// 따라서 Manual-reset mode로 이벤트를 만들어야 함.
			hr = WaitForMultipleObjects(1, &m_vEvnt[n], TRUE, 0);

			// Error or Time Out
			if(WAIT_OBJECT_0 != hr)
				continue;

			// Non-signal for Manual Reset Mode
			ResetEvent(m_vEvnt[n]);

			printf("Wake up the Event[%2d]: 0x%X\n",n, m_vEvnt[n]);
		}
	}

	_endthreadex(0);

	return 0;
}


int main()
{
	int i = 0;


	// Event Create: manual-reset event object.
	for(i=0; i<MAX_EVENT; ++i)
		m_vEvnt[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr);


	// create event
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0
							, (unsigned (__stdcall*)(void*))WorkThread
							, nullptr, 0, nullptr);


	DWORD dCur = GetTickCount();
	DWORD dLst = dCur;

	int nLimit = 0;

	int	n = 0;
	set<int > evlst;

	while(nLimit<10)
	{
		dCur = GetTickCount();
		if(dCur- dLst >= 1000)
		{
			++nLimit;
			dLst = dCur;

			n = rand()%15;

			for(i=0; i<n; ++i)
			{
				int idx = rand()%8;
				evlst.insert(idx);
			}

			printf("Main --------------------------------------------\n");

			set<int >::iterator _F = evlst.begin();
			for( ; _F != evlst.end(); ++_F)
			{
				printf("Set Event: %2d\n", (*_F));
				SetEvent(m_vEvnt[(*_F)]);
			}

			evlst.clear();
		}
	}


	WaitForSingleObject(hThread, 15*1000);
	CloseHandle(hThread);


	// close events
	for(i=0; i<MAX_EVENT; ++i)
		CloseHandle(m_vEvnt[i]);


	return 0;
}


