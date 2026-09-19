#include "PlatformInput.h"

#include <atomic>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <mutex>
#include <poll.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace ezac {
namespace {

constexpr size_t kBitsPerLong = 8 * sizeof(unsigned long);

size_t bitsToLongs(size_t bits) { return (bits + kBitsPerLong - 1) / kBitsPerLong; }

bool testBit(const unsigned long* bits, int bit) {
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1UL;
}

} // namespace

// Uses raw evdev to detect key holds and a virtual uinput mouse to inject
// clicks. Both work under X11 and Wayland alike since they operate below
// the display server, at the kernel input layer (the same mechanism tools
// like ydotool rely on for Wayland compatibility). This needs the user to
// be able to read /dev/input/event* and write /dev/uinput; see
// udev/99-ezautoclicker.rules and the README for the one-time setup.
class LinuxInput : public PlatformInput {
public:
    LinuxInput() {
        openKeyboards();
        openUinput();
        if (!keyboardFds_.empty()) {
            watchThread_ = std::thread(&LinuxInput::watchLoop, this);
        }
    }

    ~LinuxInput() override {
        stop_.store(true, std::memory_order_relaxed);
        if (watchThread_.joinable()) watchThread_.join();
        for (int fd : keyboardFds_) close(fd);
        if (uinputFd_ >= 0) {
            ioctl(uinputFd_, UI_DEV_DESTROY);
            close(uinputFd_);
        }
    }

    bool isKeyHeld(int keyCode) override {
        std::lock_guard<std::mutex> lock(keyMutex_);
        auto it = keyDown_.find(keyCode);
        return it != keyDown_.end() && it->second;
    }

