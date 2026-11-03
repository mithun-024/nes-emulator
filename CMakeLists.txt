cmake_minimum_required(VERSION 3.16)
project(NES_Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(nes_emulator ${SOURCES})

# Add test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
if(TEST_SOURCES)
    # Exclude main.cpp from test build
    list(FILTER SOURCES EXCLUDE REGEX "src/main.cpp")
    add_executable(nes_tests ${TEST_SOURCES} ${SOURCES})
endif()
