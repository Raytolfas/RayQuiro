#pragma once
#include <windows.h>
#include <string>
#include <vector>

class RayQuiroApp {
    HWND hwnd;
    HINSTANCE hInst;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

public:
    RayQuiroApp() : hwnd(NULL) { hInst = GetModuleHandle(NULL); }

    void init(std::string title, int w, int h) {
        const char CLASS_NAME[] = "RayQuiroWindowClass";
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassA(&wc);

        hwnd = CreateWindowExA(0, CLASS_NAME, title.c_str(), 
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
            CW_USEDEFAULT, CW_USEDEFAULT, w, h, 
            NULL, NULL, hInst, NULL);
    }

    void add_button(std::string text, int x, int y, int w, int h) {
        CreateWindowA("BUTTON", text.c_str(), 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            x, y, w, h, hwnd, NULL, hInst, NULL);
    }

    void add_text(std::string text, int x, int y, int w, int h) {
        CreateWindowA("STATIC", text.c_str(), 
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            x, y, w, h, hwnd, NULL, hInst, NULL);
    }

    void run() {
        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    static void show_message(std::string title, std::string text) {
        MessageBoxA(NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    }
};
