# Repository instructions

Project root:
- `CMakeLists.txt` builds the application and controls dependencies.
- `src/main.cpp` contains the wxWidgets implementation.
- `config.h.in` defines version metadata for generate-time injection.

Build notes:
- Configure with CMake and build as usual:
  - `cmake -S . -B build`
  - `cmake --build build`
- `ccache` is a required dependency for all builds and uses the shared HTTP
  cache at `http://buildcache.cyber.gent/|layout=bazel`.
- On macOS and Windows, wxWidgets is fetched and built from source and linked statically.
- On Linux, build prefers installed wxWidgets via `find_package`.

Operational behavior:
- File menu opens CSV files via built-in file dialog using CSV-first filter:
  - `CSV files (*.csv)|*.csv|All files (*.*)|*.*`
- Edit menu supports copy and find with Find/Find Next/Find Previous.
- About shows app name and version (`0.1` from `CSV_EXPLORER_VERSION` in CMake).
- On Windows the executable target is `WIN32` (no console window).
- On macOS the bundle identifier is set to `gent.cyber.csvexplorer`.
