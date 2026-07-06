#pragma once

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

class RayQuiroModernUI {
    enum class TextAlign {
        Left,
        Center,
        Right
    };

    struct InfoRow {
        std::string label;
        std::string value;
    };

    struct TextBlock {
        std::string text;
        std::string role;
    };

    struct ActionButton {
        std::string id;
        std::string label;
        std::string variant;
        RECT rect{};
    };

    struct Theme {
        COLORREF windowStart = RGB(11, 24, 43);
        COLORREF windowEnd = RGB(3, 120, 124);
        COLORREF cardBackground = RGB(245, 248, 252);
        COLORREF cardBorder = RGB(220, 228, 238);
        COLORREF badgeBackground = RGB(31, 145, 212);
        COLORREF badgeText = RGB(255, 255, 255);
        COLORREF titleColor = RGB(18, 28, 44);
        COLORREF subtitleColor = RGB(78, 95, 119);
        COLORREF statusBackground = RGB(230, 242, 250);
        COLORREF statusBorder = RGB(199, 221, 235);
        COLORREF statusTitleColor = RGB(14, 47, 79);
        COLORREF statusBodyColor = RGB(50, 76, 101);
        COLORREF labelColor = RGB(105, 118, 136);
        COLORREF valueColor = RGB(26, 39, 56);
        COLORREF textColor = RGB(37, 51, 70);
        COLORREF mutedTextColor = RGB(112, 124, 142);
        COLORREF primaryButtonBackground = RGB(14, 116, 144);
        COLORREF primaryButtonBackgroundHover = RGB(16, 145, 108);
        COLORREF primaryButtonText = RGB(255, 255, 255);
        COLORREF primaryButtonBorder = RGB(14, 116, 144);
        COLORREF secondaryButtonBackground = RGB(234, 240, 245);
        COLORREF secondaryButtonBackgroundHover = RGB(222, 231, 238);
        COLORREF secondaryButtonText = RGB(19, 35, 53);
        COLORREF secondaryButtonBorder = RGB(205, 216, 226);
        int cardRadius = 24;
        int buttonRadius = 18;
        int titleSize = 34;
        int subtitleSize = 20;
        int statusTitleSize = 22;
        int statusBodySize = 18;
        int labelSize = 17;
        int valueSize = 17;
        int textSize = 18;
        int mutedTextSize = 17;
        int buttonTextSize = 18;
        int maxCardWidth = 860;
        TextAlign titleAlign = TextAlign::Center;
        TextAlign subtitleAlign = TextAlign::Center;
        TextAlign statusTitleAlign = TextAlign::Left;
        TextAlign statusBodyAlign = TextAlign::Left;
        TextAlign textAlign = TextAlign::Left;
        TextAlign buttonAlign = TextAlign::Center;
        std::string fontFamily = "Segoe UI";
    };

    HWND hwnd = NULL;
    HINSTANCE hInst = NULL;
    Theme theme;
    std::string styleSource;
    std::string windowTitle = "RayQuiro UI";
    std::string heroTitle;
    std::string heroSubtitle;
    std::string statusTitle;
    std::string statusBody;
    std::vector<InfoRow> infoRows;
    std::vector<TextBlock> textBlocks;
    std::vector<ActionButton> actionButtons;
    std::string clickedAction = "close";
    int hoveredButton = -1;
    int windowWidth = 920;
    int windowHeight = 620;

public:
    RayQuiroModernUI() : hInst(GetModuleHandleA(NULL)) {}

    void init(const std::string& title, int width, int height) {
        resetContent();
        resetTheme();
        windowTitle = title;
        windowWidth = width;
        windowHeight = height;
        clickedAction = "close";
        hoveredButton = -1;
        createWindow();
    }

    void style(const std::string& cssLike) {
        styleSource = cssLike;
        applyStyles(cssLike);
        redraw();
    }

    void hero(const std::string& title, const std::string& subtitle) {
        heroTitle = title;
        heroSubtitle = subtitle;
        redraw();
    }

    void status(const std::string& title, const std::string& body) {
        statusTitle = title;
        statusBody = body;
        redraw();
    }

    void info(const std::string& label, const std::string& value) {
        infoRows.push_back(InfoRow{label, value});
        redraw();
    }

    void text(const std::string& body, const std::string& role = "body") {
        textBlocks.push_back(TextBlock{body, role});
        redraw();
    }

    void action(const std::string& id, const std::string& label, const std::string& variant = "primary") {
        actionButtons.push_back(ActionButton{id, label, variant, {}});
        redraw();
    }

