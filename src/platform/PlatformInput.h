#pragma once

#include <string>
#include <vector>

namespace ezac {

enum class MouseButton { Left, Right, Middle };

// Abstract OS backend: detects the global (system-wide) hold state of any
// key and injects mouse clicks at the current cursor position. Both
// operations work regardless of which window has focus, so the autoclicker
// can be used while a game or other app is the active window.
class PlatformInput {
public:
    virtual ~PlatformInput() = default;

    // keyCode is a platform-native code (a Win32 virtual-key code on
    // Windows, a Linux KEY_* evdev code on Linux); see bindableKeyOptions().
    virtual bool isKeyHeld(int keyCode) = 0;
    virtual void sendClick(MouseButton button) = 0;

    // Empty string if the backend is fully functional, otherwise a
    // human-readable explanation (e.g. a missing permission) to show in the UI.
    virtual const char* statusMessage() const = 0;

    // Platform-specific factory, implemented once per platform source file.
    static PlatformInput* create();
};

struct KeyOption {
    const char* name;
    int code;
};

// The keys offered in the "change hotkey" UI, and their display names.
// Implemented once per platform source file.
const std::vector<KeyOption>& bindableKeyOptions();

// The factory-default trigger key code for this platform (F).
int defaultTriggerKeyCode();

// Display name for a platform-native key code, e.g. for showing the
// currently bound hotkey. Falls back to "Key <code>" if unrecognized.
std::string keyDisplayName(int keyCode);

} // namespace ezac
