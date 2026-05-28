cmake_minimum_required(VERSION 3.21)
project(dipad LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statically link the MSVC runtime so the shipped dipad.dll has zero external
# dependencies beyond Windows itself (no vcruntime140.dll required).
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Default to Release build if not specified.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "dipad only builds for Windows.")
endif()

add_library(dipad SHARED
    src/dllmain.cpp
    src/com_provider.cpp
    src/real_dinput8.cpp
    src/input8_wrapper.cpp
    src/device8_wrapper.cpp
    src/remap.cpp
    src/config.cpp
    src/logger.cpp
    src/exports.def
)

target_include_directories(dipad PRIVATE src)

# The real dinput8.dll is loaded at runtime via LoadLibrary on the system32
# path (we cannot statically link to it; we ARE the COM provider that COM
# activation routes to). dxguid provides the DInput CLSID/IID constants.
# shell32 is needed for SHGetFolderPathW so we can locate %LOCALAPPDATA%.
target_link_libraries(dipad PRIVATE dxguid shell32)

set_target_properties(dipad PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dipad"
    SUFFIX ".dll"
)

target_compile_definitions(dipad PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601  # Windows 7+
    UNICODE
    _UNICODE
    DIPAD_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(dipad PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
    )
    # Strip default manifest noise from the DLL.
    set_target_properties(dipad PROPERTIES LINK_FLAGS "/MANIFEST:NO")
else()
    target_compile_options(dipad PRIVATE
        -Wall -Wextra -Wno-unknown-pragmas
    )
endif()

# Show which architecture we are building so the user can confirm.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DIPAD_ARCH "x86")
    message(STATUS "dipad: building 32-bit (x86) dipad.dll")
else()
    set(DIPAD_ARCH "amd64")
    message(STATUS "dipad: building 64-bit (x64) dipad.dll")
endif()

# Generate the side-by-side assembly manifest that pairs with dipad.dll. The
# host application's manifest must declare a <dependentAssembly> with the
# same name/version/processorArchitecture for activation to take effect.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifests/dipad.manifest.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dipad.manifest"
    @ONLY
)

add_custom_command(TARGET dipad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/dipad.manifest"
        "$<TARGET_FILE_DIR:dipad>/dipad.manifest"
    COMMENT "Copying dipad.manifest next to dipad.dll"
)

# ---------------------------------------------------------------------------
# Optional diagnostic helper: test_actctx.exe
# ---------------------------------------------------------------------------
# Tiny console EXE that exercises CoCreateInstance(CLSID_DirectInput8) with a
# matching <dependentAssembly name="dipad"> in its own manifest. Used to verify
# the activation-context plumbing independently of any real game.
option(DIPAD_BUILD_TEST_ACTCTX "Build the test_actctx.exe diagnostic helper" ON)
if(DIPAD_BUILD_TEST_ACTCTX)
    add_executable(test_actctx tools/test_actctx/test_actctx.cpp)
    target_link_libraries(test_actctx PRIVATE ole32 dxguid psapi shlwapi)
    set_target_properties(test_actctx PROPERTIES
        LINK_FLAGS "/MANIFEST:NO"
    )

    add_custom_command(TARGET test_actctx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_actctx/test_actctx.exe.manifest"
            "$<TARGET_FILE_DIR:test_actctx>/test_actctx.exe.manifest"
        COMMENT "Copying test_actctx.exe.manifest next to test_actctx.exe"
    )
endif()
