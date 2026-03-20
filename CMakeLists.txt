cmake_minimum_required(VERSION 3.24)

project(wxCsv VERSION 0.1.0 LANGUAGES CXX)

set(WXCsv_VERSION "0.1")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(wxBUILD_MONOLITHIC ON CACHE BOOL "" FORCE)
    set(wxBUILD_PRECOMP OFF CACHE BOOL "" FORCE)
    set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)

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
        URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxWidgets-3.2.6.tar.bz2
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
    set_target_properties(wxCsv PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "gent.cyber.wxCsv"
        MACOSX_BUNDLE_BUNDLE_NAME "wxCsv"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${WXCsv_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${WXCsv_VERSION}"
    )
endif()

if(WIN32)
    set_target_properties(wxCsv PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
