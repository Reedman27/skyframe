cmake_minimum_required(VERSION 3.20)
project(Skyframe VERSION 0.2.0 LANGUAGES CXX)

option(SKYFRAME_BUILD_GAME "Build the raylib desktop application when raylib is installed" ON)
option(SKYFRAME_BUILD_TESTS "Build the simulation tests" ON)
option(SKYFRAME_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SKYFRAME_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(skyframe_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(SKYFRAME_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(SKYFRAME_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(SKYFRAME_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            get_target_property(target_type ${target} TYPE)
            if(NOT target_type STREQUAL "STATIC_LIBRARY")
                target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            endif()
        endif()
    endif()
endfunction()

add_library(skyframe_core
    src/skyframe/sim/aircraft.cpp
    src/skyframe/sim/environment.cpp
    src/skyframe/sim/fleet.cpp
    src/skyframe/sim/ordnance.cpp
    src/skyframe/sim/replay.cpp
    src/skyframe/sim/replay_archive.cpp
    src/skyframe/sim/simulation.cpp
)
add_library(Skyframe::Core ALIAS skyframe_core)
target_include_directories(skyframe_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_compile_features(skyframe_core PUBLIC cxx_std_20)
skyframe_configure_target(skyframe_core)

if(SKYFRAME_BUILD_GAME)
    find_package(raylib CONFIG QUIET)
    if(NOT TARGET raylib AND NOT TARGET raylib::raylib)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(RAYLIB QUIET IMPORTED_TARGET raylib)
        endif()
    endif()

    if(TARGET raylib)
        set(SKYFRAME_RAYLIB_TARGET raylib)
    elseif(TARGET raylib::raylib)
        set(SKYFRAME_RAYLIB_TARGET raylib::raylib)
    elseif(TARGET PkgConfig::RAYLIB)
        set(SKYFRAME_RAYLIB_TARGET PkgConfig::RAYLIB)
    else()
        message(WARNING "raylib was not found. The core library and tests will still build. Install the raylib development package to build the desktop game.")
    endif()

    if(SKYFRAME_RAYLIB_TARGET)
        add_executable(skyframe
            src/main.cpp
            src/skyframe/render/world.cpp
            src/skyframe/render/aircraft_model.cpp
            src/skyframe/ui/hud.cpp
        )
        target_link_libraries(skyframe PRIVATE Skyframe::Core ${SKYFRAME_RAYLIB_TARGET})
        target_compile_features(skyframe PRIVATE cxx_std_20)
        skyframe_configure_target(skyframe)
    endif()
endif()

if(SKYFRAME_BUILD_TESTS)
    include(CTest)
    add_executable(skyframe_tests tests/sim_tests.cpp)
    target_link_libraries(skyframe_tests PRIVATE Skyframe::Core)
    target_compile_features(skyframe_tests PRIVATE cxx_std_20)
    skyframe_configure_target(skyframe_tests)
    add_test(NAME skyframe_simulation COMMAND skyframe_tests)
endif()
