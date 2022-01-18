cmake_minimum_required(VERSION 3.21)
project(MultiSense_Samples)

set(CMAKE_CXX_STANDARD 20)

# This will be useful for build rules in subordinate CMakeLists files.

set(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Define where the result of the build should go.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIRECTORY}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BASE_DIRECTORY}/lib)

include_directories(BEFORE external/LibMultiSense/source ${BASE_DIRECTORY}/include)

add_subdirectory(external/LibMultiSense/source/LibMultiSense)
find_package(OpenCV)

add_subdirectory(src)