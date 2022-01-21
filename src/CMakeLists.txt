

add_executable(main main.cpp main_pcl.cpp)
target_link_libraries(main MultiSense ${OpenCV_LIBS})

message(MSG: ${BUILD_PCL_EXAMPLE})

if(${BUILD_PCL_EXAMPLE})
    cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
    find_package(PCL 1.3 REQUIRED COMPONENTS common io)
    include_directories(${PCL_INCLUDE_DIRS})
    include_directories(${PCL_INCLUDE_DIRS})
    link_directories(${PCL_LIBRARY_DIRS})
    add_definitions(${PCL_DEFINITIONS})

    add_executable(pcl_example main_pcl.cpp)
    target_link_libraries(pcl_example ${PCL_LIBRARIES})

endif()