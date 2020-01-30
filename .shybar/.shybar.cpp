// MCE, 2020
// WINDOWS 8+
#include <windows.h> // WinAPI
#include <string> // std::string type
#include <shobjidl_core.h> // IAppVisibility

// Callback function from EnumDesktopWindows
// int BOOL, _stdcall CALLBACK (..., long* LPARAM)
int _stdcall EnumWindowsProc(HWND whwnd, long lparam) {
	// If the window is hidden, minimized or Progman skip it
	if (!IsWindowVisible(whwnd) || IsIconic(whwnd) || GetShellWindow() == whwnd) return 1;

	// Skip windows with 0 title bar text
	const int windowTextLength = GetWindowTextLengthW(whwnd);
	if (windowTextLength == 0) return 1;

	// Skip UWP windows
	char windowClassName[256];
	GetClassNameA(whwnd, windowClassName, 256);

	if (strcmp(windowClassName, "Windows.UI.Core.CoreWindow") == 0) return 1;
	
	// We have a valid window, lets check if its in the taskbar area
	// Get window position and size
	RECT windowRECT;
	if (!GetWindowRect(whwnd, &windowRECT)) { return 1; }

	// Get taskbar position and size
	HWND* tbHWND = (reinterpret_cast<HWND*>(lparam));
	RECT tbRECT;
	GetWindowRect(*tbHWND, &tbRECT);

	// Translate windowRECT to be relative to the taskbars position
	windowRECT = {
		max(windowRECT.left, 0),
		max(windowRECT.top, 0),
		min(windowRECT.right - tbRECT.left, 0),
		min(windowRECT.bottom - tbRECT.top, 0)
	};

	// Is pos or size of window inside a child of taskbar
	HWND windowPosInChild = 0;
	HWND windowSizeInChild = 0;

	if (tbRECT.left + tbRECT.top <= 0) {
		windowPosInChild = ChildWindowFromPoint(*tbHWND, POINT{ windowRECT.left, windowRECT.top });
	} else {
		windowSizeInChild = ChildWindowFromPoint(*tbHWND, POINT{ windowRECT.right, windowRECT.bottom });
	}

	// If so, set the passed HWND as 0 and stop enumerating windows
	if (windowPosInChild != 0 || windowSizeInChild != 0 ) {
		*(reinterpret_cast<HWND*>(lparam)) = 0;
		return 0;
	}

	return 1;
}

