# EzAutoclicker

A lightweight, native autoclicker for Linux and Windows. Hold a hotkey
(**F** by default, rebindable in the UI) anywhere on your system and it
fires left mouse clicks at your current cursor position, from 100 up to
900 clicks per second, adjustable live. Release the key and it stops
instantly.

Built in C++17 with a small [Dear ImGui](https://github.com/ocornut/imgui)
UI. No installer, no bloat, just a single executable.

## Features

- **Global hotkey**: works even when the app's own window isn't focused
  (e.g. while a game is the active window).
- **100-900 CPS**, adjustable live with a slider or exact numeric input.
- **Rebindable hotkey**: click "Change" in the UI and press any key.
- **Low overhead**: a hybrid sleep/spin timer holds precise, high CPS
  without pegging a CPU core or dropping frame rate.

## Requirements

### Linux

- CMake 3.16+ and a C++17 compiler (GCC or Clang)
- OpenGL, X11/Wayland, and windowing dev packages:

  ```sh
  # Arch / CachyOS
  sudo pacman -S base-devel cmake mesa libx11 libxrandr libxinerama libxcursor libxi

  # Debian / Ubuntu
  sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
  ```

- Internet connection for the first build (CMake fetches GLFW automatically)

### Windows

- CMake 3.16+
- Visual Studio 2019+ (or another CMake-compatible C++17 toolchain)
- Internet connection for the first build

## Build & run

### Linux

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/EzAutoclicker
```

Injecting clicks and reading the keyboard globally uses `/dev/uinput` and
`/dev/input/event*`, which need a one-time permission setup:

```sh
sudo cp udev/99-ezautoclicker.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG input "$USER"
sudo modprobe uinput
echo uinput | sudo tee /etc/modules-load.d/uinput.conf   # load uinput on boot
```

Log out and back in (group membership only takes effect on a new session),
then run the app. If a permission is still missing, the UI shows exactly
which one, in red.

### Windows

```powershell
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\EzAutoclicker.exe
```

If the window/app you're clicking into is running elevated (as
Administrator), run EzAutoclicker elevated too, since Windows blocks
synthetic input from a non-elevated process into an elevated one.

## Notes

- CPS above ~900 stops being reliably distinguishable from OS scheduling
  jitter on most systems, which is why the slider caps there.
- The click button itself is fixed to the left mouse button; the hotkey
  that triggers it is what's rebindable.
