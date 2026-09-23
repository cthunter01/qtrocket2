# QtRocket

TODO: what QtRocket does.

## Requirements
- CMake 3.28+ and Ninja
- A C++23 compiler with `<print>`:
  - Linux: GCC 14+ or Clang 18+
  - macOS: Xcode 16.3+ or its Command Line Tools (Apple Clang 17+)
  - Windows: Visual Studio 2022 17.7+ (MSVC) with the "Desktop development with C++" workload
- Optional: clang-tidy, clang-format, llvm-cov/llvm-profdata (coverage), Doxygen (docs), ccache.
  On macOS, clang-tidy comes from Homebrew (`brew install llvm`). Coverage uses Xcode's llvm-cov.

GoogleTest is used from the system when installed, otherwise downloaded at configure time.

## Build
Linux and macOS:
```sh
cmake --workflow --preset dev          # configure + build + test, Clang Debug
./build/clang-debug/bin/QtRocket
```

Windows, from a **Developer PowerShell for VS** (Ninja needs MSVC's environment; VS Code's CMake Tools and
Visual Studio set it up themselves):
```powershell
cmake --workflow --preset dev-msvc     # configure + build + test, MSVC Debug
.\build\msvc-debug\bin\QtRocket.exe
```

| Preset | Platforms | What it is |
| --- | --- | --- |
| `clang-debug`, `clang-release` | Linux, macOS | Everyday builds (Apple Clang on macOS) |
| `gcc-debug`, `gcc-release` | Linux | Everyday builds |
| `msvc-debug`, `msvc-release` | Windows | Everyday builds |
| `asan` | Linux, macOS | Clang Debug with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Linux, macOS | Clang RelWithDebInfo with ThreadSanitizer |
| `tidy` | Linux, macOS | Clang Debug running clang-tidy on every file; findings are errors |
| `coverage` | Linux, macOS | `cmake --workflow --preset coverage` writes `build/coverage/coverage/html/index.html` |
| `ci-gcc`, `ci-clang`, `ci-msvc` | as their compiler | Release builds with warnings as errors, as run in CI |
| `dist-linux`, `dist-macos`, `dist-windows` | Linux, macOS, Windows | The release archives (see [Releases](#releases)) |

A preset exists only on the platforms it supports; `cmake --list-presets` shows the ones for this machine.
Each workflow preset (`dev`, `dev-msvc`, `ci-gcc`, `ci-clang`, `ci-msvc`, `asan`, `tsan`, `tidy`, `coverage`)
configures, builds and tests in one command, and the `dist-*` ones also package. Separate steps:
`cmake --preset <p>`, `cmake --build --preset <p>`, `ctest --preset <p>`.

CI (GitHub Actions) builds and tests on all three: Linux (`ci-gcc`, `ci-clang`, `asan`, `tidy`), macOS
(`ci-clang`) and Windows (`ci-msvc`).

API docs: `cmake --build --preset clang-debug --target docs`, then open `build/clang-debug/docs/html/index.html`.

## Releases
The Release workflow (`.github/workflows/release.yml`) runs only when started by hand, never on a push:
1. Raise `VERSION` in `project()` in `CMakeLists.txt`, then commit and push.
2. Start it from the Actions tab (Release > Run workflow, pick the branch) or with `gh workflow run release.yml`
   (`-f prerelease=true` marks it a pre-release).

It stops at once if the tag `v<version>` already exists. Otherwise it runs all of CI and builds, tests and
packages an archive on each platform. Only when every job passes does it tag the commit `v<version>` and
publish a GitHub release with the archives, a `SHA256SUMS` file and generated release notes.

| Archive | Built with | Runs on |
| --- | --- | --- |
| `QtRocket-<version>-linux-x86_64.tar.gz` | GCC 14, Ubuntu 24.04 | x86-64 Linux with glibc 2.39+ (Ubuntu 24.04+, Debian 13+, Fedora 40+, RHEL 10+) |
| `QtRocket-<version>-macos-universal.tar.gz` | Apple Clang | macOS 14+, Apple silicon and Intel |
| `QtRocket-<version>-windows-x86_64.zip` | MSVC | 64-bit Windows |

The C++ runtime is linked into the Linux and Windows executables, so users need no libstdc++ or VC++
Redistributable. Dependencies are always built from source, never taken from the build machine. An archive holds
what the `install()` rules install: add rules for anything else it should ship.
Build one locally with `cmake --workflow --preset dist-linux` (or `dist-macos`, `dist-windows`); it lands in
`build/dist-<os>/package/`.

The executables are not code-signed. A macOS browser download needs `xattr -d com.apple.quarantine bin/QtRocket`
before it runs, and Windows SmartScreen warns the first time.