    std::string run() {
        if (hwnd == NULL) {
            createWindow();
        }

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg = {};
        while (hwnd != NULL && GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (hwnd == NULL) {
                break;
            }
        }

        return clickedAction;
    }

private:
    static RayQuiroModernUI* fromWindow(HWND hwnd) {
        return reinterpret_cast<RayQuiroModernUI*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    static std::string trim(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }

        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }

        return value.substr(start, end - start);
    }

    static std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    static COLORREF blend(COLORREF a, COLORREF b, double t) {
        const int ar = GetRValue(a);
        const int ag = GetGValue(a);
        const int ab = GetBValue(a);
        const int br = GetRValue(b);
        const int bg = GetGValue(b);
        const int bb = GetBValue(b);
        return RGB(
            static_cast<int>(ar + (br - ar) * t),
            static_cast<int>(ag + (bg - ag) * t),
            static_cast<int>(ab + (bb - ab) * t));
    }

    static COLORREF parseColor(const std::string& rawValue, COLORREF fallback) {
        const std::string value = trim(rawValue);
        if (value.size() == 7 && value[0] == '#') {
            try {
                const int r = std::stoi(value.substr(1, 2), nullptr, 16);
                const int g = std::stoi(value.substr(3, 2), nullptr, 16);
                const int b = std::stoi(value.substr(5, 2), nullptr, 16);
                return RGB(r, g, b);
            } catch (...) {
                return fallback;
            }
        }
        return fallback;
    }

    static int parseNumber(const std::string& rawValue, int fallback) {
        try {
            return std::stoi(trim(rawValue));
        } catch (...) {
            return fallback;
        }
    }

    static TextAlign parseAlign(const std::string& rawValue, TextAlign fallback) {
        const std::string value = toLower(trim(rawValue));
        if (value == "center") return TextAlign::Center;
        if (value == "right") return TextAlign::Right;
        if (value == "left") return TextAlign::Left;
        return fallback;
    }

    static UINT textAlignFlags(TextAlign align) {
        switch (align) {
        case TextAlign::Center:
            return DT_CENTER;
        case TextAlign::Right:
            return DT_RIGHT;
        default:
            return DT_LEFT;
        }
    }

    void resetTheme() {
        theme = Theme{};
    }

    void resetContent() {
        infoRows.clear();
        textBlocks.clear();
        actionButtons.clear();
        heroTitle.clear();
        heroSubtitle.clear();
        statusTitle.clear();
        statusBody.clear();
        styleSource.clear();
    }

    void redraw() {
        if (hwnd != NULL) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }

    void createWindow() {
        if (hwnd != NULL) {
            DestroyWindow(hwnd);
            hwnd = NULL;
        }

        static bool registered = false;
        if (!registered) {
            WNDCLASSA wc = {};
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = hInst;
            wc.lpszClassName = "RayQuiroModernUIWindow";
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            RegisterClassA(&wc);
            registered = true;
        }

        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        const int x = (screenW - windowWidth) / 2;
        const int y = (screenH - windowHeight) / 2;

        hwnd = CreateWindowExA(
            0,
            "RayQuiroModernUIWindow",
            windowTitle.c_str(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            x,
            y,
            windowWidth,
            windowHeight,
            NULL,
            NULL,
            hInst,
            this);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }

        RayQuiroModernUI* self = fromWindow(hwnd);
        if (self == NULL) {
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        }

        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            self->handleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSELEAVE:
            self->hoveredButton = -1;
            self->redraw();
            return 0;
        case WM_LBUTTONUP:
            self->handleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_PAINT:
            self->paint();
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            self->hwnd = NULL;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        }
    }

    void handleMouseMove(int x, int y) {
        TRACKMOUSEEVENT event = {};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = hwnd;
        TrackMouseEvent(&event);

        int newHover = -1;
        POINT point{x, y};
        for (size_t i = 0; i < actionButtons.size(); ++i) {
            if (PtInRect(&actionButtons[i].rect, point)) {
                newHover = static_cast<int>(i);
                break;
            }
        }

        if (newHover != hoveredButton) {
            hoveredButton = newHover;
            redraw();
        }
    }

    void handleClick(int x, int y) {
        POINT point{x, y};
        for (const auto& button : actionButtons) {
            if (PtInRect(&button.rect, point)) {
                clickedAction = button.id;
                DestroyWindow(hwnd);
                return;
            }
        }
    }

    void fillRect(HDC hdc, const RECT& rect, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }

    void fillRoundRect(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius) {
        HBRUSH brush = CreateSolidBrush(fillColor);
        HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    HFONT makeFont(int size, int weight) {
        return CreateFontA(
            size,
            0,
            0,
            0,
            weight,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            VARIABLE_PITCH,
            theme.fontFamily.c_str());
    }

    void drawTextLine(HDC hdc, const std::string& text, const RECT& rect, COLORREF color, int size, int weight, TextAlign align = TextAlign::Left, bool verticalCenter = false) {
        HFONT font = makeFont(size, weight);
        HGDIOBJ oldFont = SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, color);
        UINT flags = textAlignFlags(align) | DT_WORDBREAK | DT_NOPREFIX;
        if (verticalCenter) {
            flags &= ~DT_WORDBREAK;
            flags |= DT_VCENTER | DT_SINGLELINE;
        }
        DrawTextA(hdc, text.c_str(), -1, const_cast<RECT*>(&rect), flags);
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    void paintBackground(HDC hdc, const RECT& client) {
        const int height = std::max<int>(1, static_cast<int>(client.bottom - client.top));
        for (int y = 0; y < height; ++y) {
            RECT line{client.left, y, client.right, y + 1};
            fillRect(hdc, line, blend(theme.windowStart, theme.windowEnd, static_cast<double>(y) / static_cast<double>(height)));
        }
    }

    COLORREF buttonFillColor(const ActionButton& button, bool hovered) const {
        const std::string variant = toLower(button.variant);
        if (variant == "secondary" || variant == "ghost") {
            return hovered ? theme.secondaryButtonBackgroundHover : theme.secondaryButtonBackground;
        }
        return hovered ? theme.primaryButtonBackgroundHover : theme.primaryButtonBackground;
    }

    COLORREF buttonBorderColor(const ActionButton& button) const {
        const std::string variant = toLower(button.variant);
        if (variant == "secondary" || variant == "ghost") {
            return theme.secondaryButtonBorder;
        }
        return theme.primaryButtonBorder;
    }

    COLORREF buttonTextColor(const ActionButton& button) const {
        const std::string variant = toLower(button.variant);
        if (variant == "secondary" || variant == "ghost") {
            return theme.secondaryButtonText;
        }
        return theme.primaryButtonText;
    }

    int paintInfoRows(HDC hdc, RECT card, int top) {
        for (size_t i = 0; i < infoRows.size(); ++i) {
            RECT labelRect{card.left + 28, top + static_cast<int>(i) * 34, card.left + 180, top + 28 + static_cast<int>(i) * 34};
            RECT valueRect{card.left + 182, top + static_cast<int>(i) * 34, card.right - 28, top + 28 + static_cast<int>(i) * 34};
            drawTextLine(hdc, infoRows[i].label, labelRect, theme.labelColor, theme.labelSize, FW_BOLD, TextAlign::Left);
            drawTextLine(hdc, infoRows[i].value, valueRect, theme.valueColor, theme.valueSize, FW_NORMAL, TextAlign::Left);
        }
        return top + static_cast<int>(infoRows.size()) * 34;
    }

    int paintTextBlocks(HDC hdc, RECT card, int top) {
        int y = top;
        for (const auto& block : textBlocks) {
            RECT rect{card.left + 28, y, card.right - 28, y + 40};
            const std::string role = toLower(block.role);
            COLORREF color = theme.textColor;
            int size = theme.textSize;
            int weight = FW_NORMAL;
            if (role == "muted" || role == "caption") {
                color = theme.mutedTextColor;
                size = theme.mutedTextSize;
            } else if (role == "title") {
                color = theme.titleColor;
                size = theme.statusTitleSize;
                weight = FW_BOLD;
            }
            drawTextLine(hdc, block.text, rect, color, size, weight, theme.textAlign);
            y += role == "muted" ? 28 : 34;
        }
        return y;
    }

    void paintButtons(HDC hdc, RECT card, int y) {
        const int buttonW = 170;
        const int buttonH = 44;
        const int gap = 16;
        const int totalWidth = static_cast<int>(actionButtons.size()) * buttonW + std::max<int>(0, static_cast<int>(actionButtons.size()) - 1) * gap;
        const int startX = std::max<int>(card.left + 28, card.left + ((card.right - card.left) - totalWidth) / 2);
        for (size_t i = 0; i < actionButtons.size(); ++i) {
            RECT rect{
                startX + static_cast<int>(i) * (buttonW + gap),
                y,
                startX + static_cast<int>(i) * (buttonW + gap) + buttonW,
                y + buttonH};
            actionButtons[i].rect = rect;
            fillRoundRect(
                hdc,
                rect,
                buttonFillColor(actionButtons[i], hoveredButton == static_cast<int>(i)),
                buttonBorderColor(actionButtons[i]),
                theme.buttonRadius);
            drawTextLine(
                hdc,
                actionButtons[i].label,
                rect,
                buttonTextColor(actionButtons[i]),
                theme.buttonTextSize,
                FW_BOLD,
                theme.buttonAlign,
                true);
        }
    }

    void paintCard(HDC hdc, RECT card) {
        fillRoundRect(hdc, card, theme.cardBackground, theme.cardBorder, theme.cardRadius);

        RECT badge{card.left + 28, card.top + 26, card.left + 160, card.top + 56};
        fillRoundRect(hdc, badge, theme.badgeBackground, theme.badgeBackground, theme.buttonRadius);
        drawTextLine(hdc, "RAYQUIRO UI", badge, theme.badgeText, 18, FW_BOLD, TextAlign::Center, true);

        RECT heroRect{card.left + 28, card.top + 78, card.right - 28, card.top + 150};
        drawTextLine(hdc, heroTitle, heroRect, theme.titleColor, theme.titleSize, FW_BOLD, theme.titleAlign);

        RECT subtitleRect{card.left + 28, card.top + 142, card.right - 36, card.top + 208};
        drawTextLine(hdc, heroSubtitle, subtitleRect, theme.subtitleColor, theme.subtitleSize, FW_NORMAL, theme.subtitleAlign);

        RECT statusRect{card.left + 28, card.top + 224, card.right - 28, card.top + 320};
        fillRoundRect(hdc, statusRect, theme.statusBackground, theme.statusBorder, theme.cardRadius);
        RECT statusTitleRect{statusRect.left + 18, statusRect.top + 16, statusRect.right - 18, statusRect.top + 48};
        RECT statusBodyRect{statusRect.left + 18, statusRect.top + 54, statusRect.right - 18, statusRect.bottom - 16};
        drawTextLine(hdc, statusTitle, statusTitleRect, theme.statusTitleColor, theme.statusTitleSize, FW_BOLD, theme.statusTitleAlign);
        drawTextLine(hdc, statusBody, statusBodyRect, theme.statusBodyColor, theme.statusBodySize, FW_NORMAL, theme.statusBodyAlign);

        int top = statusRect.bottom + 24;
        top = paintInfoRows(hdc, card, top);
        if (!infoRows.empty()) {
            top += 10;
        }
        top = paintTextBlocks(hdc, card, top);

        const int buttonY = std::min<int>(static_cast<int>(card.bottom - 84), top + 20);
        paintButtons(hdc, card, buttonY);
    }

    void paint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);
        paintBackground(hdc, client);

        const int clientWidth = client.right - client.left;
        const int cardWidth = std::min<int>(theme.maxCardWidth, std::max<int>(clientWidth - 68, 420));
        RECT card{
            client.left + (clientWidth - cardWidth) / 2,
            client.top + 28,
            client.left + (clientWidth - cardWidth) / 2 + cardWidth,
            client.bottom - 28};
        paintCard(hdc, card);

        EndPaint(hwnd, &ps);
    }

    void applyStyles(const std::string& cssLike) {
        size_t cursor = 0;
        while (cursor < cssLike.size()) {
            const size_t selectorStart = cssLike.find_first_not_of(" \t\r\n", cursor);
            if (selectorStart == std::string::npos) {
                break;
            }

            const size_t braceOpen = cssLike.find('{', selectorStart);
            if (braceOpen == std::string::npos) {
                break;
            }
            const size_t braceClose = cssLike.find('}', braceOpen + 1);
            if (braceClose == std::string::npos) {
                break;
            }

            const std::string selector = toLower(trim(cssLike.substr(selectorStart, braceOpen - selectorStart)));
            const std::string body = cssLike.substr(braceOpen + 1, braceClose - braceOpen - 1);
            std::stringstream stream(body);
            std::string declaration;
            while (std::getline(stream, declaration, ';')) {
                const size_t colon = declaration.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                const std::string property = toLower(trim(declaration.substr(0, colon)));
                const std::string value = trim(declaration.substr(colon + 1));
                applyDeclaration(selector, property, value);
            }

            cursor = braceClose + 1;
        }
    }

    void applyDeclaration(const std::string& selector, const std::string& property, const std::string& value) {
        if (selector == "window") {
            if (property == "background") theme.windowStart = parseColor(value, theme.windowStart);
            if (property == "background-end") theme.windowEnd = parseColor(value, theme.windowEnd);
            if (property == "font") theme.fontFamily = trim(value);
            return;
        }

        if (selector == "card") {
            if (property == "background") theme.cardBackground = parseColor(value, theme.cardBackground);
            if (property == "border") theme.cardBorder = parseColor(value, theme.cardBorder);
            if (property == "radius") theme.cardRadius = parseNumber(value, theme.cardRadius);
            if (property == "max-width") theme.maxCardWidth = parseNumber(value, theme.maxCardWidth);
            return;
        }

        if (selector == "badge") {
            if (property == "background") theme.badgeBackground = parseColor(value, theme.badgeBackground);
            if (property == "color") theme.badgeText = parseColor(value, theme.badgeText);
            return;
        }

        if (selector == "title" || selector == "hero") {
            if (property == "color") theme.titleColor = parseColor(value, theme.titleColor);
            if (property == "size") theme.titleSize = parseNumber(value, theme.titleSize);
            if (property == "align") theme.titleAlign = parseAlign(value, theme.titleAlign);
            return;
        }

        if (selector == "subtitle") {
            if (property == "color") theme.subtitleColor = parseColor(value, theme.subtitleColor);
            if (property == "size") theme.subtitleSize = parseNumber(value, theme.subtitleSize);
            if (property == "align") theme.subtitleAlign = parseAlign(value, theme.subtitleAlign);
            return;
        }

        if (selector == "status-box") {
            if (property == "background") theme.statusBackground = parseColor(value, theme.statusBackground);
            if (property == "border") theme.statusBorder = parseColor(value, theme.statusBorder);
            return;
        }

        if (selector == "status-title") {
            if (property == "color") theme.statusTitleColor = parseColor(value, theme.statusTitleColor);
            if (property == "size") theme.statusTitleSize = parseNumber(value, theme.statusTitleSize);
            if (property == "align") theme.statusTitleAlign = parseAlign(value, theme.statusTitleAlign);
            return;
        }

        if (selector == "status-body") {
            if (property == "color") theme.statusBodyColor = parseColor(value, theme.statusBodyColor);
            if (property == "size") theme.statusBodySize = parseNumber(value, theme.statusBodySize);
            if (property == "align") theme.statusBodyAlign = parseAlign(value, theme.statusBodyAlign);
            return;
        }

        if (selector == "label") {
            if (property == "color") theme.labelColor = parseColor(value, theme.labelColor);
            if (property == "size") theme.labelSize = parseNumber(value, theme.labelSize);
            return;
        }

        if (selector == "value") {
            if (property == "color") theme.valueColor = parseColor(value, theme.valueColor);
            if (property == "size") theme.valueSize = parseNumber(value, theme.valueSize);
            return;
        }

        if (selector == "text" || selector == "text.body") {
            if (property == "color") theme.textColor = parseColor(value, theme.textColor);
            if (property == "size") theme.textSize = parseNumber(value, theme.textSize);
            if (property == "align") theme.textAlign = parseAlign(value, theme.textAlign);
            return;
        }

        if (selector == "text.muted") {
            if (property == "color") theme.mutedTextColor = parseColor(value, theme.mutedTextColor);
            if (property == "size") theme.mutedTextSize = parseNumber(value, theme.mutedTextSize);
            return;
        }

        if (selector == "button" || selector == "button.primary") {
            if (property == "background") theme.primaryButtonBackground = parseColor(value, theme.primaryButtonBackground);
            if (property == "background-hover") theme.primaryButtonBackgroundHover = parseColor(value, theme.primaryButtonBackgroundHover);
            if (property == "color") theme.primaryButtonText = parseColor(value, theme.primaryButtonText);
            if (property == "border") theme.primaryButtonBorder = parseColor(value, theme.primaryButtonBorder);
            if (property == "radius") theme.buttonRadius = parseNumber(value, theme.buttonRadius);
            if (property == "align") theme.buttonAlign = parseAlign(value, theme.buttonAlign);
            return;
        }

        if (selector == "button.secondary") {
            if (property == "background") theme.secondaryButtonBackground = parseColor(value, theme.secondaryButtonBackground);
            if (property == "background-hover") theme.secondaryButtonBackgroundHover = parseColor(value, theme.secondaryButtonBackgroundHover);
            if (property == "color") theme.secondaryButtonText = parseColor(value, theme.secondaryButtonText);
            if (property == "border") theme.secondaryButtonBorder = parseColor(value, theme.secondaryButtonBorder);
            if (property == "radius") theme.buttonRadius = parseNumber(value, theme.buttonRadius);
            if (property == "align") theme.buttonAlign = parseAlign(value, theme.buttonAlign);
        }
    }
};
