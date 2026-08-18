// The Windows implementation of IRoomPrompt: native controls via Win32
// (see opencv_room_prompt.cpp for the portable fallback).

#include "kfc/graphics/dialogs/room_prompt.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace kfc::graphics::dialogs {

namespace {

constexpr int kIdEdit = 1001;
constexpr int kIdCreate = 1002;
constexpr int kIdJoin = 1003;
constexpr int kIdCancel = 1004;

struct State {
    RoomChoice choice;
    HWND edit = nullptr;
};

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_COMMAND) {
        int id = LOWORD(wparam);
        if (id == kIdCreate || id == kIdJoin || id == kIdCancel) {
            if (st != nullptr) {
                if (id != kIdCancel) {
                    wchar_t wide[256] = {};
                    GetWindowTextW(st->edit, wide, 256);
                    char utf8[512] = {};
                    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
                    st->choice.room_id = utf8;
                }
                st->choice.action = id == kIdCreate  ? RoomChoice::Action::Create
                                    : id == kIdJoin ? RoomChoice::Action::Join
                                                    : RoomChoice::Action::Cancel;
            }
            DestroyWindow(hwnd);
            return 0;
        }
    } else if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    } else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

class WinRoomPrompt : public IRoomPrompt {
public:
    RoomChoice ask_room() override {
        HINSTANCE inst = GetModuleHandleW(nullptr);
        const wchar_t* cls = L"KfcRoomDialog";
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = proc;
            wc.hInstance = inst;
            wc.lpszClassName = cls;
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
            wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
            RegisterClassW(&wc);
            registered = true;
        }

        State st;
        HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, cls, L"Room",
                                    WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 350, 180,
                                    nullptr, nullptr, inst, nullptr);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
        CreateWindowW(L"STATIC", L"Room id (to Join). Create makes a new one:", WS_CHILD | WS_VISIBLE, 15, 15, 310,
                      20, hwnd, nullptr, inst, nullptr);
        st.edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 15, 40, 305, 26,
                                hwnd, reinterpret_cast<HMENU>(kIdEdit), inst, nullptr);
        CreateWindowW(L"BUTTON", L"Create", WS_CHILD | WS_VISIBLE, 15, 92, 95, 34, hwnd,
                      reinterpret_cast<HMENU>(kIdCreate), inst, nullptr);
        CreateWindowW(L"BUTTON", L"Join", WS_CHILD | WS_VISIBLE, 125, 92, 95, 34, hwnd,
                      reinterpret_cast<HMENU>(kIdJoin), inst, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 235, 92, 95, 34, hwnd,
                      reinterpret_cast<HMENU>(kIdCancel), inst, nullptr);
        SetFocus(st.edit);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return st.choice;
    }

    void show_message(const std::string& title, const std::string& text) override {
        MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    }
};

}  // namespace

std::unique_ptr<IRoomPrompt> make_room_prompt() {
    return std::make_unique<WinRoomPrompt>();
}

}  // namespace kfc::graphics::dialogs
