//
//
//
////////////////////////////////////////////////////////////////////////////////

// 1: Console mode, 0: window mode
#define USE_CONSOLE	1


#if (USE_CONSOLE)
  #pragma comment(linker,"/SUBSYSTEM:CONSOLE")
#else
  #pragma comment(linker,"/SUBSYSTEM:WINDOWS")
#endif


#pragma comment(lib, "ws2_32.lib")
#include <windows.h>


// 네트워크 메시지
#define	WM_SOCKET_NOTIFY    (WM_USER+1000)


#include "resource.h"
#include "AsyncSvr.h"

char		sPt[32]="60000";
CAsyncSvr*	g_pSvr = nullptr;


LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(WM_SOCKET_NOTIFY == uMsg)
	{
		if(g_pSvr)
		{
			if(FAILED(g_pSvr->NetProc(wParam, lParam)))
			{
				//SendMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}

		return 0;
	}

	else if(WM_INITDIALOG == uMsg)
	{
		SetWindowText(hWnd, "AsyncServer");

		g_pSvr = new CAsyncSvr;
		if(FAILED(g_pSvr->Create(nullptr, sPt, hWnd, WM_SOCKET_NOTIFY)))
		{
			delete g_pSvr;
			g_pSvr = nullptr;
			return 0;
		}

		return 0;
	}

	else if(WM_CLOSE == uMsg)
	{
		if(g_pSvr)
			delete g_pSvr;

		g_pSvr = nullptr;

		EndDialog(hWnd, 0);
		return 0;
	}

	else if(WM_COMMAND == uMsg)
	{
		WPARAM wHi = HIWORD(wParam);
		WPARAM wLo = LOWORD(wParam);

		if(IDC_EXIT == wLo)
			SendMessage(hWnd, WM_CLOSE, 0, 0);

		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}




#if (USE_CONSOLE)
int main(int argc, char** argv)
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
	DialogBox( (HINSTANCE)GetModuleHandle(nullptr)
			, MAKEINTRESOURCE(IDD_SERVER)
			, nullptr, (DLGPROC)WndProc);

	return 0;
}


