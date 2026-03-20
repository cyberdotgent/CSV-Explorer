# wxCsv

wxCsv is a minimal cross-platform CSV file explorer built with wxWidgets.

## Features

- Opens CSV data in a `wxDataViewCtrl`.
- File menu: **Open** and **Exit**.
- Edit menu: **Copy**, **Find**, **Find Next**, and **Find Previous**.
- Standard shortcut keys:  
  - Open: `Ctrl+O`  
  - Exit: `Ctrl+Q`  
  - Copy: `Ctrl+C`  
  - Find: `Ctrl+F`  
  - Find Next: `Ctrl+G`  
  - Find Previous: `Shift+Ctrl+G`  
- Built-in wxWidgets find dialog for searching.
- About dialog showing app name and version.
- App title is `wxCsv` when no file is loaded and `wxCsv - <filename>` when a file is loaded.

## Build

```bash
cmake -S . -B build
cmake --build build
```

### Cross compile for Windows (x86_64 MinGW)

```bash
cmake --preset windows-mingw64
cmake --build --preset windows-mingw64
```

Or run:

```bash
./scripts/build-mingw64-windows.sh
```

This cross build:

- Uses the `cmake/toolchains/mingw-x86_64.cmake` toolchain file.
- Uses a static wxWidgets build and disables `wxWebView` for this static Windows/MinGW path.

By default, the build uses:

- wxWidgets from source on **Windows** and **macOS** (static linking).
- System wxWidgets on **Linux** when available.

## Version

The version string is maintained in `CMakeLists.txt` as:

```cmake
set(WXCsv_VERSION "0.1")
```

`config.h` is generated at configure time from this value.
