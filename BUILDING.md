# Building CSV Explorer

`ccache` is a mandatory build dependency. All builds are configured to use the
shared HTTP cache at `http://buildcache.cyber.gent/|layout=bazel`.

## Standard build

```bash
cmake -S . -B build
cmake --build build
```

## Windows cross-build (x86_64 MinGW)

```bash
cmake --preset windows-mingw64
cmake --build --preset windows-mingw64
```

Or run:

```bash
./scripts/build-mingw64-windows.sh
```

## Build notes

- Windows and macOS fetch and build wxWidgets from source with static linking.
- Linux prefers an installed wxWidgets via `find_package`.
- The MinGW cross-build uses `cmake/toolchains/mingw-x86_64.cmake`.
- The MinGW cross-build disables `wxWebView` for the static Windows toolchain path.
