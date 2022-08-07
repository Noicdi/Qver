cmake_minimum_required (VERSION 3.19.0)

project(Qver)

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src project_src)

add_executable(${PROJECT_NAME} ${project_src})

set(CMAKE_CXX_COMPILER  "g++" )
set(CMAKE_CXX_FLAGS  "-Wall")
set(CMAKE_CXX_FLAGS  "-Werror")
set(CMAKE_CXX_FLAGS  "-std=c++11")
set(CMAKE_CXX_FLAGS  "-lpthread")

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/build)

# 开启 GDB 调试
#SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
#SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
