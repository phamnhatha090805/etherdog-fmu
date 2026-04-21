cmake_minimum_required(VERSION 3.10)
project(fmu_test)

set(CMAKE_CXX_STANDARD 17)

find_package(fmi4cpp REQUIRED)
#find_package(kickcat REQUIRED)

add_executable(etherdog-fmu main.cpp)

target_link_libraries(etherdog-fmu fmi4cpp::fmi4cpp)