int main(unsigned int argc, char* argv[]) {
	#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup") // Hide console

	// Register ALT + F1 hotkey to force-show
	if (!RegisterHotKey(0, 1, 0x0001 | 0x4000, 0x70)) return -1;
	
	// Corner size in which the mouse must be to consider showing the taskbar
	// Can be a maximum of 25 and a minimum of 1, it should never exceed the taskbars minimum size
	int OPT_HOTSPOT_SIZE = 4;
	int OPT_SLEEP_MS = 50; // How much time to sleep until we execute the next loop
	bool OPT_AUTO_EXPAND = true; // Expand the taskbar when no window is occupying its place?
	bool OPT_EXPAND_ON_START = true; // Expand and set the taskbar to alwaysontop when the start window is present?

	// Parse command line arguments
	for (unsigned int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "-hs") {
			if (isdigit(*argv[i + 1])) {
				OPT_HOTSPOT_SIZE = max(min(std::stoi(argv[i + 1]), 25), 1);
				++i;
			}
		} else if (std::string(argv[i]) == "-sm") {
			if (isdigit(*argv[i + 1])) {
				OPT_SLEEP_MS = std::stoi(argv[i + 1]);
				++i;
			}
		} else if (std::string(argv[i]) == "-nae") {
			OPT_AUTO_EXPAND = false;
		} else if (std::string(argv[i]) == "-nse") {
			OPT_EXPAND_ON_START = false;
		}
	}

	// Try to initialize a COM interface
	IAppVisibility* appVisibility = 0;
	if (OPT_EXPAND_ON_START) {
		const HRESULT CI_HR = CoInitialize(0);
		if (FAILED(CI_HR)) return -1;

		const HRESULT CCI_HR = CoCreateInstance(CLSID_AppVisibility, 0, CLSCTX_INPROC_SERVER, IID_IAppVisibility, (void**) &appVisibility);
		if (FAILED(CCI_HR)) return -1;
	}

	// Get desktop metrics
	const int DESKTOP_WIDTH = GetSystemMetrics(78);
	const int DESKTOP_HEIGHT = GetSystemMetrics(79);

	// Stores the taskbars HWND and its initial RECT position on start
	APPBARDATA appBarData = { sizeof(APPBARDATA) };

	// For checking if the taskbar has changed sides
	APPBARDATA dummyBarData = { sizeof(APPBARDATA) };

	// Check if the taskbar is in ABS_AUTOHIDE, otherwise, change it
	const unsigned int TASKBAR_STATE = SHAppBarMessage(0x00000004, &appBarData); // (ABM_GETSTATE, ...)
	if (TASKBAR_STATE != 0x0000001) {
		appBarData.lParam = 0x0000001;
		SHAppBarMessage(0x0000000A, &appBarData); // (ABM_SETSTATE, ...)
	}

	// Cycle each edge in search of the taskbar to get a handle
	for (unsigned int i = 0; i != 4; i++) {
		appBarData.uEdge = i;
		HWND appBarHWND = (HWND) SHAppBarMessage(0x00000007, &appBarData); // (ABM_GETAUTOHIDEBAR, ...)
		if (appBarHWND != 0) {
			appBarData.hWnd = appBarHWND;
			dummyBarData.uEdge = i;
			break;
		}
	}

	// Get the taskbars position
	if (!SHAppBarMessage(0x00000005, &appBarData)) return -1; // (ABM_GETTASKBARPOS, ...)

	MSG appMsg = { 0 }; // Create a message queue
	bool keyDoExpand = false;
	bool mustDoExpand = false;

	while (1) {
		// Check if the taskbar handle is still valid, and if not, try to get a new one
		while (!IsWindow(appBarData.hWnd)) {
			for (unsigned int i = 0; i != 4; i++) {
				appBarData.uEdge = i;
				HWND appBarHWND = (HWND) SHAppBarMessage(0x00000007, &appBarData); // (ABM_GETAUTOHIDEBAR, ...)
				if (appBarHWND != 0) {
					appBarData.hWnd = appBarHWND;
					break;
				}
			}
			if (appBarData.hWnd == 0) Sleep(1000);
		}

		// Show taskbar if the start menu is open
		if (OPT_EXPAND_ON_START) {
			BOOL startMenuVisible = false;
			appVisibility->IsLauncherVisible(&startMenuVisible);

			if (startMenuVisible) {
				mustDoExpand = true;
			} else {
				mustDoExpand = false;
			};
		}

		// If the thread has QS_HOTKEY in queue
		if (GetQueueStatus(0x0080)) {
			// Set the taskbar to always show
			if (PeekMessageW(&appMsg, 0, 0, 0, 0x0001)) { // (..., PM_REMOVE)
				if (appMsg.message == 0x0312) keyDoExpand = !keyDoExpand; // (... == WM_HOTKEY)
			}
		}

		// Check if the taskbar has changed sides and if so, update the initial position
		if (HWND(SHAppBarMessage(0x00000007, &dummyBarData)) == 0) {
			for (unsigned int i = 0; i != 4; i++) {
				appBarData.uEdge = i;
				HWND appBarHWND = (HWND) SHAppBarMessage(0x00000007, &appBarData); // (ABM_GETAUTOHIDEBAR, ...)
				if (appBarHWND != 0) {
					appBarData.hWnd = appBarHWND;
					dummyBarData.uEdge = i;
					SHAppBarMessage(0x00000005, &appBarData); // (ABM_GETTASKBARPOS, ...)
					break;
				}
			}
		}

		// Get the taskbars current pos and size
		RECT tbCurrentRECT;
		GetWindowRect(appBarData.hWnd, &tbCurrentRECT);

		// Set the taskbar to its expanded position if it is not already there
		int tbNewPosX = min(max(appBarData.rc.left, 0), DESKTOP_WIDTH - (appBarData.rc.right - appBarData.rc.left));
		int tbNewPosY = min(max(appBarData.rc.top, 0), DESKTOP_HEIGHT - (appBarData.rc.bottom - appBarData.rc.top));

		if (tbCurrentRECT.left + tbCurrentRECT.top != tbNewPosX + tbNewPosY) {
			SetWindowPos(appBarData.hWnd, HWND_TOP, tbNewPosX, tbNewPosY, -1, -1, 0x0001 | 0x00400);
		}

		// Auto expand the taskbar when no window is occupying its space
		bool canDoExpand = false;

		// Enumerate all desktop windows in EnumWindowsProc callback
		if (OPT_AUTO_EXPAND) {
			HWND tbEnumHWND = appBarData.hWnd;
			EnumDesktopWindows(nullptr, &EnumWindowsProc, (long) &tbEnumHWND);
			if (tbEnumHWND != 0) canDoExpand = true;
		}

		// Check if cursor is inside a hotspot
		bool isCursorInHotspot = false;
		POINT cursorPOINT;

		if (GetCursorPos(&cursorPOINT)) {
			if ((cursorPOINT.x < OPT_HOTSPOT_SIZE || cursorPOINT.x > DESKTOP_WIDTH - OPT_HOTSPOT_SIZE)
			&& (cursorPOINT.y < OPT_HOTSPOT_SIZE || cursorPOINT.y > DESKTOP_HEIGHT - OPT_HOTSPOT_SIZE)) {
				isCursorInHotspot = true;
			}
		}

		// Check if the cursor is hovering a taskbar child
		const HWND cursorPosChildHWND = ChildWindowFromPoint(appBarData.hWnd, POINT { cursorPOINT.x - tbCurrentRECT.left, cursorPOINT.y - tbCurrentRECT.top });

		// If the cursor is in a hotspot and above a child of the taskbar, show it
		// Otherwise, if the cursor point is not above a child, hide it
		if (canDoExpand || mustDoExpand || keyDoExpand || (isCursorInHotspot && cursorPosChildHWND != 0)) {
			ShowWindow(appBarData.hWnd, 5);
		} else if (IsWindowVisible(appBarData.hWnd) && !isCursorInHotspot && cursorPosChildHWND == 0) {
			ShowWindow(appBarData.hWnd, 0);
		}

		Sleep(OPT_SLEEP_MS);
	}

	// We should never get here
	return -1;
}