cmake_minimum_required(VERSION 3.24)

project(wxCsv VERSION 0.1.0 LANGUAGES CXX)

set(WXCsv_VERSION "0.1")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(wxCsv
    src/main.cpp
)

target_include_directories(wxCsv
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(WXCsv_MINGW_CROSS FALSE)
if(CMAKE_CROSSCOMPILING
    AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WXCsv_MINGW_CROSS TRUE)
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

if(WXCsv_MINGW_CROSS)
    set(wxUSE_WEBVIEW OFF CACHE BOOL "" FORCE)
    target_link_options(wxCsv PRIVATE -static-libgcc -static-libstdc++)
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
        target_link_libraries(wxCsv PRIVATE wx::wx)
    elseif(TARGET wx::mono)
        target_link_libraries(wxCsv PRIVATE wx::mono)
    elseif(TARGET wxmono)
        target_link_libraries(wxCsv PRIVATE wxmono)
    elseif(TARGET wx::wxcore AND TARGET wx::wxbase)
        target_link_libraries(wxCsv PRIVATE wx::wxcore wx::wxbase)
    elseif(TARGET wx::core AND TARGET wx::base)
        set(_wx_widgets_targets wx::base wx::core)
        if(TARGET wx::adv)
            list(APPEND _wx_widgets_targets wx::adv)
        endif()
        if(TARGET wxWidgets::wxWidgets)
            target_link_libraries(wxCsv PRIVATE wxWidgets::wxWidgets)
        else()
            target_link_libraries(wxCsv PRIVATE ${_wx_widgets_targets})
        endif()
    elseif(TARGET wxWidgets::wxWidgets)
        target_link_libraries(wxCsv PRIVATE wxWidgets::wxWidgets)
    else()
        # As a final fallback, use the FindwxWidgets-compatible variables generated
        # by wxWidgetsConfig.cmake if available.
        if(wxWidgets_LIBRARIES)
            target_link_libraries(wxCsv PRIVATE ${wxWidgets_LIBRARIES})
        else()
        message(FATAL_ERROR "Unable to find wxWidgets CMake targets after FetchContent.")
        endif()
    endif()
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base adv)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(wxCsv PRIVATE ${wxWidgets_LIBRARIES})
endif()

if(APPLE)
    set(WXCsv_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/wxcsv.icns")
    set(WXCsv_WINDOW_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/wxcsv.png")
    set_target_properties(wxCsv PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "gent.cyber.wxCsv"
        MACOSX_BUNDLE_BUNDLE_NAME "wxCsv"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${WXCsv_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${WXCsv_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "wxcsv.icns"
    )
    set_source_files_properties(${WXCsv_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${WXCsv_WINDOW_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(wxCsv PRIVATE
        ${WXCsv_BUNDLE_ICON}
        ${WXCsv_WINDOW_ICON}
    )
endif()

if(WIN32)
    set_target_properties(wxCsv PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(wxCsv PRIVATE assets/wxcsv.rc)
endif()
