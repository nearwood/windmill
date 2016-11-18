/* windmill
 * Copyright 2016 Nick Earwood <nearwood@gmail.com>
 */

/*
 windmill is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 windmill is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with windmill.  If not, see <http://www.gnu.org/licenses/>.
 */

/* windmill - save/restore window positions via the tray
 * Phase 0: ✔  Basic Win32 tray app
 * Phase 1: WIP Save window positions manually via tray menu (to registry)
 * Phase 2:     Restore window positions manually via tray menu (from registry)
 * Phase 3:     32/64-bit compatibility
 * Phase 4:     Detect specific dock-associated hardware and trigger based on that, or is there a Windows Power/Dock API?
 * Phase 5:     Allow configuration of trigger hardware
 * Phase C:     Cmake for possible cross-platform support?
 */

#include "stdafx.h"
#include "windmill.h"

//kernel32.lib;user32.lib;gdi32.lib;winspool.lib;comdlg32.lib;advapi32.lib;shell32.lib;ole32.lib;oleaut32.lib;uuid.lib;odbc32.lib;odbccp32.lib;%(AdditionalDependencies)

#define MAX_LOADSTRING 100
#define ID_TRAY_ICON   1
#define SWM_TRAYMSG    WM_APP //Message ID range: WM_APP through 0xBFFF
#define SWM_SAVE       WM_APP + 1
#define SWM_RESTORE    WM_APP + 2
#define SWM_SHOW       WM_APP + 3
#define SWM_HIDE       WM_APP + 4
#define SWM_EXIT       WM_APP + 5

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
NOTIFYICONDATA niData;
LPWSTR lpcsKey = _T("SOFTWARE\\Windmill");
HKEY hKey;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINDMILL, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDMILL));
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDMILL));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDMILL);
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

ULONGLONG GetDllVersion(LPCTSTR lpszDllName)
{
	ULONGLONG ullVersion = 0;
	HINSTANCE hinstDll;
	hinstDll = LoadLibrary(lpszDllName);

	if (hinstDll)
	{
		DLLGETVERSIONPROC pDllGetVersion;
		pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
		if (pDllGetVersion)
		{
			DLLVERSIONINFO dvi;
			HRESULT hr;
			ZeroMemory(&dvi, sizeof(dvi));
			dvi.cbSize = sizeof(dvi);
			hr = (*pDllGetVersion)(&dvi);
			if (SUCCEEDED(hr))
			{
				ullVersion = MAKEDLLVERULL(dvi.dwMajorVersion, dvi.dwMinorVersion, 0, 0);
			}
		}
		FreeLibrary(hinstDll);
	}
	return ullVersion;
}

void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();

	if (hMenu)
	{
		AppendMenu(hMenu, MF_STRING, SWM_SAVE, _T("Save windows"));
		AppendMenu(hMenu, MF_STRING, SWM_RESTORE, _T("Restore windows"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

		if (IsWindowVisible(hWnd))
		{
			AppendMenu(hMenu, MF_STRING, SWM_HIDE, _T("Hide"));
		}
		else
		{
			AppendMenu(hMenu, MF_STRING, SWM_SHOW, _T("Show"));
		}
			
		AppendMenu(hMenu, MF_STRING, SWM_EXIT, _T("Exit"));

		//Set menu to the foreground or it won't appear.
		SetForegroundWindow(hWnd);

		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
		DestroyMenu(hMenu);
	}
}

BOOL OnInitDialog(HWND hWnd)
{
	HMENU hMenu = GetSystemMenu(hWnd, FALSE);

	if (hMenu)
	{
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, IDM_ABOUT, _T("About"));
	}

	HICON hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_WINDMILL), IMAGE_ICON, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	return TRUE;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
		0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ZeroMemory(&niData, sizeof(NOTIFYICONDATA));

	//TODO Handle differences in features across XP->7 (Quiet time, etc.)
	ULONGLONG ullVersion = GetDllVersion(_T("Shell32.dll"));
	if (ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
		niData.cbSize = sizeof(NOTIFYICONDATA);
	else niData.cbSize = NOTIFYICONDATA_V2_SIZE;

	niData.uID = ID_TRAY_ICON;
	niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	niData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDMILL));

	niData.hWnd = hWnd;
	niData.uCallbackMessage = SWM_TRAYMSG;

	lstrcpyn(niData.szTip, _T("Windmill"), sizeof(niData.szTip) / sizeof(TCHAR));

	Shell_NotifyIcon(NIM_ADD, &niData);

	if (niData.hIcon && DestroyIcon(niData.hIcon))
	{
		niData.hIcon = NULL;
	}

	//ShowWindow(hWnd, nCmdShow);
	//UpdateWindow(hWnd);

	return TRUE;
}

