cmake_minimum_required(VERSION 3.16)
project(simple_threadpool CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# Default to an optimized Release build unless a build type is specified.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_library(threadpool src/threadpool.cpp)
target_include_directories(threadpool PUBLIC include)
target_link_libraries(threadpool PUBLIC Threads::Threads)

# demos
add_executable(main demo/main.cpp)
target_link_libraries(main threadpool)

add_executable(analyze demo/analyze.cpp)
target_link_libraries(analyze threadpool)

# timing
add_executable(speed bench/speed.cpp)
target_link_libraries(speed threadpool)

# tests
enable_testing()

add_executable(basic test/basic.cpp)
target_link_libraries(basic threadpool)
add_test(NAME basic COMMAND basic)

add_executable(queue_full test/queue_full.cpp)
target_link_libraries(queue_full threadpool)
add_test(NAME queue_full COMMAND queue_full)

add_executable(shutdown test/shutdown.cpp)
target_link_libraries(shutdown threadpool)
add_test(NAME shutdown COMMAND shutdown)
