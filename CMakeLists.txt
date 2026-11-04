cmake_minimum_required(VERSION 3.16)
project(NES_Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# SDL2 is only needed for the interactive window; tests stay headless.
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(nes_emulator ${SOURCES})
    target_link_libraries(nes_emulator PRIVATE SDL2::SDL2)
else()
    message(WARNING "SDL2 not found - nes_emulator target disabled (tests still build)")
endif()

# Add test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
if(TEST_SOURCES)
    # Exclude main.cpp from test build
    list(FILTER SOURCES EXCLUDE REGEX "src/main.cpp")
    add_executable(nes_tests ${TEST_SOURCES} ${SOURCES})
    target_compile_definitions(nes_tests PRIVATE NES_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endif()
