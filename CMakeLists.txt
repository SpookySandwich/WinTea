cmake_minimum_required(VERSION 3.21)

project(WinTea
    VERSION 0.9.0
    DESCRIPTION "Win+T opens a terminal. Win+Alt+T opens it as admin."
    LANGUAGES CXX RC)

# Static CRT so the exe is a single portable file with no VC++ redist.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

function(wintea_common_flags target)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_features(${target} PRIVATE cxx_std_20)
    # Guard to C++ sources: the resource compiler (rc.exe) rejects cl.exe flags.
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Os /GL /Gw /Gy>)
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO>)
endfunction()

# version.h (repo URL + version macros) generated from the project version.
configure_file(${CMAKE_SOURCE_DIR}/src/version.h.in
               ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

# Compile resources by calling rc.exe directly. CMake's Ninja generator routes
# the RC language through a "cmcldeps" shim that fails with RC1107 on the VS18
# toolset; a custom command sidesteps it. The .rc embeds icons, version info,
# and the app manifest, producing one .res we link as a source.
set(WINTEA_RES ${CMAKE_BINARY_DIR}/wintea.res)
add_custom_command(
    OUTPUT ${WINTEA_RES}
    COMMAND ${CMAKE_RC_COMPILER} /nologo
            /I "${CMAKE_BINARY_DIR}/generated" /I "${CMAKE_SOURCE_DIR}/src"
            /fo "${WINTEA_RES}" "${CMAKE_SOURCE_DIR}/src/res/wintea.rc"
    DEPENDS ${CMAKE_SOURCE_DIR}/src/res/wintea.rc
            ${CMAKE_SOURCE_DIR}/src/res/resource.h
            ${CMAKE_BINARY_DIR}/generated/version.h
            ${CMAKE_SOURCE_DIR}/assets/icon/wintea.ico
            ${CMAKE_SOURCE_DIR}/assets/icon/tray-for-light-taskbar.ico
            ${CMAKE_SOURCE_DIR}/assets/icon/tray-for-dark-taskbar.ico
    COMMENT "Compiling resources (wintea.rc)"
    VERBATIM)

# Mark the generated .res as a pre-compiled object so CMake passes it to the
# linker. Without EXTERNAL_OBJECT, a custom-command output is built but never
# linked, and the exe ends up with no icons/version resources.
set_source_files_properties(${WINTEA_RES} PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)

# --- WinTea (the app) ------------------------------------------------------
# app.manifest is a source so CMake's linker manifest step embeds it (mt.exe).
add_executable(wintea WIN32
    src/main.cpp
    src/hook.cpp
    src/chord.cpp
    src/config.cpp
    src/launch.cpp
    src/tray.cpp
    src/autostart.cpp
    src/res/app.manifest
    ${WINTEA_RES})
wintea_common_flags(wintea)
target_include_directories(wintea PRIVATE ${CMAKE_BINARY_DIR}/generated src)
target_link_libraries(wintea PRIVATE user32 shell32 advapi32 wtsapi32)
set_target_properties(wintea PROPERTIES OUTPUT_NAME "WinTea")

# --- Tests -----------------------------------------------------------------
enable_testing()
add_executable(chord_test tests/chord_test.cpp src/chord.cpp)
wintea_common_flags(chord_test)
add_test(NAME chord_test COMMAND chord_test)

# --- M1 mechanism spike (throwaway; kept for re-verification) ---------------
add_executable(spike spike/main.cpp)
wintea_common_flags(spike)
target_link_libraries(spike PRIVATE user32 shell32 dwmapi)
