#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <raylib.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class RayQuiroApp {
    int nextX = 24;
    int nextY = 24;
    int defaultItemW = 280;
    int defaultItemH = 34;
    int gap = 14;

    void compute_pos(int& x, int& y, int& w, int& h, int defH) {
        if (w <= 0) w = defaultItemW;
        if (h <= 0) h = defH;
        if (x < 0) x = nextX;
        if (y < 0) {
            y = nextY;
            nextY += h + gap;
        }
    }

#ifdef _WIN32
    HWND hwnd = NULL;
    HINSTANCE hInst = NULL;
    COLORREF bgColor = RGB(24, 24, 27);
    COLORREF textColor = RGB(244, 244, 246);
    COLORREF editBgColor = RGB(39, 39, 42);
    COLORREF editTextColor = RGB(244, 244, 246);
    HBRUSH bgBrush = NULL;
    HBRUSH editBrush = NULL;
    HFONT hFont = NULL;
    std::vector<HWND> controls;

    static std::wstring utf8_to_wide(const std::string& str) {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::wstring wstr(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        RayQuiroApp* self = reinterpret_cast<RayQuiroApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (uMsg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        if (self) {
            if (uMsg == WM_ERASEBKGND) {
                HDC hdc = (HDC)wParam;
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (self->bgBrush) FillRect(hdc, &rc, self->bgBrush);
                return 1;
            }
            if (uMsg == WM_CTLCOLORSTATIC) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, self->textColor);
                SetBkColor(hdc, self->bgColor);
                return (LRESULT)self->bgBrush;
            }
            if (uMsg == WM_CTLCOLOREDIT) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, self->editTextColor);
                SetBkColor(hdc, self->editBgColor);
                return (LRESULT)self->editBrush;
            }
            if (uMsg == WM_CTLCOLORBTN) {
                return (LRESULT)self->bgBrush;
            }
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    static COLORREF parse_hex_color(const std::string& raw) {
        std::string s = raw;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) return RGB(244, 244, 246);
        if (s == "white") return RGB(255, 255, 255);
        if (s == "black") return RGB(0, 0, 0);
        if (s.front() == '#') {
            std::string hex = s.substr(1);
            if (hex.size() == 3) {
                hex = std::string() + hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
            }
            if (hex.size() == 6) {
                try {
                    unsigned long val = std::stoul(hex, nullptr, 16);
                    int r = (val >> 16) & 0xFF;
                    int g = (val >> 8) & 0xFF;
                    int b = val & 0xFF;
                    return RGB(r, g, b);
                } catch (...) {}
            }
        }
        return RGB(244, 244, 246);
    }
#else
    struct LinuxControl {
        enum Type { Button, Text, Input, Checkbox } type;
        std::string text;
        std::string value;
        bool checked = false;
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };
    std::vector<LinuxControl> linuxControls;
    std::string linuxTitle = "RayQuiro";
    int linuxWidth = 800;
    int linuxHeight = 600;
    bool linuxRunning = false;
    Color linuxBgColor = Color{24, 24, 27, 255};
    Color linuxTextColor = Color{244, 244, 246, 255};
    Color linuxItemBgColor = Color{39, 39, 42, 255};
    Color linuxItemTextColor = Color{244, 244, 246, 255};
    Color linuxBorderColor = Color{63, 63, 70, 255};
    Color linuxButtonBgColor = Color{37, 99, 235, 255};
    Color linuxButtonHoverColor = Color{59, 130, 246, 255};

    static Color parse_color(const std::string& raw) {
        std::string s = raw;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) return Color{244, 244, 246, 255};
        if (s == "white") return Color{255, 255, 255, 255};
        if (s == "black") return Color{0, 0, 0, 255};
        if (s.front() == '#') {
            std::string hex = s.substr(1);
            if (hex.size() == 3) {
                hex = std::string() + hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
            }
            if (hex.size() == 6) {
                try {
                    unsigned long val = std::stoul(hex, nullptr, 16);
                    unsigned char r = static_cast<unsigned char>((val >> 16) & 0xFF);
                    unsigned char g = static_cast<unsigned char>((val >> 8) & 0xFF);
                    unsigned char b = static_cast<unsigned char>(val & 0xFF);
                    return Color{r, g, b, 255};
                } catch (...) {}
            }
        }
        return Color{244, 244, 246, 255};
    }
#endif

