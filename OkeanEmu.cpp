// OkeanEmu.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "OkeanEmu.h"
#include "bus/okeanBus.h"
#include <atomic>
#include <stdio.h>
#include <windows.h>
#include <shobjidl.h> 
#include <codecvt>
#include <fstream>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::string monitor = "";
std::string os = "";
//std::string monitor = "C:/work/project/OkeanEmu/OKEAN240_1(MON).BIN";
//std::string os = "C:/work/project/OkeanEmu/OKEAN240_2(CPM).BIN";
//std::string monitor = "C:/work/project/OkeanEmu/okean240/emu/Okean240/MONITOR.BIN";
//std::string os = "C:/work/project/OkeanEmu/okean240/emu/Okean240/CPM80.BIN";

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

std::shared_ptr<OkeanBus> okeanBus;
std::atomic<bool> m_repaintFlag;
std::atomic<bool> updateRecordPlayerPercent;

const UINT_PTR VIDEO_TIMER = 0x12345;
const UINT_PTR RECORD_PLAYER_TIMER = 0x12346;
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_OKEANEMU, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_OKEANEMU));

    MSG msg;
	m_repaintFlag = false;
    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_OKEANEMU));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_OKEANEMU);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void startOkean240(HWND hWnd, std::string monitor, std::string os) {
	okeanBus = std::make_shared<OkeanBus>();
	okeanBus->load(monitor, os);
	okeanBus->execute([hWnd](OkeanBus::EventType type) {
		if (type == OkeanBus::EventType::VIDEO) {
			if (!m_repaintFlag) {
				m_repaintFlag = true;
				SetTimer(hWnd, VIDEO_TIMER, 30, (TIMERPROC)NULL);

			}
		} else if (type == OkeanBus::EventType::RECORD_PLAYER) {
			SetTimer(hWnd, RECORD_PLAYER_TIMER, 500, (TIMERPROC)NULL);
		}
	});
}

void showTitle(HWND hWnd) {
	std::string title = "Okean-240  Monitor: ";
	title.append(monitor);
	title.append(" OS: ");
	title.append(os);

	SetWindowTextA(hWnd, title.c_str());
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   showTitle(hWnd);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

uint8_t updated_offset = 0;

void prepareOkeanScreen(HDC hdcTemp) {
	updated_offset = okeanBus->read(0xC0);
	uint32_t *bitmap;
	uint16_t width;
	if (okeanBus->isColorDisplay()) {
		bitmap = new uint32_t[256 * 256];
		width = 256;
	}
	else {
		bitmap = new uint32_t[512 * 256];
		width = 512;
	}
	okeanBus->createBitmap(bitmap);
	uint32_t item = 0;
	for (uint16_t y = 0; y < 256; y++) {
		for (uint16_t x = 0; x < width; x++) {
			SetPixel(hdcTemp, x, y, bitmap[item++]);
		}
	}
	delete[] bitmap;
}

// Message handler for about box.
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

void AddMenus(HWND hwnd) {

	HMENU hMenubar;
	HMENU hMenu;
	HMENU hPlayerMenu;
	HMENU hUtilMenu;

	hMenubar = CreateMenu();
	hMenu = CreateMenu();
	hPlayerMenu = CreateMenu();
	hUtilMenu = CreateMenu();

	AppendMenuW(hMenu, MF_STRING, IDC_OKEANEMU_MONITOR, L"&File Monitor");
	AppendMenuW(hMenu, MF_STRING, IDC_OKEANEMU_OS, L"&File OS");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu, MF_STRING, IDC_START_OKEAN, L"&Start Okean-240");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"&Exit");
	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");
	AppendMenuW(hPlayerMenu, MF_STRING, IDC_FILE_FOR_PLAYER, L"&File for Record Player");
	AppendMenuW(hPlayerMenu, MF_STRING, IDC_PLAYER_START, L"&Start Record Player");
	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hPlayerMenu, L"&Record Player");
	AppendMenuW(hUtilMenu, MF_STRING, IDC_FILE_COPY_TO_HOST, L"&Copy file to host");
	AppendMenuW(hUtilMenu, MF_STRING, IDC_FILE_COPY_TO_OKEAN, L"&Copy file to Okean240");
	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hUtilMenu, L"&Utilites");

	AppendMenuW(hMenubar, MF_STRING, IDM_ABOUT, L"&About");
	SetMenu(hwnd, hMenubar);
}

std::string ConvertLPCWSTRToString(LPCWSTR lpcwszStr)
{
	// Determine the length of the converted string 
	int strLength
		= WideCharToMultiByte(CP_UTF8, 0, lpcwszStr, -1,
			nullptr, 0, nullptr, nullptr) - 1;

	// Create a std::string with the determined length 
	std::string str(strLength, 0);

	// Perform the conversion from LPCWSTR to std::string 
	WideCharToMultiByte(CP_UTF8, 0, lpcwszStr, -1, &str[0],
		strLength, nullptr, nullptr);

	// Return the converted std::string 
	return str;
}

std::string fileOpenDialog() {
	std::string pszFilePath = "NONE";

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog *pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			// Show the Open dialog box.
			hr = pFileOpen->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR fileP;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &fileP);
					pszFilePath = ConvertLPCWSTRToString(fileP);
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}
	return pszFilePath;
}