//MSDN example
void handleError(LPCTSTR lpszFunction) {
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

LocalFree(lpMsgBuf);
LocalFree(lpDisplayBuf);
//ExitProcess(dw);
}

BOOL SplitInTwo(TCHAR* left, TCHAR* right, TCHAR* source, CONST TCHAR* delimiter) {
	PTCHAR tok = _tcstok(source, delimiter);
	if (tok != NULL) {
		_tcscpy(left, tok);
		tok = _tcstok(NULL, delimiter);
		if (tok != NULL) {
			_tcscpy(right, tok);
			return TRUE;
		}
	}

	return FALSE;
}

//TODO lParam should probably be {SAVE, RESTORE} or sth like that, not a PHKEY
//Just make hKey global, it's C fcs...
// lParam = 1: save; 2: restore
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
	//Skip invisible and minimized windows.
	if (hWnd == NULL || !IsWindowVisible(hWnd) || IsIconic(hWnd)) {
		return TRUE;
	}

	//TODO use std::codecvt?

	char windowClassName[255] = { 0 };
	char windowTitle[255] = { 0 };

	GetClassNameA(hWnd, (LPSTR)windowClassName, sizeof(windowClassName));
	GetWindowTextA(hWnd, (LPSTR)windowTitle, sizeof(windowTitle));

	//Skip windows with empty titles.
	if (strlen(windowTitle) == 0) { //don't need strnlen_s AFAIK
		return TRUE;
	}

	/* Exclude:
		Class: "Button", "Shell_TrayWnd", "Internet Explorer_Hidden"
		Title: ""
		hWnd: self
	*/

	OutputDebugStringA(windowClassName);
	OutputDebugString(_T(":"));
	OutputDebugStringA(windowTitle);
	OutputDebugString(_T("\r\n"));

	RECT windowRect;

	if (lParam == 1) {
		BOOL result = GetWindowRect(hWnd, &windowRect);
		if (result) {
			TCHAR buffer[32];
			_stprintf(buffer, _T("%d"), hWnd);

			//TODO make buffer max length of 32b int (long) in base10, x4 + delimiters (causing "..." in regedit I think)
			//TODO actually, merge these into a QWORD somehow, if they fit. low words = size, hi words = top left?
			//-32000,-32000:-31840x-31973
			TCHAR dataBuffer[32] = { 0 };
			//TODO verify %d works for 64b ints
			_stprintf(dataBuffer, _T("%d,%d:%dx%d"), windowRect.top, windowRect.left, windowRect.bottom, windowRect.right);

			//TODO use _T() macro evertwhere to use correct fns
			LONG setRes = RegSetValueEx(hKey, buffer, 0, REG_SZ, (LPBYTE)&dataBuffer, sizeof(dataBuffer));

			if (setRes != ERROR_SUCCESS) {
				OutputDebugString(_T("RegSetValueEx failed\r\n"));
				//handleError(L"RegSetValueEx");
			}
		}
		else {
			//handleError(L"GetWindowRect");
		}
	}
	else if (lParam == 2) {
		TCHAR keyValueBuffer[32];
		TCHAR dataBuffer[256] = { 0 };
		DWORD dataBufferSize = 255;

		_stprintf(keyValueBuffer, _T("%d"), hWnd);

		LONG getRes = RegGetValue(HKEY_CURRENT_USER, lpcsKey, keyValueBuffer, RRF_RT_REG_SZ | KEY_WOW64_64KEY, NULL, dataBuffer, &dataBufferSize);
		//TODO handle more data return value
		//TODO Doesn't seem to like already open hKey? Returns 2 (FNF)
		if (getRes != ERROR_SUCCESS) {
			OutputDebugString(_T("RegGetValue failed\r\n"));
			//handleError(L"RegSetValueExA");
			return TRUE;
		}

		LONG left, top, right, bottom;
		TCHAR tcTopLeft[32], tcBottomRight[32];
		TCHAR tcTop[32], tcLeft[32], tcBottom[32], tcRight[32];

		if (SplitInTwo(tcTopLeft, tcBottomRight, dataBuffer, _T(":"))) {
			if (SplitInTwo(tcTop, tcLeft, tcTopLeft, _T(","))) {
				if (SplitInTwo(tcBottom, tcRight, tcBottomRight, _T("x"))) {
					top = _tcstol(tcTop, NULL, 10);
					left = _tcstol(tcLeft, NULL, 10);
					bottom = _tcstol(tcBottom, NULL, 10);
					right = _tcstol(tcRight, NULL, 10);
					//TODO check all those values
					
					//TODO Have to store window z order? :/ No: SWP_NOZORDER
					//Can probably just use hWnd = GetWindow(, GW_HWNDPREV, )
					//Might want this flag: SWP_ASYNCWINDOWPOS, SWP_NOSIZE
					//LONG width = right - left;
					//LONG height = top - bottom;

					SetWindowPos(hWnd, HWND_NOTOPMOST, left, top, right, bottom, SWP_NOACTIVATE | SWP_NOZORDER);
				}
			}
		}

		/* BOOL WINAPI SetWindowPos(
		_In_     HWND hWnd,
		_In_opt_ HWND hWndInsertAfter,
		_In_     int  X,
		_In_     int  Y,
		_In_     int  cx,
		_In_     int  cy,
		_In_     UINT uFlags
		);*/

		/*BOOL WINAPI SetWindowPlacement(
		_In_       HWND            hWnd,
		_In_ const WINDOWPLACEMENT *lpwndpl
		);
		*/
	}

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;

	switch (message)
	{
	case SWM_TRAYMSG:
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			ShowWindow(hWnd, SW_RESTORE);
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
		}
		break;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_MINIMIZE)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 1;
		}
		else if (wParam == IDM_ABOUT)
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
		break;

	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId)
		{
		case SWM_SAVE:
		{
			LONG openRes = RegCreateKeyEx(HKEY_CURRENT_USER, lpcsKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &hKey, NULL);
			if (openRes != ERROR_SUCCESS) {
				OutputDebugString(_T("RegCreateKeyEx failed\r\n"));
				break;
			}
			
			EnumWindows(EnumWindowsProc, (LPARAM)1);
		}
			
			break;

		case SWM_RESTORE:
		{
			//LPWSTR lpcsKey = TEXT("SOFTWARE\\Windmill");
			LONG openRes = RegCreateKeyEx(HKEY_CURRENT_USER, lpcsKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &hKey, NULL);
			if (openRes != ERROR_SUCCESS) {
				OutputDebugString(_T("RegCreateKeyEx failed\r\n"));
				break;
			}

			EnumWindows(EnumWindowsProc, (LPARAM)2);
		}
			break;

		case SWM_SHOW:
			ShowWindow(hWnd, SW_RESTORE);
			break;

		case SWM_HIDE:
		case IDOK:
			ShowWindow(hWnd, SW_HIDE);
			break;

		case SWM_EXIT:
			DestroyWindow(hWnd);
			break;

		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
			break;
		}
		return 1;

	case WM_INITDIALOG:
		return OnInitDialog(hWnd);

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
	{
		LONG closeOut = RegCloseKey(hKey);
		if (closeOut != ERROR_SUCCESS) {
			OutputDebugString(_T("RegCloseKey failed\r\n"));
			//handleError(L"RegCloseKey");
		}
		niData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &niData);
		PostQuitMessage(0);
	}
		break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
	}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
