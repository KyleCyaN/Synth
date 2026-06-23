cmake_minimum_required(VERSION 3.21)
project(Synth LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    add_compile_options(
            /Zc:__cplusplus
            /permissive-
            /MD
            "$<$<CONFIG:Debug>:/Zi>"
            "$<$<CONFIG:Debug>:/Od>"
            "$<$<CONFIG:RelWithDebInfo>:/Zi>"
    )

    add_link_options(
            "$<$<CONFIG:Debug>:/DEBUG:FULL>"
            "$<$<CONFIG:RelWithDebInfo>:/DEBUG>"
    )
endif()

file(GLOB DLL_SOURCES
        "DLLCore/src/*.cpp"
        "DLLCore/src/features/*.cpp"
)

file(GLOB IMGUI_SOURCES
        "DLLCore/include/imgui/*.cpp"
        "DLLCore/include/imgui/backends/*.cpp"
)

list(FILTER IMGUI_SOURCES EXCLUDE REGEX ".*freetype.*")

add_library(SynthDLL SHARED
        ${DLL_SOURCES}
        ${IMGUI_SOURCES}
)

set_target_properties(SynthDLL PROPERTIES
        AUTOUIC OFF
        AUTOMOC OFF
        OUTPUT_NAME "Synth"
        PREFIX ""
        SUFFIX ".dll"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(SynthDLL PRIVATE
        DLLCore/include
        DLLCore/include/imgui
        DLLCore/include/MinHook
        DLLCore/src
        Shared
)

target_link_libraries(SynthDLL
        user32
        gdi32
        d3d11
        dxgi
        d3dcompiler
        dwmapi
        shell32
        advapi32
        Shcore
        version
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DLLCore/libs/MinHook.lib")
    target_link_libraries(SynthDLL
            "${CMAKE_CURRENT_SOURCE_DIR}/DLLCore/libs/MinHook.lib"
    )
endif()

target_compile_definitions(SynthDLL PRIVATE
        SYNTHDLL_EXPORTS
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        IMGUI_DISABLE_FREETYPE
        IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
        IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
)

set(INJECTOR_SOURCES
        Injector/src/main.cpp
        Injector/src/InjectorGUI.cpp
        Injector/src/core/Inject.cpp
        Injector/resources/resources.qrc
)

set(INJECTOR_HEADERS
        Injector/src/InjectorGUI.h
        Injector/src/core/Inject.h
)

add_executable(Injector
        ${INJECTOR_SOURCES}
        ${INJECTOR_HEADERS}
        icon.rc
)

set_target_properties(Injector PROPERTIES
        OUTPUT_NAME "Synth"
        WIN32_EXECUTABLE ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

target_link_libraries(Injector
        Qt6::Core
        Qt6::Widgets
        advapi32
        ole32
        user32
        kernel32
        shell32
        psapi
)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt)

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET Injector POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --no-compiler-runtime
            --no-system-d3d-compiler
            --no-opengl-sw
            --no-translations
            --core
            --widgets
            --no-quick
            --no-qml
            --no-network
            --no-sql
            --no-svg
            --no-printsupport
            --no-plugins
            --verbose 0
            "$<TARGET_FILE:Injector>"

            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:Injector>/platforms"

            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Qt6_DIR}/../../../plugins/platforms/qwindows.dll"
            "$<TARGET_FILE_DIR:Injector>/platforms/qwindows.dll"

            COMMENT "Deploying minimal Qt runtime..."
    )
else()
    message(WARNING "windeployqt not found. Qt libraries will NOT be deployed.")
endif()