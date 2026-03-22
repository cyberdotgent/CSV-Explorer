cmake_minimum_required(VERSION 3.24)

project(CSVExplorer VERSION 0.4.0 LANGUAGES C CXX)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal macOS binaries" FORCE)
endif()

set(CSV_EXPLORER_VERSION "${PROJECT_VERSION}")
set(CSV_EXPLORER_LICENSE "MIT")
set(CSV_EXPLORER_AUTHOR "Yvan Janssens")
set(CSV_EXPLORER_COMPANY "Cyber.Gent")
set(CSV_EXPLORER_REPOSITORY_URL "https://github.com/cyberdotgent/CSV-Explorer")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(Git QUIET)
set(CSV_EXPLORER_GIT_COMMIT_SHORT "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE CSV_EXPLORER_GIT_COMMIT_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(CSV_EXPLORER_GIT_COMMIT_SHORT STREQUAL "")
    set(CSV_EXPLORER_GIT_COMMIT_SHORT "unknown")
endif()

find_program(CCACHE_PROGRAM ccache)
if(NOT CCACHE_PROGRAM)
    message(FATAL_ERROR "ccache is required for all builds. Install ccache and retry.")
endif()

set(CCACHE_REMOTE_STORAGE_URL "http://buildcache.cyber.gent/|layout=bazel")
set(_ccache_launcher
    "${CMAKE_COMMAND}" "-E" "env"
    "CCACHE_REMOTE_STORAGE=${CCACHE_REMOTE_STORAGE_URL}"
    "CCACHE_RESHARE=true"
    "CCACHE_COMPILERCHECK=content"
    "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
    "CCACHE_NOHASHDIR=true"
    "${CCACHE_PROGRAM}"
)
foreach(_launcher_var
    CMAKE_C_COMPILER_LAUNCHER
    CMAKE_CXX_COMPILER_LAUNCHER
    CMAKE_OBJCXX_COMPILER_LAUNCHER)
    set(${_launcher_var} "${_ccache_launcher}" CACHE STRING "Mandatory ccache launcher" FORCE)
endforeach()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

set(CSV_EXPLORER_VERSION_COMMA "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(CSV_EXPLORER_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/csv-explorer.ico")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/csv-explorer.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/csv-explorer.rc
    @ONLY
)

add_executable(csv_explorer
    src/about_dialog.cpp
    src/go_to_row_dialog.cpp
    src/main.cpp
    src/main_frame.cpp
    src/print_support.cpp
    src/unsaved_changes_dialog.cpp
)

set_target_properties(csv_explorer PROPERTIES
    OUTPUT_NAME "csv-explorer"
)

if((APPLE OR WIN32) AND CMAKE_STRIP)
    add_custom_command(
        TARGET csv_explorer POST_BUILD
        COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:csv_explorer>"
        COMMENT "Stripping csv-explorer binary"
    )
endif()

