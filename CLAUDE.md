# QtRocket

C++23 project built with CMake presets + Ninja, tested with GoogleTest. Cross-platform: Linux (GCC, Clang),
macOS (Apple Clang) and Windows (MSVC).

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'Greet\.'` or `build/clang-debug/bin/QtRocket_tests --gtest_filter='Greet.*'`
- Before finishing a change, also run: `cmake --workflow --preset tidy` (clang-tidy, warnings are errors) and
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan)
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. The pre-commit hook and CI also reject unformatted files

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`, and
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`).
Each builds into `build/<preset>/`; never edit anything under `build/`. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets`
shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It tags
`v<project VERSION>` and publishes the `dist-*` archives, so the version is raised in `project()` in
`CMakeLists.txt`. An archive holds what the `install()` rules install.

## Layout
- `include/QtRocket/`: public headers
- `src/`: `QtRocket_lib` (the code) and the `QtRocket` executable (`main.cpp`)
- `tests/`: GoogleTest files, all in `QtRocket_tests` (class tests: `MyClassTests.cpp`; other tests: `*_tests.cpp`)
- `cmake/ProjectOptions.cmake`: `QtRocket_configure_target()` (warnings, sanitizers, coverage, tidy)
- `cmake/Dependencies.cmake`: third-party libraries via FetchContent

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- A class's header and implementation files are named exactly after the class, including capitalization:
  `class MyClass` lives in `include/QtRocket/MyClass.h` and `src/MyClass.cpp`, and its tests in `tests/MyClassTests.cpp`
- Code lives in `namespace QtRocket`; project includes use quotes: `#include "QtRocket/greet.h"`
- Every new target must call `QtRocket_configure_target(<target>)`
- New source files go into the relevant `CMakeLists.txt`; new tests go into `tests/CMakeLists.txt`
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC
- Code must build and pass its tests on Linux, macOS and Windows (CI runs all three). Use the standard library
  (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it in one
  source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a branch for each platform