public:
    static RayQuiroApp& instance() {
        static RayQuiroApp app;
        return app;
    }

    RayQuiroApp() {
#ifdef _WIN32
        hInst = GetModuleHandleW(NULL);
#endif
    }

    ~RayQuiroApp() {
        cleanup();
    }

    void init(std::string title, int w, int h) {
#ifdef _WIN32
        SetProcessDPIAware();
        cleanup();
        bgBrush = CreateSolidBrush(bgColor);
        editBrush = CreateSolidBrush(editBgColor);
        hFont = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        const wchar_t CLASS_NAME[] = L"RayQuiroWindowClassW";
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = bgBrush;
        RegisterClassW(&wc);

        std::wstring wtitle = utf8_to_wide(title);
        hwnd = CreateWindowExW(0, CLASS_NAME, wtitle.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, w, h,
            NULL, NULL, hInst, NULL);
        if (hwnd) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
        nextX = 24;
        nextY = 24;
        defaultItemW = (w > 120) ? (w - 60) : 280;
#else
        cleanup();
        linuxTitle = title.empty() ? "RayQuiro" : title;
        linuxWidth = w > 0 ? w : 800;
        linuxHeight = h > 0 ? h : 600;
        nextX = 24;
        nextY = 24;
        defaultItemW = (linuxWidth > 120) ? (linuxWidth - 60) : 280;
#endif
    }

    void add_button(std::string text, int x = -1, int y = -1, int w = -1, int h = -1) {
#ifdef _WIN32
        if (!hwnd) return;
        compute_pos(x, y, w, h, 36);
        std::wstring wtext = utf8_to_wide(text);
        HWND btn = CreateWindowW(L"BUTTON", wtext.c_str(),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            x, y, w, h, hwnd, NULL, hInst, NULL);
        if (btn && hFont) SendMessageW(btn, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (btn) controls.push_back(btn);
#else
        compute_pos(x, y, w, h, 36);
        linuxControls.push_back(LinuxControl{LinuxControl::Button, text, "", false, x, y, w, h});
#endif
    }

    void add_text(std::string text, int x = -1, int y = -1, int w = -1, int h = -1) {
#ifdef _WIN32
        if (!hwnd) return;
        compute_pos(x, y, w, h, 36);
        std::wstring wtext = utf8_to_wide(text);
        HWND lbl = CreateWindowW(L"STATIC", wtext.c_str(),
            WS_VISIBLE | WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
            x, y, w, h, hwnd, NULL, hInst, NULL);
        if (lbl && hFont) SendMessageW(lbl, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (lbl) controls.push_back(lbl);
#else
        compute_pos(x, y, w, h, 36);
        linuxControls.push_back(LinuxControl{LinuxControl::Text, text, "", false, x, y, w, h});
#endif
    }

    void add_input(std::string placeholder, int x = -1, int y = -1, int w = -1, int h = -1) {
#ifdef _WIN32
        if (!hwnd) return;
        compute_pos(x, y, w, h, 36);
        std::wstring wph = utf8_to_wide(placeholder);
        HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wph.c_str(),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
            x, y, w, h, hwnd, NULL, hInst, NULL);
        if (edit && hFont) SendMessageW(edit, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (edit) controls.push_back(edit);
#else
        compute_pos(x, y, w, h, 36);
        linuxControls.push_back(LinuxControl{LinuxControl::Input, placeholder, "", false, x, y, w, h});
#endif
    }

    void add_checkbox(std::string label, bool checked = false, int x = -1, int y = -1, int w = -1, int h = -1) {
#ifdef _WIN32
        if (!hwnd) return;
        compute_pos(x, y, w, h, 36);
        std::wstring wlbl = utf8_to_wide(label);
        HWND chk = CreateWindowW(L"BUTTON", wlbl.c_str(),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_VCENTER,
            x, y, w, h, hwnd, NULL, hInst, NULL);
        if (chk && hFont) SendMessageW(chk, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (chk && checked) SendMessageW(chk, BM_SETCHECK, BST_CHECKED, 0);
        if (chk) controls.push_back(chk);
#else
        compute_pos(x, y, w, h, 36);
        linuxControls.push_back(LinuxControl{LinuxControl::Checkbox, label, "", checked, x, y, w, h});
#endif
    }

    void set_style(const std::string& cssLike) {
        std::stringstream ss(cssLike);
        std::string token;
        while (std::getline(ss, token, ';')) {
            size_t colon = token.find(':');
            if (colon == std::string::npos) continue;
            std::string key = token.substr(0, colon);
            std::string val = token.substr(colon + 1);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
#ifdef _WIN32
            if (key == "background" || key == "bg" || key == "background-color") {
                bgColor = parse_hex_color(val);
                if (bgBrush) DeleteObject(bgBrush);
                bgBrush = CreateSolidBrush(bgColor);
                int avg = (GetRValue(bgColor) + GetGValue(bgColor) + GetBValue(bgColor)) / 3;
                if (avg < 128) {
                    editBgColor = RGB(39, 39, 42);
                    editTextColor = RGB(244, 244, 246);
                } else {
                    editBgColor = RGB(255, 255, 255);
                    editTextColor = RGB(24, 24, 27);
                }
                if (editBrush) DeleteObject(editBrush);
                editBrush = CreateSolidBrush(editBgColor);
            } else if (key == "color" || key == "text-color") {
                textColor = parse_hex_color(val);
            } else if (key == "font-size") {
                try {
                    int sz = std::stoi(val);
                    if (sz > 0) {
                        if (hFont) DeleteObject(hFont);
                        hFont = CreateFontW(
                            -sz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                        for (HWND ctrl : controls) {
                            SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFont, TRUE);
                        }
                    }
                } catch (...) {}
            }
#else
            if (key == "background" || key == "bg" || key == "background-color") {
                linuxBgColor = parse_color(val);
                int avg = (linuxBgColor.r + linuxBgColor.g + linuxBgColor.b) / 3;
                if (avg < 128) {
                    linuxItemBgColor = Color{39, 39, 42, 255};
                    linuxItemTextColor = Color{244, 244, 246, 255};
                    linuxBorderColor = Color{63, 63, 70, 255};
                } else {
                    linuxItemBgColor = Color{255, 255, 255, 255};
                    linuxItemTextColor = Color{24, 24, 27, 255};
                    linuxBorderColor = Color{212, 212, 216, 255};
                }
            } else if (key == "color" || key == "text-color") {
                linuxTextColor = parse_color(val);
            }
#endif
        }
#ifdef _WIN32
        if (hwnd) {
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
#endif
    }

    void set_theme(const std::string& name) {
#ifdef _WIN32
        if (name == "dark" || name == "midnight" || name == "dracula" || name == "cyberpunk") {
            set_style("background: #18181b; color: #f4f4f5;");
            editBgColor = RGB(39, 39, 42);
            editTextColor = RGB(244, 244, 246);
        } else {
            set_style("background: #ffffff; color: #18181b;");
            editBgColor = RGB(255, 255, 255);
            editTextColor = RGB(24, 24, 27);
        }
        if (editBrush) DeleteObject(editBrush);
        editBrush = CreateSolidBrush(editBgColor);
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
#else
        if (name == "dark" || name == "midnight" || name == "dracula" || name == "cyberpunk") {
            linuxBgColor = Color{24, 24, 27, 255};
            linuxTextColor = Color{244, 244, 246, 255};
            linuxItemBgColor = Color{39, 39, 42, 255};
            linuxItemTextColor = Color{244, 244, 246, 255};
            linuxBorderColor = Color{63, 63, 70, 255};
            linuxButtonBgColor = Color{37, 99, 235, 255};
            linuxButtonHoverColor = Color{59, 130, 246, 255};
        } else {
            linuxBgColor = Color{255, 255, 255, 255};
            linuxTextColor = Color{24, 24, 27, 255};
            linuxItemBgColor = Color{244, 244, 246, 255};
            linuxItemTextColor = Color{24, 24, 27, 255};
            linuxBorderColor = Color{212, 212, 216, 255};
            linuxButtonBgColor = Color{37, 99, 235, 255};
            linuxButtonHoverColor = Color{29, 78, 216, 255};
        }
#endif
    }

    void run() {
#ifdef _WIN32
        if (!hwnd) return;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        MSG msg = {};
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#else
        const char* dpy = std::getenv("DISPLAY");
        const char* way = std::getenv("WAYLAND_DISPLAY");
        if (!dpy && !way) {
            std::cout << "[RayQuiro App] " << linuxTitle << std::endl;
            for (const auto& c : linuxControls) {
                if (c.type == LinuxControl::Button) std::cout << "  [Button] " << c.text << std::endl;
                else if (c.type == LinuxControl::Text) std::cout << "  [Text] " << c.text << std::endl;
                else if (c.type == LinuxControl::Input) std::cout << "  [Input] " << c.text << std::endl;
                else if (c.type == LinuxControl::Checkbox) std::cout << "  [Checkbox] " << c.text << (c.checked ? " [x]" : " [ ]") << std::endl;
            }
            return;
        }

        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(linuxWidth, linuxHeight, linuxTitle.c_str());
        SetTargetFPS(60);

        Font appFont = GetFontDefault();
        bool customFont = false;
        const char* fontCandidates[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf"
        };
        for (const char* path : fontCandidates) {
            if (std::filesystem::exists(path)) {
                int codepoints[512];
                for (int i = 0; i < 256; ++i) codepoints[i] = 32 + i;
                for (int i = 0; i < 256; ++i) codepoints[256 + i] = 0x0400 + i;
                appFont = LoadFontEx(path, 18, codepoints, 512);
                if (appFont.texture.id > 0) {
                    customFont = true;
                    break;
                }
            }
        }

        linuxRunning = true;
        int activeInput = -1;

        while (!WindowShouldClose() && linuxRunning) {
            Vector2 mouse = GetMousePosition();
            bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

            if (activeInput >= 0 && activeInput < static_cast<int>(linuxControls.size())) {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32 && key <= 126) || (key >= 0x0400 && key <= 0x04FF)) {
                        if (key < 0x80) {
                            linuxControls[activeInput].value.push_back(static_cast<char>(key));
                        } else {
                            linuxControls[activeInput].value.push_back(static_cast<char>(0xC0 | ((key >> 6) & 0x1F)));
                            linuxControls[activeInput].value.push_back(static_cast<char>(0x80 | (key & 0x3F)));
                        }
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (!linuxControls[activeInput].value.empty()) {
                        unsigned char c = static_cast<unsigned char>(linuxControls[activeInput].value.back());
                        linuxControls[activeInput].value.pop_back();
                        if ((c & 0xC0) == 0x80 && !linuxControls[activeInput].value.empty()) {
                            linuxControls[activeInput].value.pop_back();
                        }
                    }
                }
            }

            for (size_t i = 0; i < linuxControls.size(); ++i) {
                auto& ctrl = linuxControls[i];
                Rectangle r{static_cast<float>(ctrl.x), static_cast<float>(ctrl.y), static_cast<float>(ctrl.w), static_cast<float>(ctrl.h)};
                bool hover = CheckCollisionPointRec(mouse, r);
                if (hover && click) {
                    if (ctrl.type == LinuxControl::Input) {
                        activeInput = static_cast<int>(i);
                    } else if (ctrl.type == LinuxControl::Checkbox) {
                        ctrl.checked = !ctrl.checked;
                    }
                }
            }

            BeginDrawing();
            ClearBackground(linuxBgColor);

            for (size_t i = 0; i < linuxControls.size(); ++i) {
                auto& ctrl = linuxControls[i];
                Rectangle r{static_cast<float>(ctrl.x), static_cast<float>(ctrl.y), static_cast<float>(ctrl.w), static_cast<float>(ctrl.h)};
                bool hover = CheckCollisionPointRec(mouse, r);

                if (ctrl.type == LinuxControl::Button) {
                    Color bg = hover ? linuxButtonHoverColor : linuxButtonBgColor;
                    if (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                        bg.r = static_cast<unsigned char>(std::max(0, static_cast<int>(bg.r) - 25));
                        bg.g = static_cast<unsigned char>(std::max(0, static_cast<int>(bg.g) - 25));
                        bg.b = static_cast<unsigned char>(std::max(0, static_cast<int>(bg.b) - 25));
                    }
                    DrawRectangleRounded(r, 0.2f, 4, bg);
                    DrawRectangleRoundedLines(r, 0.2f, 4, linuxBorderColor);
                    Vector2 sz = customFont ? MeasureTextEx(appFont, ctrl.text.c_str(), 16.0f, 1.0f) : Vector2{static_cast<float>(MeasureText(ctrl.text.c_str(), 16)), 16.0f};
                    float tx = r.x + (r.width - sz.x) / 2.0f;
                    float ty = r.y + (r.height - sz.y) / 2.0f;
                    if (customFont) {
                        DrawTextEx(appFont, ctrl.text.c_str(), Vector2{tx, ty}, 16.0f, 1.0f, Color{255, 255, 255, 255});
                    } else {
                        DrawText(ctrl.text.c_str(), static_cast<int>(tx), static_cast<int>(ty), 16, Color{255, 255, 255, 255});
                    }
                } else if (ctrl.type == LinuxControl::Text) {
                    Vector2 sz = customFont ? MeasureTextEx(appFont, ctrl.text.c_str(), 16.0f, 1.0f) : Vector2{static_cast<float>(MeasureText(ctrl.text.c_str(), 16)), 16.0f};
                    float ty = r.y + (r.height - sz.y) / 2.0f;
                    if (customFont) {
                        DrawTextEx(appFont, ctrl.text.c_str(), Vector2{r.x, ty}, 16.0f, 1.0f, linuxTextColor);
                    } else {
                        DrawText(ctrl.text.c_str(), static_cast<int>(r.x), static_cast<int>(ty), 16, linuxTextColor);
                    }
                } else if (ctrl.type == LinuxControl::Input) {
                    DrawRectangleRounded(r, 0.15f, 4, linuxItemBgColor);
                    Color border = (static_cast<int>(i) == activeInput) ? linuxButtonBgColor : linuxBorderColor;
                    DrawRectangleRoundedLines(r, 0.15f, 4, border);
                    std::string display = ctrl.value.empty() ? ctrl.text : ctrl.value;
                    Color tc = ctrl.value.empty() ? Color{161, 161, 170, 255} : linuxItemTextColor;
                    if (customFont) {
                        DrawTextEx(appFont, display.c_str(), Vector2{r.x + 8.0f, r.y + (r.height - 16.0f) / 2.0f}, 16.0f, 1.0f, tc);
                    } else {
                        DrawText(display.c_str(), static_cast<int>(r.x + 8.0f), static_cast<int>(r.y + (r.height - 16.0f) / 2.0f), 16, tc);
                    }
                    if (static_cast<int>(i) == activeInput && (static_cast<int>(GetTime() * 2) % 2 == 0)) {
                        Vector2 sz = customFont ? MeasureTextEx(appFont, ctrl.value.c_str(), 16.0f, 1.0f) : Vector2{static_cast<float>(MeasureText(ctrl.value.c_str(), 16)), 16.0f};
                        DrawLine(static_cast<int>(r.x + 10.0f + sz.x), static_cast<int>(r.y + 6.0f), static_cast<int>(r.x + 10.0f + sz.x), static_cast<int>(r.y + r.height - 6.0f), linuxTextColor);
                    }
                } else if (ctrl.type == LinuxControl::Checkbox) {
                    Rectangle box{r.x, r.y + (r.height - 18.0f) / 2.0f, 18.0f, 18.0f};
                    DrawRectangleRounded(box, 0.2f, 4, linuxItemBgColor);
                    DrawRectangleRoundedLines(box, 0.2f, 4, ctrl.checked ? linuxButtonBgColor : linuxBorderColor);
                    if (ctrl.checked) {
                        DrawRectangleRounded(Rectangle{box.x + 3.0f, box.y + 3.0f, 12.0f, 12.0f}, 0.2f, 4, linuxButtonBgColor);
                    }
                    if (customFont) {
                        DrawTextEx(appFont, ctrl.text.c_str(), Vector2{r.x + 26.0f, r.y + (r.height - 16.0f) / 2.0f}, 16.0f, 1.0f, linuxTextColor);
                    } else {
                        DrawText(ctrl.text.c_str(), static_cast<int>(r.x + 26.0f), static_cast<int>(r.y + (r.height - 16.0f) / 2.0f), 16, linuxTextColor);
                    }
                }
            }

            EndDrawing();
        }

        if (customFont) UnloadFont(appFont);
        CloseWindow();
#endif
    }

    void close() {
#ifdef _WIN32
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = NULL;
        }
        PostQuitMessage(0);
#else
        linuxRunning = false;
#endif
    }

    void cleanup() {
#ifdef _WIN32
        if (bgBrush) { DeleteObject(bgBrush); bgBrush = NULL; }
        if (editBrush) { DeleteObject(editBrush); editBrush = NULL; }
        if (hFont) { DeleteObject(hFont); hFont = NULL; }
        controls.clear();
        hwnd = NULL;
#else
        linuxControls.clear();
#endif
    }

    static void show_message(std::string title, std::string text) {
#ifdef _WIN32
        std::wstring wt = utf8_to_wide(title);
        std::wstring wm = utf8_to_wide(text);
        MessageBoxW(NULL, wm.c_str(), wt.c_str(), MB_OK | MB_ICONINFORMATION);
#else
        std::cout << "[" << title << "] " << text << std::endl;
#endif
    }
};
