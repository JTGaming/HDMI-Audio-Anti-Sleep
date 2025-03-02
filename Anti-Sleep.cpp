// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Anti-Sleep.h"
#include "resource.h"

// we need commctrl v6 for LoadIconMetric()
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comsuppw.lib")

HINSTANCE g_hInst = NULL;
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
wchar_t const szWindowClass[] = L"HDMI Audio Anti-Sleep";
// Use a guid to uniquely identify our icon
class __declspec(uuid("d692a428-83d7-4506-b1f6-82b0045b99d6")) NotifIcon;

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int
)
{
    // Get class id as string
    LPOLESTR className;
    HRESULT hr = StringFromCLSID(__uuidof(NotifIcon), &className);
    if (hr != S_OK)
        return -1;

    // convert to CString
    CString c = (char*)(_bstr_t)className;
    // then release the memory used by the class name
    CoTaskMemFree(className);

    CreateMutex(0, FALSE, c); // try to create a named mutex
    if (GetLastError() == ERROR_ALREADY_EXISTS) // did the mutex already exist?
        return -1; // quit; mutex is released automatically

    SetProcessDPIAware();
    g_hInst = hInstance;
    RegisterWindowClass(szWindowClass, WndProc);

    // Create the main window. This could be a hidden window if you don't need
    // any UI other than the notification icon.
    HWND hwnd = CreateWindow(szWindowClass, szWindowClass, 0, 0, 0, 0, 0, 0, 0, g_hInst, 0);
    if (hwnd)
        MainLoop();

    return 0;
}

void MainLoop()
{
    NotificationClient pClient;

    std::vector<float> peaks{};
    UINT channels = 0;

    bool CanRun = true;
    while (CanRun)
    {
        Sleep(500);

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                CanRun = false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        IAudioMeterInformation* pMeterInfo = pClient.GetMeter();
        if (!pMeterInfo)
        {
            UpdateIcon(DISABLED);
            continue;
        }

        float volume{};
        auto hr = pMeterInfo->GetPeakValue(&volume);
        if (hr != S_OK)
        {
            UpdateIcon(DISABLED);
            continue;
        }

		MODES mode = CheckVolume(volume, pClient.IsHDMI);
        UpdateIcon(mode);
    }
}

MODES CheckVolume(float volume, bool hdmi)
{
    static MODES mode = DISABLED;
    static auto start_time = std::chrono::high_resolution_clock::now();
    static bool sound_started = false;

    if (volume <= CUST_FLT_EPS)
    {
		sound_started = false;
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> delta_time_ms = end_time - start_time;

        //10sec since last sound
		if (delta_time_ms.count() > 10000) // 10 seconds
            mode = DISABLED;
    }
    else
    {
        if (!sound_started)
            start_time = std::chrono::high_resolution_clock::now();

		sound_started = true;
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> delta_time_ms = end_time - start_time;

        //1sec since sound started
        if (delta_time_ms.count() > 1000) // 1 second
        {
            start_time = std::chrono::high_resolution_clock::now();
            mode = ACTIVE;
        }
    }

    if (mode == ACTIVE)
    {
        static auto start_time1 = std::chrono::high_resolution_clock::now();
        auto end_time1 = std::chrono::high_resolution_clock::now(); //-V656
        std::chrono::duration<float, std::milli> delta_time_ms = end_time1 - start_time1;
        if (delta_time_ms.count() > 10000 && hdmi) // 10 seconds
        {
            start_time1 = std::chrono::high_resolution_clock::now();
            SetThreadExecutionState(ES_DISPLAY_REQUIRED);
        }
    }

	return mode;
}

void RegisterWindowClass(PCWSTR pszClassName, WNDPROC lpfnWndProc)
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = lpfnWndProc;
    wcex.hInstance = g_hInst;
    wcex.lpszClassName = pszClassName;
    RegisterClassEx(&wcex);
}

BOOL AddIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    // add the icon, setting the icon, tooltip, and callback message.
    // the icon will be identified with the GUID
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_GUID;
    nid.guidItem = __uuidof(NotifIcon);
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    LoadIconMetric(g_hInst, MAKEINTRESOURCE(IDI_NOTIFICATIONICON1 + (int)DISABLED), LIM_SMALL, &nid.hIcon);
    Shell_NotifyIcon(NIM_ADD, &nid);

    // NOTIFYICON_VERSION_4 is prefered
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void UpdateIcon(MODES mode)
{
    static MODES old_mode = DISABLED;
    if (mode == old_mode)
        return;

    int IDX = (int)mode + IDI_NOTIFICATIONICON1;
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_ICON | NIF_GUID;
    nid.guidItem = __uuidof(NotifIcon);

    HRESULT hr = LoadIconMetric(g_hInst, MAKEINTRESOURCE(IDX), LIM_SMALL, &nid.hIcon);
    if (hr != S_OK)
        return;
    BOOL ret = Shell_NotifyIcon(NIM_MODIFY, &nid);
    if (ret != TRUE) //-V676
        return;

    old_mode = mode;
}

BOOL DeleteIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = __uuidof(NotifIcon);
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDC_CONTEXTMENU));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);

            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
                uFlags |= TPM_RIGHTALIGN;
            else
                uFlags |= TPM_LEFTALIGN;
            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // add the notification icon
        if (!AddIcon(hwnd))
            return -1;
        break;
    case WM_COMMAND:
    {
        // Parse the menu selections:
        switch (LOWORD(wParam))
        {
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }
    break;

    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case WM_CONTEXTMENU:
            ShowContextMenu(hwnd);
            break;
        }
        break;

    case WM_DESTROY:
        DeleteIcon();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
