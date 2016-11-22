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
 * VERSION PROGRESS DETAILS
 * 0.1:    ✔        Basic Win32 tray app
 * 0.2:    ✔        Save window positions manually via tray menu (to registry)
 * 0.3:    ✔        Restore window positions manually via tray menu (from registry)
 * 0.5:             UI and registry settings to filter windows to save (or restore?)
 * 0.6:             Ensure windows will stay within bounds.
 * 1.0:             32/64-bit compatibility
 * 2.0:             Detect specific dock-associated hardware and trigger based on that, or is there a Windows Power/Dock API?
 * 2.5:             Allow configuration of trigger hardware
 * X:               Cmake for possible cross-platform support?

 Known issues:
 - Can't modify administrator process' windows
 - 
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
LPWSTR lpcsKey = TEXT("SOFTWARE\\Windmill");
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
		AppendMenu(hMenu, MF_STRING, SWM_SAVE, TEXT("Save windows"));
		AppendMenu(hMenu, MF_STRING, SWM_RESTORE, TEXT("Restore windows"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

		if (IsWindowVisible(hWnd))
		{
			AppendMenu(hMenu, MF_STRING, SWM_HIDE, TEXT("Hide"));
		}
		else
		{
			AppendMenu(hMenu, MF_STRING, SWM_SHOW, TEXT("Show"));
		}
			
		AppendMenu(hMenu, MF_STRING, SWM_EXIT, TEXT("Exit"));

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
		AppendMenu(hMenu, MF_STRING, IDM_ABOUT, TEXT("About"));
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
	ULONGLONG ullVersion = GetDllVersion(TEXT("Shell32.dll"));
	if (ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
		niData.cbSize = sizeof(NOTIFYICONDATA);
	else niData.cbSize = NOTIFYICONDATA_V2_SIZE;

	niData.uID = ID_TRAY_ICON;
	niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	niData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDMILL));

	niData.hWnd = hWnd;
	niData.uCallbackMessage = SWM_TRAYMSG;

	lstrcpyn(niData.szTip, TEXT("Windmill"), sizeof(niData.szTip) / sizeof(TCHAR));

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
}

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

	TCHAR buffer[32] = { 0 };
	_stprintf(buffer, TEXT("%#x:"), hWnd);
	OutputDebugString(buffer);
	OutputDebugStringA(windowClassName);
	OutputDebugString(TEXT(":"));
	OutputDebugStringA(windowTitle);
	OutputDebugString(TEXT("\r\n"));

	WINDOWPLACEMENT windowPlacement;

	if (lParam == SWM_SAVE) {
		BOOL result = GetWindowPlacement(hWnd, &windowPlacement);
		if (result) {
			TCHAR buffer[32] = { 0 };
			_stprintf(buffer, TEXT("%#016x"), hWnd);

			//TODO use TEXT() macro evertwhere to use correct fns
			LONG setRes = RegSetValueEx(hKey, buffer, 0, REG_BINARY, (LPBYTE)&windowPlacement, sizeof(windowPlacement));

			if (setRes != ERROR_SUCCESS) {
				OutputDebugString(TEXT("RegSetValueEx failed\r\n"));
				//handleError(L"RegSetValueEx");
			}
		}
		else {
			//handleError(L"GetWindowRect");
		}
	}
	else if (lParam == SWM_RESTORE) {
		TCHAR keyValueBuffer[32] = { 0 };
		DWORD lwpSize = sizeof(WINDOWPLACEMENT);

		_stprintf(keyValueBuffer, TEXT("%#016x"), hWnd);

		LONG getRes = RegQueryValueEx(hKey, keyValueBuffer, NULL, NULL, (LPBYTE)&windowPlacement, &lwpSize);
		if (getRes != ERROR_SUCCESS) {
			OutputDebugString(TEXT("RegQueryValueEx failed\r\n"));
			//handleError(L"RegSetValueExA");
			return TRUE;
		}

		windowPlacement.length = sizeof(WINDOWPLACEMENT); //lwpSize;

		if (!SetWindowPlacement(hWnd, &windowPlacement)) {
			handleError(TEXT("SetWindowPlacement"));
			//OutputDebugString(TEXT("SetWindowPlacement failed\r\n"));
		}
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
				OutputDebugString(TEXT("RegCreateKeyEx failed\r\n"));
				break;
			}
			
			EnumWindows(EnumWindowsProc, SWM_SAVE);
		}
			
			break;

		case SWM_RESTORE:
		{
			//LPWSTR lpcsKey = TEXT("SOFTWARE\\Windmill");
			LONG openRes = RegCreateKeyEx(HKEY_CURRENT_USER, lpcsKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &hKey, NULL);
			if (openRes != ERROR_SUCCESS) {
				OutputDebugString(TEXT("RegCreateKeyEx failed\r\n"));
				break;
			}

			EnumWindows(EnumWindowsProc, SWM_RESTORE);
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
			OutputDebugString(TEXT("RegCloseKey failed\r\n"));
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
