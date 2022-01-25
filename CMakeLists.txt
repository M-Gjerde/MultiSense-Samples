cmake_minimum_required(VERSION 3.1)
project(MultiSense_Samples)

set(CMAKE_CXX_STANDARD 20)

# This will be useful for build rules in subordinate CMakeLists files.
set(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
find_package(OpenCV)

# Define where the result of the build should go.
include_directories(BEFORE external/LibMultiSense/source ${BASE_DIRECTORY}/include)
add_subdirectory(external/LibMultiSense/source/LibMultiSense)

# Inclue PCL and build examples including PCL
if(${BUILD_PCL_EXAMPLE})
    find_package(PCL 1.2 REQUIRED)

    include_directories(${PCL_INCLUDE_DIRS})
    link_directories(${PCL_LIBRARY_DIRS})
    add_definitions(${PCL_DEFINITIONS})

    add_executable(TEST src/pcl_test.cpp)
    target_link_libraries (TEST ${PCL_LIBRARIES} /usr/lib/x86_64-linux-gnu/libpcl_io.so)

    install(TARGETS TEST RUNTIME DESTINATION bin)

    # Pure PCL Visualizer example
    # add_executable(pcl_visualizer_example src/pcl_test.cpp)
    # target_link_libraries(pcl_example ${PCL_LIBARIES} ${OpenCV_LIBS})

    add_executable(TEST2 src/simple_viewer.cpp)
    target_link_libraries (TEST2 ${OpenCV_LIBS} ${PCL_LIBRARIES} MultiSense )
endif()



add_executable(main src/main.cpp)
target_link_libraries(main MultiSense ${OpenCV_LIBS})