    void sendClick(MouseButton button) override {
        if (uinputFd_ < 0) return;
        int code = BTN_LEFT;
        if (button == MouseButton::Right) code = BTN_RIGHT;
        else if (button == MouseButton::Middle) code = BTN_MIDDLE;

        emit(EV_KEY, code, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        emit(EV_KEY, code, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }

    const char* statusMessage() const override { return status_.c_str(); }

private:
    void addStatus(const std::string& msg) {
        if (!status_.empty()) status_ += " ";
        status_ += msg;
    }

    void openKeyboards() {
        DIR* dir = opendir("/dev/input");
        if (!dir) {
            addStatus("Cannot open /dev/input (permission denied). "
                       "Add your user to the 'input' group and re-login.");
            return;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            const std::string name = entry->d_name;
            if (name.rfind("event", 0) != 0) continue;
            const std::string path = "/dev/input/" + name;
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            std::vector<unsigned long> evBits(bitsToLongs(EV_MAX + 1));
            if (ioctl(fd, EVIOCGBIT(0, evBits.size() * sizeof(unsigned long)),
                      evBits.data()) < 0 ||
                !testBit(evBits.data(), EV_KEY)) {
                close(fd);
                continue;
            }

            // Require a full letter-key layout so we pick up keyboards but
            // skip mice/touchpads (which also report EV_KEY for buttons).
            std::vector<unsigned long> keyBits(bitsToLongs(KEY_MAX + 1));
            if (ioctl(fd, EVIOCGBIT(EV_KEY, keyBits.size() * sizeof(unsigned long)),
                      keyBits.data()) < 0 ||
                !testBit(keyBits.data(), KEY_A)) {
                close(fd);
                continue;
            }

            keyboardFds_.push_back(fd);
        }
        closedir(dir);

        if (keyboardFds_.empty()) {
            addStatus("No readable keyboard device found under /dev/input. "
                       "Add your user to the 'input' group and re-login.");
        }
    }

    void openUinput() {
        uinputFd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (uinputFd_ < 0) {
            addStatus("Cannot open /dev/uinput (permission denied), clicks can't "
                       "be sent. See README for the required udev rule.");
            return;
        }

        ioctl(uinputFd_, UI_SET_EVBIT, EV_KEY);
        ioctl(uinputFd_, UI_SET_EVBIT, EV_SYN);
        ioctl(uinputFd_, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(uinputFd_, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(uinputFd_, UI_SET_KEYBIT, BTN_MIDDLE);

        struct uinput_setup usetup {};
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1d6b;  // Linux Foundation vendor id (virtual device)
        usetup.id.product = 0x0101;
        std::strncpy(usetup.name, "EzAutoclicker Virtual Mouse", sizeof(usetup.name) - 1);

        ioctl(uinputFd_, UI_DEV_SETUP, &usetup);
        ioctl(uinputFd_, UI_DEV_CREATE);
        // Give the kernel/display server a moment to register the new device
        // before the first click event is written.
        usleep(50000);
    }

    void emit(int type, int code, int value) {
        struct input_event ev {};
        ev.type = static_cast<unsigned short>(type);
        ev.code = static_cast<unsigned short>(code);
        ev.value = value;
        ssize_t written = write(uinputFd_, &ev, sizeof(ev));
        (void)written;
    }

    void watchLoop() {
        std::vector<pollfd> fds;
        fds.reserve(keyboardFds_.size());
        for (int fd : keyboardFds_) fds.push_back({fd, POLLIN, 0});

        while (!stop_.load(std::memory_order_relaxed)) {
            int n = poll(fds.data(), fds.size(), 50);
            if (n <= 0) continue;

            for (const auto& pfd : fds) {
                if (!(pfd.revents & POLLIN)) continue;
                struct input_event ev;
                while (read(pfd.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
                    if (ev.type == EV_KEY) {
                        std::lock_guard<std::mutex> lock(keyMutex_);
                        keyDown_[ev.code] = (ev.value != 0);  // 1=down, 2=repeat, 0=up
                    }
                }
            }
        }
    }

    std::vector<int> keyboardFds_;
    int uinputFd_ = -1;
    std::mutex keyMutex_;
    std::unordered_map<int, bool> keyDown_;
    std::atomic<bool> stop_{false};
    std::thread watchThread_;
    std::string status_;
};

PlatformInput* PlatformInput::create() { return new LinuxInput(); }

const std::vector<KeyOption>& bindableKeyOptions() {
    static const std::vector<KeyOption> keys = [] {
        std::vector<KeyOption> v;
        static const int letterCodes[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
            KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
            KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
        };
        static const char* letterNames[] = {
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
            "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        };
        for (int i = 0; i < 26; ++i) v.push_back({letterNames[i], letterCodes[i]});

        static const int digitCodes[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
                                          KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
        static const char* digitNames[] = {"0", "1", "2", "3", "4",
                                            "5", "6", "7", "8", "9"};
        for (int i = 0; i < 10; ++i) v.push_back({digitNames[i], digitCodes[i]});

        static const int fCodes[] = {KEY_F1, KEY_F2, KEY_F3,  KEY_F4,  KEY_F5,  KEY_F6,
                                      KEY_F7, KEY_F8, KEY_F9,  KEY_F10, KEY_F11, KEY_F12};
        static const char* fNames[] = {"F1", "F2", "F3",  "F4",  "F5",  "F6",
                                        "F7", "F8", "F9",  "F10", "F11", "F12"};
        for (int i = 0; i < 12; ++i) v.push_back({fNames[i], fCodes[i]});

        v.push_back({"Space", KEY_SPACE});
        v.push_back({"Tab", KEY_TAB});
        v.push_back({"Caps Lock", KEY_CAPSLOCK});
        v.push_back({"Left Shift", KEY_LEFTSHIFT});
        v.push_back({"Right Shift", KEY_RIGHTSHIFT});
        v.push_back({"Left Ctrl", KEY_LEFTCTRL});
        v.push_back({"Right Ctrl", KEY_RIGHTCTRL});
        v.push_back({"Left Alt", KEY_LEFTALT});
        v.push_back({"Right Alt", KEY_RIGHTALT});
        v.push_back({"`", KEY_GRAVE});
        return v;
    }();
    return keys;
}

int defaultTriggerKeyCode() { return KEY_F; }

std::string keyDisplayName(int keyCode) {
    for (const auto& k : bindableKeyOptions()) {
        if (k.code == keyCode) return k.name;
    }
    return "Key " + std::to_string(keyCode);
}

} // namespace ezac