target_include_directories(csv_explorer
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(SQLITE_AMALGAMATION_VERSION 3510300)
set(SQLITE_VERSION 3.51.3)
set(SQLITE_DOWNLOAD_YEAR 2026)
FetchContent_Declare(
    sqlite
    URL https://sqlite.org/${SQLITE_DOWNLOAD_YEAR}/sqlite-amalgamation-${SQLITE_AMALGAMATION_VERSION}.zip
)
FetchContent_GetProperties(sqlite)
if(NOT sqlite_POPULATED)
    cmake_policy(PUSH)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(sqlite)
    cmake_policy(POP)
endif()

add_library(sqlite3 STATIC
    ${sqlite_SOURCE_DIR}/sqlite3.c
)
add_library(SQLite::SQLite3 ALIAS sqlite3)
set_target_properties(sqlite3 PROPERTIES
    OUTPUT_NAME sqlite3
)
target_include_directories(sqlite3
    PUBLIC
    ${sqlite_SOURCE_DIR}
)
target_compile_definitions(sqlite3
    PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION
)
target_link_libraries(csv_explorer PRIVATE SQLite::SQLite3)

set(CSV_EXPLORER_MINGW_CROSS FALSE)
if(CMAKE_CROSSCOMPILING
    AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CSV_EXPLORER_MINGW_CROSS TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR APPLE)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
        set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
    endif()

    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(wxBUILD_INSTALL OFF CACHE BOOL "" FORCE)
    set(wxBUILD_MONOLITHIC OFF CACHE BOOL "" FORCE)
    set(wxBUILD_PRECOMP OFF CACHE BOOL "" FORCE)
    set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(wxUSE_AUI ON CACHE BOOL "" FORCE)
    set(wxUSE_DEBUGREPORT OFF CACHE BOOL "" FORCE)
    set(wxUSE_FS_INET OFF CACHE BOOL "" FORCE)
    set(wxUSE_GRID ON CACHE BOOL "" FORCE)
    set(wxUSE_HELP OFF CACHE BOOL "" FORCE)
    set(wxUSE_OPENGL OFF CACHE BOOL "" FORCE)
    set(wxUSE_PRINTING_ARCHITECTURE ON CACHE BOOL "" FORCE)
    set(wxUSE_PROTOCOL OFF CACHE BOOL "" FORCE)
    set(wxUSE_PROTOCOL_FILE OFF CACHE BOOL "" FORCE)
    set(wxUSE_PROTOCOL_FTP OFF CACHE BOOL "" FORCE)
    set(wxUSE_PROTOCOL_HTTP OFF CACHE BOOL "" FORCE)
    set(wxUSE_HTML OFF CACHE BOOL "" FORCE)
    set(wxUSE_MEDIACTRL OFF CACHE BOOL "" FORCE)
    set(wxUSE_MS_HTML_HELP OFF CACHE BOOL "" FORCE)
    set(wxUSE_PROPGRID OFF CACHE BOOL "" FORCE)
    set(wxUSE_RIBBON OFF CACHE BOOL "" FORCE)
    set(wxUSE_RICHTEXT OFF CACHE BOOL "" FORCE)
    set(wxUSE_SOCKETS OFF CACHE BOOL "" FORCE)
    set(wxUSE_STC OFF CACHE BOOL "" FORCE)
    set(wxUSE_URL OFF CACHE BOOL "" FORCE)
    set(wxUSE_WEBVIEW OFF CACHE BOOL "" FORCE)
    set(wxUSE_WEBREQUEST OFF CACHE BOOL "" FORCE)
    set(wxUSE_WXHTML_HELP OFF CACHE BOOL "" FORCE)
    set(wxUSE_XML OFF CACHE BOOL "" FORCE)
    set(wxUSE_XRC OFF CACHE BOOL "" FORCE)

if(CSV_EXPLORER_MINGW_CROSS)
    set(wxUSE_WEBVIEW OFF CACHE BOOL "" FORCE)
    target_link_options(csv_explorer PRIVATE -static-libgcc -static-libstdc++)
endif()

    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(wxBUILD_TOOLKIT msw CACHE STRING "" FORCE)
        set(wxBUILD_CONFIG msw)
    elseif(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(wxBUILD_TOOLKIT osx_cocoa CACHE STRING "" FORCE)
        set(wxBUILD_CONFIG osx_cocoa)
    endif()

    FetchContent_Declare(
        wxwidgets
        URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.9/wxWidgets-3.2.9.tar.bz2
    )
    FetchContent_MakeAvailable(wxwidgets)

    if(TARGET wx::wxcore AND TARGET wx::wxbase)
        set(_wx_widgets_targets wx::wxbase wx::wxcore)
        if(TARGET wx::wxadv)
            list(APPEND _wx_widgets_targets wx::wxadv)
        endif()
        if(TARGET wx::wxaui)
            list(APPEND _wx_widgets_targets wx::wxaui)
        endif()
        target_link_libraries(csv_explorer PRIVATE ${_wx_widgets_targets})
    elseif(TARGET wx::core AND TARGET wx::base)
        set(_wx_widgets_targets wx::base wx::core)
        if(TARGET wx::adv)
            list(APPEND _wx_widgets_targets wx::adv)
        endif()
        if(TARGET wx::aui)
            list(APPEND _wx_widgets_targets wx::aui)
        endif()
        target_link_libraries(csv_explorer PRIVATE ${_wx_widgets_targets})
    else()
        # As a final fallback, use the FindwxWidgets-compatible variables generated
        # by wxWidgetsConfig.cmake if available.
        if(wxWidgets_LIBRARIES)
            target_link_libraries(csv_explorer PRIVATE ${wxWidgets_LIBRARIES})
        else()
        message(FATAL_ERROR "Unable to find wxWidgets CMake targets after FetchContent.")
        endif()
    endif()
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base adv aui)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(csv_explorer PRIVATE ${wxWidgets_LIBRARIES})
endif()

if(APPLE)
    set(CSV_EXPLORER_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/csv-explorer.icns")
    set(CSV_EXPLORER_WINDOW_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/csv-explorer.png")
    set_target_properties(csv_explorer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "gent.cyber.csvexplorer"
        MACOSX_BUNDLE_BUNDLE_NAME "CSV Explorer"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CSV_EXPLORER_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CSV_EXPLORER_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "csv-explorer.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    set_source_files_properties(${CSV_EXPLORER_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${CSV_EXPLORER_WINDOW_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(csv_explorer PRIVATE
        ${CSV_EXPLORER_BUNDLE_ICON}
        ${CSV_EXPLORER_WINDOW_ICON}
    )

    install(TARGETS csv_explorer BUNDLE DESTINATION .)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION .)

    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_NAME "csv-explorer")
    set(CPACK_PACKAGE_VENDOR "${CSV_EXPLORER_COMPANY}")
    set(CPACK_PACKAGE_VERSION "${CSV_EXPLORER_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "csv-explorer-${CSV_EXPLORER_VERSION}-macos-universal")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CSV Explorer")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    include(CPack)
endif()

if(WIN32)
    set_target_properties(csv_explorer PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(csv_explorer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/csv-explorer.rc)
endif()
