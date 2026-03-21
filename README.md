# CSV Explorer

CSV Explorer is a minimal cross-platform CSV file explorer built with wxWidgets.

<img src="assets/csv-explorer.png" alt="CSV Explorer icon" width="128" />

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
- App title is `CSV Explorer` when no file is loaded and `CSV Explorer - <filename>` when a file is loaded.

## Application icon

`assets/csv-explorer.png` is the source icon artwork.

- macOS: packaged as `csv-explorer.icns` and used as the app bundle icon.
- Windows: linked as the executable icon in `assets/csv-explorer.ico`.
- Runtime: the icon is embedded into the executable (`src/csv_explorer_png_data.h`) and used for window title bars so no external icon file needs to be loaded at runtime.

## Build

`ccache` is a mandatory build dependency. All builds are configured to use the
shared HTTP cache at `http://buildcache.cyber.gent/|layout=bazel`.

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
- Mandatory `ccache` compiler launchers with a shared HTTP cache backend.

## Version

The version string is maintained in `CMakeLists.txt` as:

```cmake
set(CSV_EXPLORER_VERSION "0.1")
```

`config.h` is generated at configure time from this value.

## License

This project is licensed under the MIT License. See `LICENSE`.
