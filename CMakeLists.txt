cmake_minimum_required(VERSION 3.1)
project(MultiSense_Samples)

set(CMAKE_CXX_STANDARD 20)

# This will be useful for build rules in subordinate CMakeLists files.

set(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Define where the result of the build should go.

include_directories(BEFORE external/LibMultiSense/source ${BASE_DIRECTORY}/include)
add_subdirectory(external/LibMultiSense/source/LibMultiSense)
find_package(OpenCV)

add_executable(main src/main.cpp src/pcl_test.cpp)
target_link_libraries(main MultiSense ${OpenCV_LIBS})

# Inclue PCL and build examples including PCL
if(${BUILD_PCL_EXAMPLE})
    cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
    find_package(PCL REQUIRED COMPONENTS common io)
    include_directories(${PCL_INCLUDE_DIRS})
    include_directories(${PCL_INCLUDE_DIRS})
    link_directories(${PCL_LIBRARY_DIRS})
    add_definitions(${PCL_DEFINITIONS})

    add_executable(pcl_example src/main_pcl.cpp)
    target_link_libraries(pcl_example ${PCL_LIBRARIES} MultiSense ${OpenCV_LIBS})

    # Pure PCL Visualizer example
    add_executable(pcl_visualizer_example src/pcl_test.cpp)
    target_link_libraries(pcl_example ${PCL_LIBARIES} ${OpenCV_LIBS})

endif()