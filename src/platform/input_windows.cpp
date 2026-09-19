#include "PlatformInput.h"

#include <windows.h>

namespace ezac {

// GetAsyncKeyState / SendInput both work regardless of which window has
// focus, which is what lets the click loop run globally. Note: if the
// foreground app is elevated (running as admin) and this process is not,
// Windows' UIPI will block synthetic input into it, so the app must then be
// run elevated too. This mirrors the limitation of every other autoclicker.
class WindowsInput : public PlatformInput {
public:
    WindowsInput() {
        // Raise the global timer resolution so std::this_thread::sleep_for
        // in the click loop can actually land close to 1ms instead of the
        // default ~15.6ms Windows tick.
        timeBeginPeriod(1);
    }

    ~WindowsInput() override { timeEndPeriod(1); }

    bool isKeyHeld(int keyCode) override {
        return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
    }

    void sendClick(MouseButton button) override {
        DWORD down = MOUSEEVENTF_LEFTDOWN, up = MOUSEEVENTF_LEFTUP;
        switch (button) {
            case MouseButton::Right:
                down = MOUSEEVENTF_RIGHTDOWN;
                up = MOUSEEVENTF_RIGHTUP;
                break;
            case MouseButton::Middle:
                down = MOUSEEVENTF_MIDDLEDOWN;
                up = MOUSEEVENTF_MIDDLEUP;
                break;
            case MouseButton::Left:
                break;
        }

        INPUT input[2] = {};
        input[0].type = INPUT_MOUSE;
        input[0].mi.dwFlags = down;
        input[1].type = INPUT_MOUSE;
        input[1].mi.dwFlags = up;
        SendInput(2, input, sizeof(INPUT));
    }

    const char* statusMessage() const override { return ""; }
};

PlatformInput* PlatformInput::create() { return new WindowsInput(); }

const std::vector<KeyOption>& bindableKeyOptions() {
    static const std::vector<KeyOption> keys = [] {
        std::vector<KeyOption> v;
        static const char* letterNames[] = {
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
            "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        };
        for (int i = 0; i < 26; ++i) v.push_back({letterNames[i], 'A' + i});

        static const char* digitNames[] = {"0", "1", "2", "3", "4",
                                            "5", "6", "7", "8", "9"};
        for (int i = 0; i < 10; ++i) v.push_back({digitNames[i], '0' + i});

        static const char* fNames[] = {"F1", "F2", "F3",  "F4",  "F5",  "F6",
                                        "F7", "F8", "F9",  "F10", "F11", "F12"};
        for (int i = 0; i < 12; ++i) v.push_back({fNames[i], VK_F1 + i});

        v.push_back({"Space", VK_SPACE});
        v.push_back({"Tab", VK_TAB});
        v.push_back({"Caps Lock", VK_CAPITAL});
        v.push_back({"Left Shift", VK_LSHIFT});
        v.push_back({"Right Shift", VK_RSHIFT});
        v.push_back({"Left Ctrl", VK_LCONTROL});
        v.push_back({"Right Ctrl", VK_RCONTROL});
        v.push_back({"Left Alt", VK_LMENU});
        v.push_back({"Right Alt", VK_RMENU});
        v.push_back({"`", VK_OEM_3});
        return v;
    }();
    return keys;
}

int defaultTriggerKeyCode() { return 'F'; }

std::string keyDisplayName(int keyCode) {
    for (const auto& k : bindableKeyOptions()) {
        if (k.code == keyCode) return k.name;
    }
    return "Key " + std::to_string(keyCode);
}

} // namespace ezac
