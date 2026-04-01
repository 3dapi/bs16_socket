// NetworkGui.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "NetworkGui.h"

#define MAX_LOADSTRING 100

INT_PTR CALLBACK ClientDlgProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR , int nCmdShow)
{
    InitCommonControls();

	return (int)DialogBox(hInst,MAKEINTRESOURCE(IDD_CLIENT), nullptr, ClientDlgProc);
}

INT_PTR CALLBACK ClientDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
		case WM_INITDIALOG:
		{
			char ip[64] ="127.0.0.1";
			char pt[16] = "60000";

			SetDlgItemText(hDlg, IDC_IP, ip);
			SetDlgItemText(hDlg, IDC_PORT, pt);
			return (INT_PTR)TRUE;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case BTN_CONNECT:
					MessageBox(hDlg, "Connect 클릭", "Info", MB_OK);
					break;

				case BTN_CHAT:
					MessageBox(hDlg, "Chat 클릭", "Info", MB_OK);
					break;

				case BTN_CLOSE:
				case BTN_EXIT:
					EndDialog(hDlg, 0);
					return (INT_PTR)TRUE;
			}
			break;
		}
		case WM_CLOSE:
		{
			EndDialog(hDlg, 0);
			return (INT_PTR)TRUE;
		}
	}
	return (INT_PTR)FALSE;
}