HRESULT CreateDialogToPickFolder(HWND hWnd)
{
	IFileOpenDialog* pPickFolderDialog = NULL;
	IShellItem* pPickedFolder = NULL;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pPickFolderDialog));

	if (SUCCEEDED(hr))
	{
		DWORD dialogOptions;
		hr = pPickFolderDialog->GetOptions(&dialogOptions);
		if (SUCCEEDED(hr))
		{
			hr = pPickFolderDialog->SetOptions(dialogOptions | FOS_PICKFOLDERS);
			if (SUCCEEDED(hr))
			{
				hr = pPickFolderDialog->Show(hWnd);
				if (SUCCEEDED(hr))
				{
					hr = pPickFolderDialog->GetResult(&pPickedFolder);
					if (SUCCEEDED(hr))
					{
						PWSTR pszFolderPath = NULL;
						hr = pPickedFolder->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
						if (SUCCEEDED(hr))
						{
							okeanBus->copyFilesToHost(ConvertLPCWSTRToString(pszFolderPath));
						}
					}
					pPickedFolder->Release();
				}
			}
		}
		pPickFolderDialog->Release();
	}
	return hr;
}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
		AddMenus(hWnd);
		break;    
	case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
			case IDC_OKEANEMU_MONITOR:
				monitor = fileOpenDialog();
				showTitle(hWnd);
				break;
			case IDC_OKEANEMU_OS:
				os = fileOpenDialog();
				showTitle(hWnd);
				break;
			case IDC_START_OKEAN:
				startOkean240(hWnd, monitor, os);
				break;
			case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
			case IDC_FILE_FOR_PLAYER:
				if (okeanBus != nullptr) {
					auto fileFoPlayer = fileOpenDialog();
					okeanBus->loadFileToRecordPlayer(fileFoPlayer);
				}
				break;
			case IDC_PLAYER_START:
				if (okeanBus != nullptr) {
					okeanBus->startRecordPlayer();
				}
				break;
			case IDC_FILE_COPY_TO_HOST:
				if (okeanBus != nullptr) {
					CreateDialogToPickFolder(hWnd);
				}
				break;
			case IDC_FILE_COPY_TO_OKEAN:
				if (okeanBus != nullptr) {
					auto fileFoPlayer = fileOpenDialog();
					okeanBus->copyFilesFromHost(fileFoPlayer);
				}
				break;
			default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	case WM_TIMER:
		switch (wParam)
		{
		case RECORD_PLAYER_TIMER:
		{
			KillTimer(hWnd, RECORD_PLAYER_TIMER);
			RECT rect;
			GetClientRect(hWnd, &rect);
			InvalidateRect(hWnd, &rect, false);
			updateRecordPlayerPercent = true;
			UpdateWindow(hWnd);
			return 0;
		}
		break;
		case VIDEO_TIMER:
		{
			KillTimer(hWnd, VIDEO_TIMER);
			m_repaintFlag = false;
			RECT rect;
			GetClientRect(hWnd, &rect);
			InvalidateRect(hWnd, &rect, false);
			UpdateWindow(hWnd);
			return 0;
		}
		break;
	}
    case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		if (okeanBus != nullptr) {
			if (updateRecordPlayerPercent == false) {
				HDC hdcTemp = CreateCompatibleDC(hdc);
				uint16_t ySize = okeanBus->isColorDisplay() ? 256 : 512;
				HBITMAP hbmpTemp = CreateCompatibleBitmap(hdc, ySize, 256);
				if (hbmpTemp) {
					if (auto hOld = SelectObject(hdcTemp, hbmpTemp)) {
						prepareOkeanScreen(hdcTemp);
						RECT rect;
						GetClientRect(hWnd, &rect);
						StretchBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hdcTemp, 0, 0, ySize, 256, SRCCOPY);
						SelectObject(hdcTemp, hOld);
						DeleteObject(hbmpTemp);
						DeleteDC(hdcTemp);
					}
				}
				if (updated_offset != okeanBus->read(0xC0) && m_repaintFlag == false) {
					m_repaintFlag = true;
					SetTimer(hWnd, VIDEO_TIMER, 30, (TIMERPROC)NULL);
				}
			}
			if (updateRecordPlayerPercent) {
				updateRecordPlayerPercent = false;
				double_t per = 0;
				auto status = okeanBus->getRecordPlayerStatus(per);
				if (status == RecordPlayer::PlaySatus::PLAYING || status == RecordPlayer::PlaySatus::SILENCE) {
					HBRUSH yBrush;
					yBrush = CreateSolidBrush(RGB(0, 200, 0));
					SelectObject(hdc, yBrush);
					RECT rect;
					GetClientRect(hWnd, &rect);
					rect.top = rect.bottom - 20;
					rect.right = rect.right * per;
					FillRect(hdc, &rect, yBrush);
					SetTimer(hWnd, RECORD_PLAYER_TIMER, 500, (TIMERPROC)NULL);
				} else {
					SetTimer(hWnd, VIDEO_TIMER, 30, (TIMERPROC)NULL);
					m_repaintFlag = true;
				}
			}
		}
		EndPaint(hWnd, &ps);
	}
		break;
	case WM_CHAR:
		if (okeanBus != nullptr) {
			okeanBus->write(0x40, static_cast<uint8_t>(wParam));
		}
		break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
