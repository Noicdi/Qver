cmake_minimum_required(VERSION 3.19.0)

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "-std=c++11")
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-lpthread")

add_executable(test-thread-pool test-thread-pool.cpp ../src/thread-pool.cpp)

add_executable(test-socket test-socket.cpp ../src/socket.cpp ../src/epoller.cpp ../src/thread-pool.cpp ../src/timer.cpp ../src/signaler.cpp ../src/http-event.cpp)

SET(EXECUTABLE_OUTPUT_PATH /home/z/Qver)
