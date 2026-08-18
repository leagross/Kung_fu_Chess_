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

constexpr int kIdUserEdit = 1005;
constexpr int kIdPassEdit = 1006;
constexpr int kIdLogin = 1007;
constexpr int kIdLoginCancel = 1008;

struct State {
    RoomChoice choice;
    HWND edit = nullptr;
};

struct LoginState {
    LoginChoice choice;
    HWND user_edit = nullptr;
    HWND pass_edit = nullptr;
};

// GetWindowTextW + WideCharToMultiByte, in one place -- both edit controls in
// login_proc below need exactly this, and State's own proc has no equivalent
// helper because it only ever reads one control.
std::string read_edit_utf8(HWND edit) {
    wchar_t wide[256] = {};
    GetWindowTextW(edit, wide, 256);
    char utf8[512] = {};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

LRESULT CALLBACK login_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* st = reinterpret_cast<LoginState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_COMMAND) {
        int id = LOWORD(wparam);
        if (id == kIdLogin || id == kIdLoginCancel) {
            if (st != nullptr) {
                if (id == kIdLogin) {
                    st->choice.username = read_edit_utf8(st->user_edit);
                    st->choice.password = read_edit_utf8(st->pass_edit);
                    st->choice.cancelled = false;
                } else {
                    st->choice.cancelled = true;
                }
            }
            DestroyWindow(hwnd);
            return 0;
        }
    } else if (msg == WM_CLOSE) {
        if (st != nullptr) {
            st->choice.cancelled = true;
        }
        DestroyWindow(hwnd);
        return 0;
    } else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

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
    LoginChoice ask_login() override {
        HINSTANCE inst = GetModuleHandleW(nullptr);
        const wchar_t* cls = L"KfcLoginDialog";
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = login_proc;
            wc.hInstance = inst;
            wc.lpszClassName = cls;
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
            wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
            RegisterClassW(&wc);
            registered = true;
        }

        LoginState st;
        HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, cls, L"Kung Fu Chess -- Login",
                                    WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 350, 230,
                                    nullptr, nullptr, inst, nullptr);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
        CreateWindowW(L"STATIC", L"Username:", WS_CHILD | WS_VISIBLE, 15, 15, 310, 20, hwnd, nullptr, inst, nullptr);
        st.user_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 15, 38, 305,
                                     26, hwnd, reinterpret_cast<HMENU>(kIdUserEdit), inst, nullptr);
        CreateWindowW(L"STATIC", L"Password:", WS_CHILD | WS_VISIBLE, 15, 72, 310, 20, hwnd, nullptr, inst, nullptr);
        // ES_PASSWORD masks every character as it's typed -- a real
        // improvement over the terminal prompt this dialog replaces, which
        // showed nothing typed at all (see read_password_masked's own doc
        // comment in game_session.cpp for why that used _getch() instead of
        // an ordinary getline in the first place: never sitting in shell
        // history is the actual requirement, not literally showing nothing).
        st.pass_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                                     15, 95, 305, 26, hwnd, reinterpret_cast<HMENU>(kIdPassEdit), inst, nullptr);
        CreateWindowW(L"STATIC",
                      L"A username not seen before registers automatically with the password typed here.",
                      WS_CHILD | WS_VISIBLE, 15, 128, 310, 40, hwnd, nullptr, inst, nullptr);
        CreateWindowW(L"BUTTON", L"Login", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 15, 172, 145, 34, hwnd,
                      reinterpret_cast<HMENU>(kIdLogin), inst, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 175, 172, 145, 34, hwnd,
                      reinterpret_cast<HMENU>(kIdLoginCancel), inst, nullptr);
        SetFocus(st.user_edit);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            // Enter submits from either field, Tab moves between them --
            // neither works for free the way it would inside a real dialog
            // box (this is a plain window, not a DS_CONTROL dialog template),
            // so both are handled by hand here rather than left broken.
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                SendMessageW(hwnd, WM_COMMAND, kIdLogin, 0);
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
                HWND focused = GetFocus();
                SetFocus(focused == st.user_edit ? st.pass_edit : st.user_edit);
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return st.choice;
    }

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
