cmake_minimum_required(VERSION 3.24)

project(CSVExplorer VERSION 0.2.0 LANGUAGES CXX)

set(CSV_EXPLORER_VERSION "0.2")
set(CSV_EXPLORER_LICENSE "MIT")
set(CSV_EXPLORER_AUTHOR "Yvan Janssens")
set(CSV_EXPLORER_REPOSITORY_URL "https://github.com/cyberdotgent/CSV-Explorer")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(csv_explorer
    src/main.cpp
)

set_target_properties(csv_explorer PROPERTIES
    OUTPUT_NAME "csv-explorer"
)

target_include_directories(csv_explorer
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(CSV_EXPLORER_MINGW_CROSS FALSE)
if(CMAKE_CROSSCOMPILING
    AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CSV_EXPLORER_MINGW_CROSS TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR APPLE)
    include(FetchContent)

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
        set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
    endif()

    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(wxBUILD_MONOLITHIC ON CACHE BOOL "" FORCE)
    set(wxBUILD_PRECOMP OFF CACHE BOOL "" FORCE)
    set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(wxUSE_STC OFF CACHE BOOL "" FORCE)

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

    if(TARGET wx::wx)
        target_link_libraries(csv_explorer PRIVATE wx::wx)
    elseif(TARGET wx::mono)
        target_link_libraries(csv_explorer PRIVATE wx::mono)
    elseif(TARGET wxmono)
        target_link_libraries(csv_explorer PRIVATE wxmono)
    elseif(TARGET wx::wxcore AND TARGET wx::wxbase)
        target_link_libraries(csv_explorer PRIVATE wx::wxcore wx::wxbase)
    elseif(TARGET wx::core AND TARGET wx::base)
        set(_wx_widgets_targets wx::base wx::core)
        if(TARGET wx::adv)
            list(APPEND _wx_widgets_targets wx::adv)
        endif()
        if(TARGET wxWidgets::wxWidgets)
            target_link_libraries(csv_explorer PRIVATE wxWidgets::wxWidgets)
        else()
            target_link_libraries(csv_explorer PRIVATE ${_wx_widgets_targets})
        endif()
    elseif(TARGET wxWidgets::wxWidgets)
        target_link_libraries(csv_explorer PRIVATE wxWidgets::wxWidgets)
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
    find_package(wxWidgets REQUIRED COMPONENTS core base adv)
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
    )
    set_source_files_properties(${CSV_EXPLORER_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${CSV_EXPLORER_WINDOW_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(csv_explorer PRIVATE
        ${CSV_EXPLORER_BUNDLE_ICON}
        ${CSV_EXPLORER_WINDOW_ICON}
    )
endif()

if(WIN32)
    set_target_properties(csv_explorer PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(csv_explorer PRIVATE assets/csv-explorer.rc)
endif()
