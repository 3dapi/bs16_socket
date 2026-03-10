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
#define WM_SOCKET_NOTIFY 0x0373


#include "resource.h"
#include "AsyncCln.h"

char		sPt[32]="60000";
char		sIp[64]="127.0.0.1";
CAsyncCln*	g_pCln = nullptr;


LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(WM_SOCKET_NOTIFY == uMsg)
	{
		if(g_pCln)
		{
			if(FAILED(g_pCln->NetProc(wParam, lParam)))
			{
				//SendMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}

		return 0;
	}

	else if(WM_COMMAND == uMsg && g_pCln)
	{
		WPARAM wHi = HIWORD(wParam);
		WPARAM wLo = LOWORD(wParam);

		if(IDC_DISCON == wLo)
			g_pCln->DisConnect();

		if(IDC_CON == wLo)
		{
			GetDlgItemText(hWnd, IDC_IP, sIp, 64);
			GetDlgItemText(hWnd, IDC_PORT, sPt, 32);

			g_pCln->Connect(sIp, sPt);
		}
		else if(IDC_SEND == wLo)
		{
			char sbuf[1024]={0};
			GetDlgItemText(hWnd, IDC_CHAT, sbuf, 1024);

			g_pCln->SendMsg(sbuf, strlen(sbuf));

			SetDlgItemText(hWnd, IDC_CHAT, "");
		}

		return 0;
	}

	else if(WM_INITDIALOG == uMsg)
	{
		SetWindowText(hWnd, "AsyncClient");

		SetDlgItemText(hWnd, IDC_IP, sIp);
		SetDlgItemText(hWnd, IDC_PORT, sPt);


		g_pCln = new CAsyncCln;
		if(FAILED(g_pCln->Create(hWnd, WM_SOCKET_NOTIFY)))
		{
			delete g_pCln;
			g_pCln = nullptr;
			return 0;
		}

		return 0;
	}
	else if(WM_CLOSE == uMsg)
	{
		if(g_pCln)
			delete g_pCln;

		g_pCln = nullptr;

		EndDialog(hWnd, 0);
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
			, MAKEINTRESOURCE(IDD_CHAT)
			, nullptr, (DLGPROC)WndProc);

	return 0;
}


