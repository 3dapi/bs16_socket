//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4996)

#include <windows.h>
#include <process.h>

#include <stdio.h>

int     m_Total = 0;            // test value
HANDLE  m_hEv   = 0;            // Event Handle


// work thread.
DWORD WINAPI WorkThread(void*)
{
    DWORD   dId = GetCurrentThreadId(); // thread id

    int     nLimit= 0;                  // while limit

    while(500 > nLimit++)
    {
    
        // wait till event to signaled.
        WaitForSingleObject(m_hEv, INFINITE);       // same to WaitForMultipleObjects(1, &m_hEv, FALSE, INFINITE);
        ResetEvent(m_hEv);                          // Non-signal for Manual Reset Mode

        ++m_Total;

        printf("%5d %3d\n",(int)dId, m_Total);
    }

    _endthreadex(0);

    return 0;
}


int main()
{
    // Event Create
    m_hEv = CreateEvent(
            nullptr
        ,   TRUE    // ManualReset : TRUE ==> WaitForSingleObject 후에도 계속 Signaled.
        ,   FALSE   // InitialState: TRUE ==> Signaled
        ,   nullptr
        );

    // WSACreateEvent:: same to CreateEvent(nullptr, TRUE, FALSE, nullptr);


    HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0
                            , (unsigned (__stdcall*)(void*))WorkThread
                            , nullptr, 0, nullptr);


    DWORD dCur = GetTickCount();
    DWORD dLst = dCur;

    int nLimit = 0;

    while(nLimit<50)
    {
        dCur = GetTickCount();
        if(dCur- dLst >= 500)
        {
            ++nLimit;
            dLst = dCur;

            SetEvent(m_hEv);
        }
    }


    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    CloseHandle(m_hEv);

    return 0;
}


